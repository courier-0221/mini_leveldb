#include <limits>
#include <string>

#include "slice.h"
#include "logging.h"
#include "env.h"
#include "posix_logger.h"
#include "env_posix_test_helper.h"

using namespace leveldb;

static const int kReadOnlyFileLimit = 4;
static const int kMMapLimit = 4;

namespace leveldb {

class EnvPosixTest {
 public:
  static void SetFileLimits(int read_only_file_limit, int mmap_limit) {
    EnvPosixTestHelper::SetReadOnlyFDLimit(read_only_file_limit);
    EnvPosixTestHelper::SetReadOnlyMMapLimit(mmap_limit);
  }

  EnvPosixTest() : env_(Env::Default()) {}

  Env* env_;
};

}

int main(void) {
  EnvPosixTest::SetFileLimits(kReadOnlyFileLimit, kMMapLimit);

  EnvPosixTest envTest;
  
  Logger* file = nullptr;
  envTest.env_->NewLogger("./LoggerTest.txt", &file);

  Log(file, "%s\n", "<<Waiting for Love>>");
  Log(file, "%s\n", "Monday left me broken");
  Log(file, "%s\n", "Tuesday I was through with hoping");
  Log(file, "%s\n", "Wednesday my empty arms were open");
  Log(file, "%s\n", "Thursday waiting for love, waiting for love");
  Log(file, "%s\n", "Thank the stars it's Friday");
  Log(file, "%s\n", "I'm burning like a fire gone wild on Saturday");
  Log(file, "%s\n", "Guess I won't be coming to church on Sunday");
  Log(file, "%s\n", "I'll be waiting for love, waiting for love");
#if 0
  std::string str(1500, 'a');
  Log(file, "%s\n", str.c_str());
#endif
  delete file;

  return 0;
}