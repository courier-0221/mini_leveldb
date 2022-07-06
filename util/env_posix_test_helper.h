#ifndef STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_
#define STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_

namespace leveldb {

class EnvPosixTest;

// A helper for the POSIX Env to facilitate testing.
class EnvPosixTestHelper {
 private:
  friend class EnvPosixTest;

  // 设置将打开的只读文件的最大数量。
  // Must be called before creating an Env.
  static void SetReadOnlyFDLimit(int limit);

  // 设置将通过 mmap 映射的只读文件的最大数量。
  // Must be called before creating an Env.
  static void SetReadOnlyMMapLimit(int limit);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_
