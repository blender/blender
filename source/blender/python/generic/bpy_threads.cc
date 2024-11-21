/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * This file contains wrapper functions related to global interpreter lock.
 * these functions are slightly different from the original Python API,
 * don't throw SIGABRT even if the thread state is nullptr. */

#include <Python.h>

#include "../BPY_extern.hh"

BPy_ThreadStatePtr BPY_thread_save()
{
  /* Use `_PyThreadState_UncheckedGet()` instead of `PyThreadState_Get()`, to avoid a fatal error
   * issued when a thread state is nullptr (the thread state can be nullptr when quitting Blender).
   *
   * `PyEval_SaveThread()` will release the GIL, so this thread has to have the GIL to begin with
   * or badness will ensue. */
  if (_PyThreadState_UncheckedGet() && PyGILState_Check()) {
    return (BPy_ThreadStatePtr)PyEval_SaveThread();
  }
  return nullptr;
}

void BPY_thread_restore(BPy_ThreadStatePtr tstate)
{
  if (tstate) {
    PyEval_RestoreThread((PyThreadState *)tstate);
  }
}

void BPY_thread_backtrace_print()
{
  PyThreadState *tstate = PyGILState_GetThisThreadState();

  if (tstate) {
    PyFrameObject *frame = PyThreadState_GetFrame(tstate);

    printf(frame ? "Python stack trace:\n" : "No Python stack trace available.\n");

    while (frame) {
      PyCodeObject *frame_co = PyFrame_GetCode(frame);
      int line = PyFrame_GetLineNumber(frame);
      const char *filename = PyUnicode_AsUTF8(frame_co->co_filename);
      const char *funcname = PyUnicode_AsUTF8(frame_co->co_name);
      printf("    %s:%d %s\n", filename, line, funcname);
      Py_DECREF(frame_co);
      PyFrameObject *frame_back = PyFrame_GetBack(frame);
      Py_DECREF(frame);
      frame = frame_back;
    }
    printf("\n");
  }
  else {
    printf("No Python thread state available.\n");
  }
}
