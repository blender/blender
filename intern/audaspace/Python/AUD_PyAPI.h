/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_PYAPI
#define AUD_PYAPI

#include "Python.h"

#ifdef __cplusplus
extern "C" {
#include "AUD_IDevice.h"
#else
typedef void AUD_IFactory;
typedef void AUD_IDevice;
typedef void AUD_Handle;
#endif

typedef struct {
	PyObject_HEAD
	PyObject* child_list;
	AUD_IFactory* factory;
} Factory;

typedef struct {
	PyObject_HEAD
	AUD_Handle* handle;
	PyObject* device;
} Handle;

typedef struct {
	PyObject_HEAD
	AUD_IDevice* device;
} Device;

PyMODINIT_FUNC
PyInit_aud(void);

extern PyObject *
Device_empty();

#ifdef __cplusplus
}
#endif

#endif //AUD_PYAPI
