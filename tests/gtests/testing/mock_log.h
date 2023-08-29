/* SPDX-FileCopyrightText: 2007 Google Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause */

/** \file
 * Defines the #ScopedMockLog class (using Google C++ Mocking Framework),
 * which is convenient for testing code that uses LOG().
 *
 * NOTE(@keir): This is a fork until Google Log exports the scoped mock log class;
 * see: http://code.google.com/p/google-glog/issues/detail?id=88
 *
 * Author: Zhanyong Wan
 */

#ifndef GOOGLE_CERES_INTERNAL_MOCK_LOG_H_
#define GOOGLE_CERES_INTERNAL_MOCK_LOG_H_

#include <string>

#include "glog/logging.h"
#include "gmock/gmock.h"

// Needed to make the scoped mock log tests work without modification.
namespace ceres {
namespace internal {
using google::WARNING;
}  // namespace internal
}  // namespace ceres

namespace testing {

// A ScopedMockLog object intercepts LOG() messages issued during its
// lifespan.  Using this together with Google C++ Mocking Framework,
// it's very easy to test how a piece of code calls LOG().  The
// typical usage:
//
//   TEST(FooTest, LogsCorrectly) {
//     ScopedMockLog log;
//
//     // We expect the WARNING "Something bad!" exactly twice.
//     EXPECT_CALL(log, Log(WARNING, _, "Something bad!"))
//         .Times(2);
//
//     // We allow foo.cc to call LOG(INFO) any number of times.
//     EXPECT_CALL(log, Log(INFO, HasSubstr("/foo.cc"), _))
//         .Times(AnyNumber());
//
//     Foo();  // Exercises the code under test.
//   }
class ScopedMockLog : public google::LogSink {
 public:
  // When a ScopedMockLog object is constructed, it starts to
  // intercept logs.
  ScopedMockLog()
  {
    AddLogSink(this);
  }

  // When the object is destructed, it stops intercepting logs.
  virtual ~ScopedMockLog()
  {
    RemoveLogSink(this);
  }

  // Implements the mock method:
  //
  //   void Log(LogSeverity severity, const string& file_path,
  //            const string& message);
  //
  // The second argument to Send() is the full path of the source file
  // in which the LOG() was issued.
  //
  // Note, that in a multi-threaded environment, all LOG() messages from a
  // single thread will be handled in sequence, but that cannot be guaranteed
  // for messages from different threads. In fact, if the same or multiple
  // expectations are matched on two threads concurrently, their actions will
  // be executed concurrently as well and may interleave.
  MOCK_METHOD3(Log,
               void(google::LogSeverity severity,
                    const std::string &file_path,
                    const std::string &message));

 private:
  // Implements the send() virtual function in class LogSink.
  // Whenever a LOG() statement is executed, this function will be
  // invoked with information presented in the LOG().
  //
  // The method argument list is long and carries much information a
  // test usually doesn't care about, so we trim the list before
  // forwarding the call to Log(), which is much easier to use in
  // tests.
  //
  // We still cannot call Log() directly, as it may invoke other LOG()
  // messages, either due to Invoke, or due to an error logged in
  // Google C++ Mocking Framework code, which would trigger a deadlock
  // since a lock is held during send().
  //
  // Hence, we save the message for WaitTillSent() which will be called after
  // the lock on send() is released, and we'll call Log() inside
  // WaitTillSent(). Since while a single send() call may be running at a
  // time, multiple WaitTillSent() calls (along with the one send() call) may
  // be running simultaneously, we ensure thread-safety of the exchange between
  // send() and WaitTillSent(), and that for each message, LOG(), send(),
  // WaitTillSent() and Log() are executed in the same thread.
  virtual void send(google::LogSeverity severity,
                    const char *full_filename,
                    const char * /*base_filename*/,
                    int /*line*/,
                    const tm * /*tm_time*/,
                    const char *message,
                    size_t message_len)
  {
    // We are only interested in the log severity, full file name, and
    // log message.
    message_info_.severity = severity;
    message_info_.file_path = full_filename;
    message_info_.message = std::string(message, message_len);
  }

  // Implements the WaitTillSent() virtual function in class LogSink.
  // It will be executed after send() and after the global logging lock is
  // released, so calls within it (or rather within the Log() method called
  // within) may also issue LOG() statements.
  //
  // LOG(), send(), WaitTillSent() and Log() will occur in the same thread for
  // a given log message.
  virtual void WaitTillSent()
  {
    // First, and very importantly, we save a copy of the message being
    // processed before calling Log(), since Log() may indirectly call send()
    // and WaitTillSent() in the same thread again.
    MessageInfo message_info = message_info_;
    Log(message_info.severity, message_info.file_path, message_info.message);
  }

  // All relevant information about a logged message that needs to be passed
  // from send() to WaitTillSent().
  struct MessageInfo {
    google::LogSeverity severity;
    std::string file_path;
    std::string message;
  };
  MessageInfo message_info_;
};

}  // namespace testing

#endif  // GOOGLE_CERES_INTERNAL_MOCK_LOG_H_
