// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: Craig Silverstein.
//
// A simple mutex wrapper, supporting locks and read-write locks.
// You should assume the locks are *not* re-entrant.
//
// This class is meant to be internal-only and should be wrapped by an
// internal namespace.  Before you use this module, please give the
// name of your internal namespace for this module.  Or, if you want
// to expose it, you'll want to move it to the Google namespace.  We
// cannot put this class in global namespace because there can be some
// problems when we have multiple versions of Mutex in each shared object.
//
// NOTE: by default, we have #ifdef'ed out the TryLock() method.
//       This is for two reasons:
// 1) TryLock() under Windows is a bit annoying (it requires a
//    #define to be defined very early).
// 2) TryLock() is broken for NO_THREADS mode, at least in NDEBUG
//    mode.
// If you need TryLock(), and either these two caveats are not a
// problem for you, or you're willing to work around them, then
// feel free to #define GMUTEX_TRYLOCK, or to remove the #ifdefs
// in the code below.
//
// CYGWIN NOTE: Cygwin support for rwlock seems to be buggy:
//    http://www.cygwin.com/ml/cygwin/2008-12/msg00017.html
// Because of that, we might as well use windows locks for
// cygwin.  They seem to be more reliable than the cygwin pthreads layer.
//
// TRICKY IMPLEMENTATION NOTE:
// This class is designed to be safe to use during
// dynamic-initialization -- that is, by global constructors that are
// run before main() starts.  The issue in this case is that
// dynamic-initialization happens in an unpredictable order, and it
// could be that someone else's dynamic initializer could call a
// function that tries to acquire this mutex -- but that all happens
// before this mutex's constructor has run.  (This can happen even if
// the mutex and the function that uses the mutex are in the same .cc
// file.)  Basically, because Mutex does non-trivial work in its
// constructor, it's not, in the naive implementation, safe to use
// before dynamic initialization has run on it.
//
// The solution used here is to pair the actual mutex primitive with a
// bool that is set to true when the mutex is dynamically initialized.
// (Before that it's false.)  Then we modify all mutex routines to
// look at the bool, and not try to lock/unlock until the bool makes
// it to true (which happens after the Mutex constructor has run.)
//
// This works because before main() starts -- particularly, during
// dynamic initialization -- there are no threads, so a) it's ok that
// the mutex operations are a no-op, since we don't need locking then
// anyway; and b) we can be quite confident our bool won't change
// state between a call to Lock() and a call to Unlock() (that would
// require a global constructor in one translation unit to call Lock()
// and another global constructor in another translation unit to call
// Unlock() later, which is pretty perverse).
//
// That said, it's tricky, and can conceivably fail; it's safest to
// avoid trying to acquire a mutex in a global constructor, if you
// can.  One way it can fail is that a really smart compiler might
// initialize the bool to true at static-initialization time (too
// early) rather than at dynamic-initialization time.  To discourage
// that, we set is_safe_ to true in code (not the constructor
// colon-initializer) and set it to true via a function that always
// evaluates to true, but that the compiler can't know always
// evaluates to true.  This should be good enough.

#ifndef CERES_INTERNAL_MUTEX_H_
#define CERES_INTERNAL_MUTEX_H_

#include "ceres/internal/port.h"

#if defined(CERES_NO_THREADS)
  typedef int MutexType;      // to keep a lock-count
#elif defined(_WIN32) || defined(__CYGWIN32__) || defined(__CYGWIN64__)
# define CERES_WIN32_LEAN_AND_MEAN  // We only need minimal includes
# ifdef CERES_GMUTEX_TRYLOCK
  // We need Windows NT or later for TryEnterCriticalSection().  If you
  // don't need that functionality, you can remove these _WIN32_WINNT
  // lines, and change TryLock() to assert(0) or something.
#   ifndef _WIN32_WINNT
#     define _WIN32_WINNT 0x0400
#   endif
# endif
// Unfortunately, windows.h defines a bunch of macros with common
// names. Two in particular need avoiding: ERROR and min/max.
// To avoid macro definition of ERROR.
# define NOGDI
// To avoid macro definition of min/max.
# ifndef NOMINMAX
#   define NOMINMAX
# endif
# include <windows.h>
  typedef CRITICAL_SECTION MutexType;
#elif defined(CERES_HAVE_PTHREAD) && defined(CERES_HAVE_RWLOCK)
  // Needed for pthread_rwlock_*.  If it causes problems, you could take it
  // out, but then you'd have to unset CERES_HAVE_RWLOCK (at least on linux --
  // it *does* cause problems for FreeBSD, or MacOSX, but isn't needed for
  // locking there.)
# if defined(__linux__) && !defined(_XOPEN_SOURCE)
#   define _XOPEN_SOURCE 500  // may be needed to get the rwlock calls
# endif
# include <pthread.h>
  typedef pthread_rwlock_t MutexType;
#elif defined(CERES_HAVE_PTHREAD)
# include <pthread.h>
  typedef pthread_mutex_t MutexType;
#else
# error Need to implement mutex.h for your architecture, or #define NO_THREADS
#endif

// We need to include these header files after defining _XOPEN_SOURCE
// as they may define the _XOPEN_SOURCE macro.
#include <assert.h>
#include <stdlib.h>      // for abort()

namespace ceres {
namespace internal {

class Mutex {
 public:
  // Create a Mutex that is not held by anybody.  This constructor is
  // typically used for Mutexes allocated on the heap or the stack.
  // See below for a recommendation for constructing global Mutex
  // objects.
  inline Mutex();

  // Destructor
  inline ~Mutex();

  inline void Lock();    // Block if needed until free then acquire exclusively
  inline void Unlock();  // Release a lock acquired via Lock()
#ifdef CERES_GMUTEX_TRYLOCK
  inline bool TryLock(); // If free, Lock() and return true, else return false
#endif
  // Note that on systems that don't support read-write locks, these may
  // be implemented as synonyms to Lock() and Unlock().  So you can use
  // these for efficiency, but don't use them anyplace where being able
  // to do shared reads is necessary to avoid deadlock.
  inline void ReaderLock();   // Block until free or shared then acquire a share
  inline void ReaderUnlock(); // Release a read share of this Mutex
  inline void WriterLock() { Lock(); }     // Acquire an exclusive lock
  inline void WriterUnlock() { Unlock(); } // Release a lock from WriterLock()

  // TODO(hamaji): Do nothing, implement correctly.
  inline void AssertHeld() {}

 private:
  MutexType mutex_;
  // We want to make sure that the compiler sets is_safe_ to true only
  // when we tell it to, and never makes assumptions is_safe_ is
  // always true.  volatile is the most reliable way to do that.
  volatile bool is_safe_;

  inline void SetIsSafe() { is_safe_ = true; }

  // Catch the error of writing Mutex when intending MutexLock.
  Mutex(Mutex* /*ignored*/) {}
  // Disallow "evil" constructors
  Mutex(const Mutex&);
  void operator=(const Mutex&);
};

// Now the implementation of Mutex for various systems
#if defined(CERES_NO_THREADS)

// When we don't have threads, we can be either reading or writing,
// but not both.  We can have lots of readers at once (in no-threads
// mode, that's most likely to happen in recursive function calls),
// but only one writer.  We represent this by having mutex_ be -1 when
// writing and a number > 0 when reading (and 0 when no lock is held).
//
// In debug mode, we assert these invariants, while in non-debug mode
// we do nothing, for efficiency.  That's why everything is in an
// assert.

Mutex::Mutex() : mutex_(0) { }
Mutex::~Mutex()            { assert(mutex_ == 0); }
void Mutex::Lock()         { assert(--mutex_ == -1); }
void Mutex::Unlock()       { assert(mutex_++ == -1); }
#ifdef CERES_GMUTEX_TRYLOCK
bool Mutex::TryLock()      { if (mutex_) return false; Lock(); return true; }
#endif
void Mutex::ReaderLock()   { assert(++mutex_ > 0); }
void Mutex::ReaderUnlock() { assert(mutex_-- > 0); }

#elif defined(_WIN32) || defined(__CYGWIN32__) || defined(__CYGWIN64__)

Mutex::Mutex()             { InitializeCriticalSection(&mutex_); SetIsSafe(); }
Mutex::~Mutex()            { DeleteCriticalSection(&mutex_); }
void Mutex::Lock()         { if (is_safe_) EnterCriticalSection(&mutex_); }
void Mutex::Unlock()       { if (is_safe_) LeaveCriticalSection(&mutex_); }
#ifdef GMUTEX_TRYLOCK
bool Mutex::TryLock()      { return is_safe_ ?
                                 TryEnterCriticalSection(&mutex_) != 0 : true; }
#endif
void Mutex::ReaderLock()   { Lock(); }      // we don't have read-write locks
void Mutex::ReaderUnlock() { Unlock(); }

#elif defined(CERES_HAVE_PTHREAD) && defined(CERES_HAVE_RWLOCK)

#define CERES_SAFE_PTHREAD(fncall) do { /* run fncall if is_safe_ is true */ \
  if (is_safe_ && fncall(&mutex_) != 0) abort();                             \
} while (0)

Mutex::Mutex() {
  SetIsSafe();
  if (is_safe_ && pthread_rwlock_init(&mutex_, NULL) != 0) abort();
}
Mutex::~Mutex()            { CERES_SAFE_PTHREAD(pthread_rwlock_destroy); }
void Mutex::Lock()         { CERES_SAFE_PTHREAD(pthread_rwlock_wrlock); }
void Mutex::Unlock()       { CERES_SAFE_PTHREAD(pthread_rwlock_unlock); }
#ifdef CERES_GMUTEX_TRYLOCK
bool Mutex::TryLock()      { return is_safe_ ?
                                    pthread_rwlock_trywrlock(&mutex_) == 0 :
                                    true; }
#endif
void Mutex::ReaderLock()   { CERES_SAFE_PTHREAD(pthread_rwlock_rdlock); }
void Mutex::ReaderUnlock() { CERES_SAFE_PTHREAD(pthread_rwlock_unlock); }
#undef CERES_SAFE_PTHREAD

#elif defined(CERES_HAVE_PTHREAD)

#define CERES_SAFE_PTHREAD(fncall) do { /* run fncall if is_safe_ is true */  \
  if (is_safe_ && fncall(&mutex_) != 0) abort();                              \
} while (0)

Mutex::Mutex()             {
  SetIsSafe();
  if (is_safe_ && pthread_mutex_init(&mutex_, NULL) != 0) abort();
}
Mutex::~Mutex()            { CERES_SAFE_PTHREAD(pthread_mutex_destroy); }
void Mutex::Lock()         { CERES_SAFE_PTHREAD(pthread_mutex_lock); }
void Mutex::Unlock()       { CERES_SAFE_PTHREAD(pthread_mutex_unlock); }
#ifdef CERES_GMUTEX_TRYLOCK
bool Mutex::TryLock()      { return is_safe_ ?
                                 pthread_mutex_trylock(&mutex_) == 0 : true; }
#endif
void Mutex::ReaderLock()   { Lock(); }
void Mutex::ReaderUnlock() { Unlock(); }
#undef CERES_SAFE_PTHREAD

#endif

// --------------------------------------------------------------------------
// Some helper classes

// Note: The weird "Ceres" prefix for the class is a workaround for having two
// similar mutex.h files included in the same translation unit. This is a
// problem because macros do not respect C++ namespaces, and as a result, this
// does not work well (e.g. inside Chrome). The offending macros are
// "MutexLock(x) COMPILE_ASSERT(false)". To work around this, "Ceres" is
// prefixed to the class names; this permits defining the classes.

// CeresMutexLock(mu) acquires mu when constructed and releases it
// when destroyed.
class CeresMutexLock {
 public:
  explicit CeresMutexLock(Mutex *mu) : mu_(mu) { mu_->Lock(); }
  ~CeresMutexLock() { mu_->Unlock(); }
 private:
  Mutex * const mu_;
  // Disallow "evil" constructors
  CeresMutexLock(const CeresMutexLock&);
  void operator=(const CeresMutexLock&);
};

// CeresReaderMutexLock and CeresWriterMutexLock do the same, for rwlocks
class CeresReaderMutexLock {
 public:
  explicit CeresReaderMutexLock(Mutex *mu) : mu_(mu) { mu_->ReaderLock(); }
  ~CeresReaderMutexLock() { mu_->ReaderUnlock(); }
 private:
  Mutex * const mu_;
  // Disallow "evil" constructors
  CeresReaderMutexLock(const CeresReaderMutexLock&);
  void operator=(const CeresReaderMutexLock&);
};

class CeresWriterMutexLock {
 public:
  explicit CeresWriterMutexLock(Mutex *mu) : mu_(mu) { mu_->WriterLock(); }
  ~CeresWriterMutexLock() { mu_->WriterUnlock(); }
 private:
  Mutex * const mu_;
  // Disallow "evil" constructors
  CeresWriterMutexLock(const CeresWriterMutexLock&);
  void operator=(const CeresWriterMutexLock&);
};

// Catch bug where variable name is omitted, e.g. MutexLock (&mu);
#define CeresMutexLock(x) \
    COMPILE_ASSERT(0, ceres_mutex_lock_decl_missing_var_name)
#define CeresReaderMutexLock(x) \
    COMPILE_ASSERT(0, ceres_rmutex_lock_decl_missing_var_name)
#define CeresWriterMutexLock(x) \
    COMPILE_ASSERT(0, ceres_wmutex_lock_decl_missing_var_name)

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_MUTEX_H_
