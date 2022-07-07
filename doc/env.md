# 简介

考虑到移植以及灵活性，LevelDB将系统相关的处理（文件/进程/时间）抽象成Evn，用户可以自己实现相应的接口，作为option的一部分传入，默认使用自带的实现。 
env.h中声明了:

1.虚基类env，在env_posix.cc中，派生类PosixEnv继承自env类，是LevelDB的默认实现。
2.虚基类WritableFile、SequentialFile、RandomAccessFile，分别是文件的写抽象类，顺序读抽象类和随机读抽象类，env_posix.cc中都给了相应实现。
3.类Logger，log文件的写入接口，log文件是防止系统异常终止造成数据丢失，是memtable在磁盘的备份
4.类FileLock，为文件上锁
5.WriteStringToFile、ReadFileToString、Log三个全局函数，封装了上述接口





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

