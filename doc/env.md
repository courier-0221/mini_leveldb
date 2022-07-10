# 简介

考虑到移植以及灵活性，LevelDB将系统相关的处理（文件/进程/时间）抽象成Evn，用户可以自己实现相应的接口，作为option的一部分传入，默认使用自带的实现。 
env.h中声明了:

1.虚基类env，在env_posix.cc中，派生类PosixEnv继承自env类，是LevelDB的默认实现。
2.虚基类WritableFile、SequentialFile、RandomAccessFile，分别是文件的写抽象类，顺序读抽象类和随机读抽象类，env_posix.cc中都给了相应实现。
3.类Logger，log文件的写入接口，log文件是防止系统异常终止造成数据丢失，是memtable在磁盘的备份
4.类FileLock，为文件上锁
5.WriteStringToFile、ReadFileToString、Log三个全局函数，封装了上述接口

# SequentialFile
Read方法使用linux下的read系统调用。Skip方法使用linux下的lseek系统调用。
```cpp
// 用于顺序读取文件的文件抽象类
class PosixSequentialFile final : public SequentialFile {
 public:
  PosixSequentialFile(std::string filename, int fd)
      : fd_(fd), filename_(std::move(filename)) {}
  ~PosixSequentialFile() override { close(fd_); }
  // 从文件中读取n个字节存放到 "scratch[0..n-1]"， 然后将"scratch[0..n-1]"转化为Slice类型并存放到*result中
  // 如果正确读取，则返回OK status，否则返回non-OK status
  Status Read(size_t n, Slice* result, char* scratch) override {
    Status status;
    while (true) {
      ::ssize_t read_size = ::read(fd_, scratch, n);
      if (read_size < 0) {  // Read error.
        if (errno == EINTR) {
          continue;  // Retry
        }
        status = PosixError(filename_, errno);
        break;
      }
      *result = Slice(scratch, read_size);
      break;
    }
    return status;
  }
  // 跳过n字节的内容，这并不比读取n字节的内容慢，而且会更快。
  // 如果到达了文件尾部，则会停留在文件尾部，并返回OK Status。
  // 否则，返回错误信息
  Status Skip(uint64_t n) override {
    if (::lseek(fd_, n, SEEK_CUR) == static_cast<off_t>(-1)) {
      return PosixError(filename_, errno);
    }
    return Status::OK();
  }

 private:
  const int fd_;
  const std::string filename_;
};
```

# RandomAccessFile
env_posix.c 总定义了两种 random-access 文件，分别是 PosixRandomAccessFile 和 PosixMmapReadableFile。
## PosixRandomAccessFile随机访问的实现：
PosixRandomAccessFile 使用了 pread 来实现原子的定位加访问功能。常规的随机访问文件的过程可以分为两步，fseek (seek) 定位到访问点，调用 fread (read) 来从特定位置开始访问 FILE* (fd)。然而，这两个操作组合在一起并不是原子的，即 fseek 和 fread 之间可能会插入其他线程的文件操作。相比之下 pread 由系统来保证实现原子的定位和读取组合功能。需要注意的是，pread 操作不会更新文件指针。
```cpp
class PosixRandomAccessFile final : public RandomAccessFile {
virtual Status Read(uint64_t offset, size_t n, Slice* result,
                    char* scratch) const {
    Status s;
    ssize_t r = pread(fd_, scratch, n, static_cast<off_t>(offset));
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
        // An error: return a non-ok status
        s = IOError(filename_, errno);
    }
    return s;
}
};
```

## PosixMmapReadableFile随机访问的实现：
PosixMmapReadableFile 使用了内存映射文件来实现对于文件的随机访问，在初始化该 class 时提供一块已经映射好的内存 mmapped_region 即可，之后的随机访问将如同操作内存中的字节数组一样简单.
```cpp
class PosixMmapReadableFile final : public RandomAccessFile {
virtual Status Read(uint64_t offset, size_t n, Slice* result, 
                    char* scratch) const {
    Status s;
    if (offset + n > length_) {
        *result = Slice();
        s = IOError(filename_, EINVAL);
    } else {
        *result = Slice(reinterpret_cast<char*>(mmapped_region_)
                        + offset, n);  //访问内存空间
    }
    return s;
}
};
```
## Limiter
帮助类来限制资源使用以避免耗尽。当前用于限制只读文件描述符和 mmap 文件的使用，这样我们就不会耗尽文件描述符或虚拟内存，或者遇到非常大的数据库的内核性能问题。
```cpp
class Limiter {
 public:
  // 将最大资源数限制为 |max_acquires|。
  Limiter(int max_acquires)
      : max_acquires_(max_acquires),
        acquires_allowed_(max_acquires) {
    assert(max_acquires >= 0);
  }

  // 如果另一个资源可用，获取它并返回 true。否则返回假。
  bool Acquire() ;
  // 释放由先前调用 Acquire() 获取的资源，返回 true。
  void Release();

 private:
  // 捕获过多的 Release() 调用。
  const int max_acquires_;
  // 可用资源的数量。
  std::atomic<int> acquires_allowed_;
};
```

## 使用
1.每次 env_posix 中调用 NewRandomAccessFile 时，其会首先调用 mmap_limit_.Acquire() 来测试当前的系统是否还允许生成 PosixMmapReadableFile。
2.如果返回 true，那么调用 mmap 系统调用来将文件映射到一块连续的虚拟内存上。关于 mmap 系统调用对于虚拟内存的使用，可以看 mmap memory。
3.如果 mmap 调用失败，或者 mmap_limit_.Acquire() 返回 false，那么系统将返回一个 PosixRandomAccessFile 用来实现对文件的随机访问。
```cpp
Status NewRandomAccessFile(const std::string& filename,
                             RandomAccessFile** result) override {
    *result = nullptr;
    int fd = ::open(filename.c_str(), O_RDONLY | kOpenBaseFlags);
    if (fd < 0) {
      return PosixError(filename, errno);
    }

    if (!mmap_limiter_.Acquire()) {
      *result = new PosixRandomAccessFile(filename, fd, &fd_limiter_);
      return Status::OK();
    }

    uint64_t file_size;
    Status status = GetFileSize(filename, &file_size);
    if (status.ok()) {
      void* mmap_base =
          ::mmap(/*addr=*/nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
      if (mmap_base != MAP_FAILED) {
        *result = new PosixMmapReadableFile(filename,
                                            reinterpret_cast<char*>(mmap_base),
                                            file_size, &mmap_limiter_);
      } else {
        status = PosixError(filename, errno);
      }
    }
    ::close(fd);
    if (!status.ok()) {
      mmap_limiter_.Release();
    }
    return status;
  }
```

# WritableFile
用于顺序写入的文件抽象类，实现必须提供缓冲，因为调用者可能一次将小片段附加到文件中。
```cpp
class PosixWritableFile final : public WritableFile {
public:
  // 先尝试存入到自己的buf中
  virtual Status Append(const Slice& data) override;
  // 关闭文件
  virtual Status Close() override;
  // 调用write接口将自己buf中的数据写入到文件中
  virtual Status Flush() override;
  // 同步缓存和磁盘中的数据，使用fsync
  virtual Status Sync() override;
private:
  // buf_[0, pos_ - 1] contains data to be written to fd_.
  char buf_[kWritableFileBufferSize];
  size_t pos_;
  int fd_;
};
```
这里介绍下sync和fsync系统调用
1.缓冲
传统的UNIX实现的内核中都设置有缓冲区或者页面高速缓存，大多数磁盘IO都是通过缓冲写的。
当你想将数据write进文件时，内核通常会将该数据复制到其中一个缓冲区中，如果该缓冲没被写满的话，内核就不会把它放入到输出队列中。
当这个缓冲区被写满或者内核想重用这个缓冲区时，才会将其排到输出队列中。等它到达等待队列首部时才会进行实际的IO操作。
这里的输出方式就是大家耳熟能详的： 延迟写
这个缓冲区就是大家耳熟能详的：OS Cache
2.延迟写的优缺点#
很明显、延迟写降低了磁盘读写的次数，但同时也降低了文件的更新速度。
这样当OS Crash时由于这种延迟写的机制可能会造成文件更新内容的丢失。而为了保证磁盘上的实际文件和缓冲区中的内容保持一致，UNIX系统提供了三个系统调用：sync、fsync、fdatasync
3.系统调用
```cpp
#include<unistd.h>
int fsync(int filedes);
int fdatasync(int filedes);
int sync();
```
sync系统调用：将所有修改过的缓冲区排入写队列，然后就返回了，它并不等实际的写磁盘的操作结束。所以它的返回并不能保证数据的安全性。通常会有一个update系统守护进程每隔30s调用一次sync。命令sync(1)也是调用sync函数。
fsync系统调用：需要你在入参的位置上传递给他一个fd，然后系统调用就会对这个fd指向的文件起作用。fsync会确保一直到写磁盘操作结束才会返回。所以fsync适合数据库这种程序。
fdatasync系统调用：和fsync类似但是它只会影响文件的一部分，因为除了文件中的数据之外，fsync还会同步文件的属性。

# 文件总结
定义完这三个文件的读写抽象类，把他们加入到PosixEnv类中，定义三个NewSequentialFile、NewRandomAccessFile、NewWritableFile函数，产生文件读写的对象，在程序上层中调用env_->NewWritableFile即可创建一个文件，并可写入数据。

# FileLock
env_posix 中定义了一个用来对文件进行加锁和解锁的函数 LockFile 和 UnlockFile。
使用fcntl对文件加锁，加锁效果类似于自旋锁，只有写写互斥和读写互斥，读读并不互斥
这里还涉及两个类 PosixFileLock 和 PosixLockTable

```cpp
Status LockFile(const std::string& filename, FileLock** lock) override {
    // 打开文件，并且根据文件名检测当前的文件是否已经被加锁。事实上，所有的加过锁的文件都会被保存到一个列表 PosixLockTable 中。
    // 如果当前的文件没有被加锁，那么调用函数 LockOrUnlock 来进行文件加锁。
}

Status UnlockFile(FileLock* lock) override {
    // 解锁
    // 从 PosixLockTable 中删除
}

int LockOrUnlock(int fd, bool lock) {
  errno = 0;
  struct ::flock file_lock_info;
  std::memset(&file_lock_info, 0, sizeof(file_lock_info));
  // 调用了 posix 函数 fcntl 来完成对文件的加锁和解锁。
  file_lock_info.l_type = (lock ? F_WRLCK : F_UNLCK);
  file_lock_info.l_whence = SEEK_SET;
  file_lock_info.l_start = 0;
  file_lock_info.l_len = 0;  // Lock/unlock entire file.
  return ::fcntl(fd, F_SETLK, &file_lock_info); // F_SETLK : 给当前文件上锁（非阻塞）
}
```


# Logger

首先要区分LOG文件和.log文件。
LOG文件：用来记录数据库打印的运行日志信息，方便bug的查找。
.log文件：在LevelDB中的主要作用是系统故障恢复时，能够保证不会丢失数据。因为在将记录写入内存的Memtable之前，会先写入.log文件，这样即使系统发生故障，Memtable中的数据没有来得及Dump到磁盘的SSTable文件，LevelDB也可以根据.log文件恢复内存的Memtable数据结构内容，不会造成系统丢失数据。
Env.h中定义了操作LOG文件的虚基类Logger，只提供了一个对外的接口Logv。Logger的Linux版本实现是PosixLogger。

```cpp
class PosixLogger final : public Logger {
 public:
  explicit PosixLogger(std::FILE* fp) : fp_(fp) { assert(fp != nullptr); }
  ~PosixLogger() override { std::fclose(fp_); }
  void Logv(const char* format, std::va_list arguments) override;
 private:
  std::FILE* const fp_;
};
```

核心接口Logv:

```cpp
void Logv(const char* format, std::va_list arguments) override {
	// 记录时间
    // 记录线程id
    // 然后将时间和线程id (logheader) 添加到buffer中等待写入LOG文件

    // 日志记录会尝试两次内存分配：第一次分配固定大小的栈内存,如果不够，第二次分配更大的堆内存
    for (int iteration = 0; iteration < 2; ++iteration) {
    	// 第一次从栈上申请512字节大小的空间用于存放用户需要记录的内容，这512也包括logheader，
        // 如果栈空间不足才会触发第二次堆空间的内存分配
    	// 如果第二次分的内存还不够，只能对存放内容做截断处理了。
    }
}
```
简单来说逻辑就是，如果用户需要记录的内容<512字节，从栈空间上申请空间操作；如果>512字节从堆空间上申请全部内存来记录全部的内容，第一次写入栈空间的512字节不再需要。

# 计划任务
PosixEnv还有一个很重要的功能，计划任务，也就是后台的compaction线程。compaction就是压缩合并的意思。对于LevelDB来说，写入记录操作很简单，删除记录仅仅写入一个删除标记就算完事，但是读取记录比较复杂，需要在内存以及各个层级文件中依照新鲜程度依次查找，代价很高。为了加快读取速度，LevelDB采取了compaction的方式来对已有的记录进行整理压缩，通过这种方式，来删除掉一些不再有效的KV数据，减小数据规模，减少文件数量等。
核心采用的是线程生产者消费者模型，使用互斥锁+条件变量来实现.
```cpp
class PosixEnv : public Env {
public:
  // 生产端 将任务放入队列中
  void Schedule(void (*background_work_function)(void* background_work_arg),
                void* background_work_arg) override;
private:
  // 在 Schedule() 调用中存储工作项数据。
  struct BackgroundWorkItem {
    explicit BackgroundWorkItem(void (*function)(void* arg), void* arg)
        : function(function), arg(arg) {}

    void (*const function)(void*);
    void* const arg;
  };

  // 消费端
  void BackgroundThreadMain();

  Mutex background_work_mutex_;
  CondVar background_work_cv_;
  bool started_background_thread_;
}
```




# EnvWrapper
该类继承自Env，采用代理模式来实现，传入PosixEnv，实际上调用EnvWrapper接口时调用的是PosixEnv里面的内容。

