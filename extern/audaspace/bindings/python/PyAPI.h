/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

#include <Python.h>
#include "Audaspace.h"

#ifdef __cplusplus
extern "C" {
#endif

PyMODINIT_FUNC
PyInit_aud();

/**
 * Retrieves the python factory of a sound.
 * \param sound The sound factory.
 * \return The python factory.
 */
extern AUD_API PyObject* AUD_getPythonSound(void* sound);

/**
 * Retrieves the sound factory of a python factory.
 * \param sound The python factory.
 * \return The sound factory.
 */
extern AUD_API void* AUD_getSoundFromPython(PyObject* object);

#ifdef __cplusplus
}
#endif
