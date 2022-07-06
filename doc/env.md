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



