/* SPDX-FileCopyrightText: 2009-2011 Jörg Hermann Müller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup audaspaceintern
 */

#ifndef __AUD_PYINIT_H__
#define __AUD_PYINIT_H__

#ifdef WITH_PYTHON
#  include "Python.h"

#  ifdef __cplusplus
extern "C" {
#  endif

/**
 * Initializes the Python module.
 */
extern PyObject *AUD_initPython(void);

#  ifdef __cplusplus
}
#  endif

#endif

#endif  //__AUD_PYINIT_H__
