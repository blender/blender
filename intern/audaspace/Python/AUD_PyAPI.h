/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/Python/AUD_PyAPI.h
 *  \ingroup audpython
 */


#ifndef __AUD_PYAPI_H__
#define __AUD_PYAPI_H__

#include "Python.h"

#ifdef __cplusplus
extern "C" {
#else
typedef void AUD_IFactory;
typedef void AUD_IDevice;
typedef void AUD_IHandle;
#endif

typedef void AUD_Reference_AUD_IFactory;
typedef void AUD_Reference_AUD_IDevice;
typedef void AUD_Reference_AUD_IHandle;

typedef struct {
	PyObject_HEAD
	PyObject* child_list;
	AUD_Reference_AUD_IFactory* factory;
} Factory;

typedef struct {
	PyObject_HEAD
	AUD_Reference_AUD_IHandle* handle;
} Handle;

typedef struct {
	PyObject_HEAD
	AUD_Reference_AUD_IDevice* device;
} Device;

PyMODINIT_FUNC
PyInit_aud(void);

extern PyObject* Device_empty();
extern PyObject* Factory_empty();
extern Factory* checkFactory(PyObject* factory);

#ifdef __cplusplus
}
#endif

#endif //__AUD_PYAPI_H__
