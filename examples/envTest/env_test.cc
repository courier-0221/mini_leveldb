#include "env.h"
#include <algorithm>
#include "gtest/gtest.h"
#include "mutex.h"
#include "random.h"

namespace leveldb {

class EnvTest : public testing::Test {
 public:
  EnvTest() : env_(Env::Default()) {}

  Env* env_;
};

TEST_F(EnvTest, ReadWrite) {
  Random rnd(128);

  // Get file to use for testing.
  std::string test_dir;
  env_->GetTestDirectory(&test_dir);
  std::string test_file_name = test_dir + "/open_on_read.txt";
  WritableFile* writable_file;
  env_->NewWritableFile(test_file_name, &writable_file);

  // Fill a file with data generated via a sequence of randomly sized writes.
  static const size_t kDataSize = 10 * 1048576;
  std::string data;
  while (data.size() < kDataSize) {
    int len = rnd.Skewed(18);  // Up to 2^18 - 1, but typically much smaller
    std::string r;
    r.resize(len);
    for (int i = 0; i < len; i++) {
        r[i] = static_cast<char>(' ' + rnd.Uniform(95));  // ' ' .. '~'
    }
    writable_file->Append(r);
    data += r;
    if (rnd.OneIn(10)) {
      writable_file->Flush();
    }
  }
  writable_file->Sync();
  writable_file->Close();
  delete writable_file;

  // Read all data using a sequence of randomly sized reads.
  SequentialFile* sequential_file;
  env_->NewSequentialFile(test_file_name, &sequential_file);
  std::string read_result;
  std::string scratch;
  while (read_result.size() < data.size()) {
    int len = std::min<int>(rnd.Skewed(18), data.size() - read_result.size());
    scratch.resize(std::max(len, 1));  // at least 1 so &scratch[0] is legal
    Slice read;
    sequential_file->Read(len, &read, &scratch[0]);
    if (len > 0) {
      ASSERT_GT(read.size(), 0);
    }
    ASSERT_LE(read.size(), len);
    read_result.append(read.data(), read.size());
  }
  ASSERT_EQ(read_result, data);
  delete sequential_file;
}

TEST_F(EnvTest, RunImmediately) {
  struct RunState {
    Mutex mu;
    CondVar cvar{&mu};
    bool called = false;

    static void Run(void* arg) {
      RunState* state = reinterpret_cast<RunState*>(arg);
      MutexLock l(&state->mu);
      ASSERT_EQ(state->called, false);
      state->called = true;
      state->cvar.Signal();
    }
  };

  RunState state;
  env_->Schedule(&RunState::Run, &state);

  MutexLock l(&state.mu);
  while (!state.called) {
    state.cvar.Wait();
  }
}

TEST_F(EnvTest, RunMany) {
  struct RunState {
    Mutex mu;
    CondVar cvar{&mu};
    int last_id = 0;
  };

  struct Callback {
    RunState* state_;  // Pointer to shared state.
    const int id_;  // Order# for the execution of this callback.

    Callback(RunState* s, int id) : state_(s), id_(id) {}

    static void Run(void* arg) {
      Callback* callback = reinterpret_cast<Callback*>(arg);
      RunState* state = callback->state_;

      MutexLock l(&state->mu);
      ASSERT_EQ(state->last_id, callback->id_ - 1);
      state->last_id = callback->id_;
      state->cvar.Signal();
    }
  };

  RunState state;
  Callback callback1(&state, 1);
  Callback callback2(&state, 2);
  Callback callback3(&state, 3);
  Callback callback4(&state, 4);
  env_->Schedule(&Callback::Run, &callback1);
  env_->Schedule(&Callback::Run, &callback2);
  env_->Schedule(&Callback::Run, &callback3);
  env_->Schedule(&Callback::Run, &callback4);

  MutexLock l(&state.mu);
  while (state.last_id != 4) {
    state.cvar.Wait();
  }
}

struct State {
  Mutex mu;
  CondVar cvar{&mu};

  int val ;
  int num_running;

  State(int val, int num_running) : val(val), num_running(num_running) {}
};

static void ThreadBody(void* arg) {
  State* s = reinterpret_cast<State*>(arg);
  s->mu.Lock();
  s->val += 1;
  s->num_running -= 1;
  s->cvar.Signal();
  s->mu.Unlock();
}

TEST_F(EnvTest, StartThread) {
  State state(0, 3);
  for (int i = 0; i < 3; i++) {
    env_->StartThread(&ThreadBody, &state);
  }

  MutexLock l(&state.mu);
  while (state.num_running != 0) {
    state.cvar.Wait();
  }
  ASSERT_EQ(state.val, 3);
}

TEST_F(EnvTest, TestOpenNonExistentFile) {
  // Write some test data to a single file that will be opened |n| times.
  std::string test_dir;
  env_->GetTestDirectory(&test_dir);

  std::string non_existent_file = test_dir + "/non_existent_file";
  ASSERT_TRUE(!env_->FileExists(non_existent_file));

  RandomAccessFile* random_access_file;
  Status status =
      env_->NewRandomAccessFile(non_existent_file, &random_access_file);
  ASSERT_TRUE(status.IsNotFound());

  SequentialFile* sequential_file;
  status = env_->NewSequentialFile(non_existent_file, &sequential_file);
  ASSERT_TRUE(status.IsNotFound());
}

TEST_F(EnvTest, ReopenWritableFile) {
  std::string test_dir;
  env_->GetTestDirectory(&test_dir);
  std::string test_file_name = test_dir + "/reopen_writable_file.txt";
  env_->RemoveFile(test_file_name);

  WritableFile* writable_file;
  env_->NewWritableFile(test_file_name, &writable_file);
  std::string data("hello world!");
  writable_file->Append(data);
  writable_file->Close();
  delete writable_file;

  env_->NewWritableFile(test_file_name, &writable_file);
  data = "42";
  writable_file->Append(data);
  writable_file->Close();
  delete writable_file;

  ReadFileToString(env_, test_file_name, &data);
  ASSERT_EQ(std::string("42"), data);
  env_->RemoveFile(test_file_name);
}

TEST_F(EnvTest, ReopenAppendableFile) {
  std::string test_dir;
  env_->GetTestDirectory(&test_dir);
  std::string test_file_name = test_dir + "/reopen_appendable_file.txt";
  env_->RemoveFile(test_file_name);

  WritableFile* appendable_file;
  env_->NewAppendableFile(test_file_name, &appendable_file);
  std::string data("hello world!");
  appendable_file->Append(data);
  appendable_file->Close();
  delete appendable_file;

  env_->NewAppendableFile(test_file_name, &appendable_file);
  data = "42";
  appendable_file->Append(data);
  appendable_file->Close();
  delete appendable_file;

  ReadFileToString(env_, test_file_name, &data);
  ASSERT_EQ(std::string("hello world!42"), data);
  env_->RemoveFile(test_file_name);
}

}  // namespace leveldb
