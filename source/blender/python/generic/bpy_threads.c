/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): None yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/bpy_threads.c
 *  \ingroup pygen
 *
 * This file contains wrapper functions related to global interpreter lock.
 * these functions are slightly different from the original Python API,
 * don't throw SIGABRT even if the thread state is NULL. */

/* grr, python redefines */
#ifdef _POSIX_C_SOURCE
#  undef _POSIX_C_SOURCE
#endif

#include <Python.h>

#include "BLI_utildefines.h"
#include "../BPY_extern.h"

/* analogue of PyEval_SaveThread() */
BPy_ThreadStatePtr BPY_thread_save(void)
{
	PyThreadState *tstate = PyThreadState_Swap(NULL);
	/* note: tstate can be NULL when quitting Blender */

	if (tstate && PyEval_ThreadsInitialized()) {
		PyEval_ReleaseLock();
	}

	return (BPy_ThreadStatePtr)tstate;
}

/* analogue of PyEval_RestoreThread() */
void BPY_thread_restore(BPy_ThreadStatePtr tstate)
{
	if (tstate) {
		PyEval_RestoreThread((PyThreadState *)tstate);
	}
}
