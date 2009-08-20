/* Threads.c -- multithreading library
2008-08-05
Igor Pavlov
Public domain */

#include "Threads.h"
#include <process.h>

static WRes GetError()
{
  DWORD res = GetLastError();
  return (res) ? (WRes)(res) : 1;
}

WRes HandleToWRes(HANDLE h) { return (h != 0) ? 0 : GetError(); }
WRes BOOLToWRes(BOOL v) { return v ? 0 : GetError(); }

static WRes MyCloseHandle(HANDLE *h)
{
  if (*h != NULL)
    if (!CloseHandle(*h))
      return GetError();
  *h = NULL;
  return 0;
}

WRes Thread_Create(CThread *thread, THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE *startAddress)(void *), LPVOID parameter)
{
  unsigned threadId; /* Windows Me/98/95: threadId parameter may not be NULL in _beginthreadex/CreateThread functions */
  thread->handle =
    /* CreateThread(0, 0, startAddress, parameter, 0, &threadId); */
    (HANDLE)_beginthreadex(NULL, 0, startAddress, parameter, 0, &threadId);
    /* maybe we must use errno here, but probably GetLastError() is also OK. */
  return HandleToWRes(thread->handle);
}

WRes WaitObject(HANDLE h)
{
  return (WRes)WaitForSingleObject(h, INFINITE);
}

WRes Thread_Wait(CThread *thread)
{
  if (thread->handle == NULL)
    return 1;
  return WaitObject(thread->handle);
}

WRes Thread_Close(CThread *thread)
{
  return MyCloseHandle(&thread->handle);
}

WRes Event_Create(CEvent *p, BOOL manualReset, int initialSignaled)
{
  p->handle = CreateEvent(NULL, manualReset, (initialSignaled ? TRUE : FALSE), NULL);
  return HandleToWRes(p->handle);
}

WRes ManualResetEvent_Create(CManualResetEvent *p, int initialSignaled)
  { return Event_Create(p, TRUE, initialSignaled); }
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p)
  { return ManualResetEvent_Create(p, 0); }

WRes AutoResetEvent_Create(CAutoResetEvent *p, int initialSignaled)
  { return Event_Create(p, FALSE, initialSignaled); }
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p)
  { return AutoResetEvent_Create(p, 0); }

WRes Event_Set(CEvent *p) { return BOOLToWRes(SetEvent(p->handle)); }
WRes Event_Reset(CEvent *p) { return BOOLToWRes(ResetEvent(p->handle)); }
WRes Event_Wait(CEvent *p) { return WaitObject(p->handle); }
WRes Event_Close(CEvent *p) { return MyCloseHandle(&p->handle); }


WRes Semaphore_Create(CSemaphore *p, UInt32 initiallyCount, UInt32 maxCount)
{
  p->handle = CreateSemaphore(NULL, (LONG)initiallyCount, (LONG)maxCount, NULL);
  return HandleToWRes(p->handle);
}

WRes Semaphore_Release(CSemaphore *p, LONG releaseCount, LONG *previousCount)
{
  return BOOLToWRes(ReleaseSemaphore(p->handle, releaseCount, previousCount));
}
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 releaseCount)
{
  return Semaphore_Release(p, (LONG)releaseCount, NULL);
}
WRes Semaphore_Release1(CSemaphore *p)
{
  return Semaphore_ReleaseN(p, 1);
}

WRes Semaphore_Wait(CSemaphore *p) { return WaitObject(p->handle); }
WRes Semaphore_Close(CSemaphore *p) { return MyCloseHandle(&p->handle); }

WRes CriticalSection_Init(CCriticalSection *p)
{
  /* InitializeCriticalSection can raise only STATUS_NO_MEMORY exception */
  __try
  {
    InitializeCriticalSection(p);
    /* InitializeCriticalSectionAndSpinCount(p, 0); */
  }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
  return 0;
}

