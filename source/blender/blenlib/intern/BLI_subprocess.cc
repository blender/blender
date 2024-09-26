/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_subprocess.hh"

#if BLI_SUBPROCESS_SUPPORT

/* Based on https://github.com/jarikomppa/ipc (Unlicense) */

#  include "BLI_assert.h"
#  include "BLI_path_utils.hh"
#  include "BLI_string_utf8.h"
#  include <iostream>

namespace blender {

static bool check_arguments_are_valid(Span<StringRefNull> args)
{
  for (StringRefNull arg : args) {
    for (const char c : arg) {
      if (!std::isalnum(c) && !ELEM(c, '_', '-')) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace blender

#  ifdef _WIN32

#    define WIN32_LEAN_AND_MEAN
#    include <comdef.h>
#    include <windows.h>

namespace blender {

static void print_last_error(const char *function, const char *msg)
{
  DWORD error_code = GetLastError();
  std::cerr << "ERROR (" << error_code << "): " << function << " : " << msg << std::endl;
}

static void check(bool result, const char *function, const char *msg)
{
  if (!result) {
    print_last_error(function, msg);
    BLI_assert(false);
  }
}

#    define CHECK(result) check((result), __func__, #result)
#    undef ERROR /* Defined in wingdi.h */
#    define ERROR(msg) check(false, __func__, msg)

/* Owning process group that will close subprocesses assigned to it when the instance is destructed
 * or the creator process ends. */
class ProcessGroup {
 private:
  HANDLE handle_;

 public:
  ProcessGroup()
  {
    handle_ = CreateJobObject(nullptr, nullptr);
    CHECK(handle_);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    CHECK(
        SetInformationJobObject(handle_, JobObjectExtendedLimitInformation, &info, sizeof(info)));
  }

  ~ProcessGroup()
  {
    CHECK(CloseHandle(handle_));
  }

  void assign_subprocess(HANDLE subprocess)
  {
    CHECK(AssignProcessToJobObject(handle_, subprocess));
  }
};

bool BlenderSubprocess::create(Span<StringRefNull> args)
{
  BLI_assert(handle_ == nullptr);

  if (!check_arguments_are_valid(args)) {
    BLI_assert(false);
    return false;
  }

  wchar_t path[FILE_MAX];
  if (!GetModuleFileNameW(nullptr, path, FILE_MAX)) {
    ERROR("GetModuleFileNameW");
    return false;
  }

  std::string args_str;
  for (StringRefNull arg : args) {
    args_str += arg + " ";
  }

  const int length_wc = MultiByteToWideChar(
      CP_UTF8, 0, args_str.c_str(), args_str.length(), nullptr, 0);
  std::wstring w_args(length_wc, 0);
  CHECK(MultiByteToWideChar(
      CP_UTF8, 0, args_str.c_str(), args_str.length(), w_args.data(), length_wc));

  STARTUPINFOW startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info = {0};
  if (!CreateProcessW(path,
                      /** Use data() since lpCommandLine must be mutable. */
                      w_args.data(),
                      nullptr,
                      nullptr,
                      false,
                      CREATE_BREAKAWAY_FROM_JOB,
                      nullptr,
                      nullptr,
                      &startup_info,
                      &process_info))
  {
    ERROR("CreateProcessW");
    return false;
  }

  handle_ = process_info.hProcess;
  CHECK(CloseHandle(process_info.hThread));

  static ProcessGroup group;
  /* Don't let the subprocess outlive its parent. */
  group.assign_subprocess(handle_);

  return true;
}

BlenderSubprocess::~BlenderSubprocess()
{
  if (handle_) {
    CHECK(CloseHandle(handle_));
  }
}

bool BlenderSubprocess::is_running()
{
  if (!handle_) {
    return false;
  }

  DWORD exit_code = 0;
  if (GetExitCodeProcess(handle_, &exit_code)) {
    return exit_code == STILL_ACTIVE;
  }

  ERROR("GetExitCodeProcess");
  /* Assume the process is still running. */
  return true;
}

SharedMemory::SharedMemory(std::string name, size_t size, bool is_owner)
    : name_(name), is_owner_(is_owner)
{
  if (is_owner) {
    handle_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name.c_str());
    CHECK(handle_ /*Create*/);
  }
  else {
    handle_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    CHECK(handle_ /*Open*/);
  }

  if (handle_) {
    data_ = MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CHECK(data_);
  }
  else {
    data_ = nullptr;
  }

  data_size_ = data_ ? size : 0;
}

SharedMemory::~SharedMemory()
{
  if (data_) {
    CHECK(UnmapViewOfFile(data_));
  }
  if (handle_) {
    CHECK(CloseHandle(handle_));
  }
}

SharedSemaphore::SharedSemaphore(std::string name, bool is_owner)
    : name_(name), is_owner_(is_owner)
{
  handle_ = CreateSemaphoreA(nullptr, 0, 1, name.c_str());
  CHECK(handle_);
}

SharedSemaphore::~SharedSemaphore()
{
  if (handle_) {
    CHECK(CloseHandle(handle_));
  }
}

void SharedSemaphore::increment()
{
  CHECK(ReleaseSemaphore(handle_, 1, nullptr));
}

void SharedSemaphore::decrement()
{
  CHECK(WaitForSingleObject(handle_, INFINITE) != WAIT_FAILED);
}

bool SharedSemaphore::try_decrement(int wait_ms)
{
  DWORD result = WaitForSingleObject(handle_, wait_ms);
  CHECK(result != WAIT_FAILED);
  return result == WAIT_OBJECT_0;
}

}  // namespace blender

#  elif defined(__linux__)

#    include "BLI_time.h"
#    include "BLI_vector.hh"
#    include <fcntl.h>
#    include <linux/limits.h>
#    include <stdlib.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#    include <wait.h>

namespace blender {

static void print_last_error(const char *function, const char *msg)
{
  int error_code = errno;
  std::string error_msg = "ERROR (" + std::to_string(error_code) + "): " + function + " : " + msg;
  perror(error_msg.c_str());
}

static void check(int result, const char *function, const char *msg)
{
  if (result == -1) {
    print_last_error(function, msg);
    BLI_assert(false);
  }
}

#    define CHECK(result) check((result), __func__, #result)
#    define ERROR(msg) check(-1, __func__, msg)

bool BlenderSubprocess::create(Span<StringRefNull> args)
{
  if (!check_arguments_are_valid(args)) {
    BLI_assert(false);
    return false;
  }

  char path[PATH_MAX + 1];
  const size_t len = readlink("/proc/self/exe", path, PATH_MAX);
  if (len == size_t(-1)) {
    ERROR("readlink");
    return false;
  }
  /* readlink doesn't append a null terminator. */
  path[len] = '\0';

  Vector<char *> char_args;
  for (StringRefNull arg : args) {
    char_args.append((char *)arg.data());
  }
  char_args.append(nullptr);

  pid_ = fork();

  if (pid_ == -1) {
    ERROR("fork");
    return false;
  }
  else if (pid_ > 0) {
    return true;
  }

  /* Child process initialization. */
  execv(path, char_args.data());

  /* This should only be reached if `execvp` fails and stack isn't replaced. */
  ERROR("execv");

  /* Ensure outputs are flushed as `_exit` doesn't flush. */
  fflush(stdout);
  fflush(stderr);

  /* Use `_exit` instead of `exit` so Blender's `atexit` cleanup functions don't run. */
  _exit(errno);
  BLI_assert_unreachable();
  return false;
}

BlenderSubprocess::~BlenderSubprocess() {}

bool BlenderSubprocess::is_running()
{
  if (pid_ == -1) {
    return false;
  }

  pid_t result = waitpid(pid_, nullptr, WNOHANG);
  CHECK(result);

  if (result == pid_) {
    pid_ = -1;
    return false;
  }

  return true;
}

SharedMemory::SharedMemory(std::string name, size_t size, bool is_owner)
    : name_(name), is_owner_(is_owner)
{
  constexpr mode_t user_mode = S_IRUSR | S_IWUSR;
  if (is_owner) {
    handle_ = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, user_mode);
    CHECK(handle_);
    if (handle_ != -1) {
      if (ftruncate(handle_, size) == -1) {
        ERROR("ftruncate");
        CHECK(close(handle_));
        handle_ = -1;
      }
    }
  }
  else {
    handle_ = shm_open(name.c_str(), O_RDWR, user_mode);
    CHECK(handle_);
  }

  if (handle_ != -1) {
    data_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle_, 0);
    if (data_ == MAP_FAILED) {
      ERROR("mmap");
      data_ = nullptr;
    }
    /* File descriptor can close after mmap. */
    CHECK(close(handle_));
  }
  else {
    data_ = nullptr;
  }

  data_size_ = data_ ? size : 0;
}

SharedMemory::~SharedMemory()
{
  if (data_) {
    CHECK(munmap(data_, data_size_));
    if (is_owner_) {
      CHECK(shm_unlink(name_.c_str()));
    }
  }
}

SharedSemaphore::SharedSemaphore(std::string name, bool is_owner)
    : name_(name), is_owner_(is_owner)
{
  constexpr mode_t user_mode = S_IRUSR | S_IWUSR;
  handle_ = sem_open(name.c_str(), O_CREAT, user_mode, 0);
  if (!handle_) {
    ERROR("sem_open");
  }
}

SharedSemaphore::~SharedSemaphore()
{
  if (handle_) {
    CHECK(sem_close(handle_));
    if (is_owner_) {
      CHECK(sem_unlink(name_.c_str()));
    }
  }
}

void SharedSemaphore::increment()
{
  CHECK(sem_post(handle_));
}

void SharedSemaphore::decrement()
{
  while (true) {
    int result = sem_wait(handle_);
    if (result == 0) {
      return;
    }
    else if (errno != EINTR) {
      ERROR("sem_wait");
      return;
    }
    /* Try again if interrupted by handler. */
  }
}

bool SharedSemaphore::try_decrement(int wait_ms)
{
  if (wait_ms == 0) {
    int result = sem_trywait(handle_);
    if (result == 0) {
      return true;
    }
    else if (errno == EINVAL) {
      ERROR("sem_trywait");
    }
    return false;
  }

  timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) == -1) {
    ERROR("clock_gettime");
    BLI_time_sleep_ms(wait_ms);
    return try_decrement(0);
  }

  time.tv_sec += wait_ms / 1000;
  time.tv_nsec += (wait_ms % 1000) * 10e6;

  while (true) {
    int result = sem_timedwait(handle_, &time);
    if (result == 0) {
      return true;
    }
    else if (errno != EINTR) {
      if (errno != ETIMEDOUT) {
        ERROR("sem_timedwait");
      }
      return false;
    }
    /* Try again if interrupted by handler. */
  }
}

}  // namespace blender

#  endif

#endif
