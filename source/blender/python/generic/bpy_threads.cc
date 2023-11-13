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

#include "../BPY_extern.h"
#include "BLI_utildefines.h"

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
