/*
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
 */

/** \file
 * \ingroup audaspaceintern
 */

#include "AUD_PyInit.h"

#include <AUD_Sound.h>
#include <python/PySound.h>
#include <python/PyAPI.h>

extern "C" {
extern void *BKE_sound_get_factory(void *sound);
}

static PyObject *AUD_getSoundFromPointer(PyObject *self, PyObject *args)
{
  PyObject *lptr = NULL;

  if (PyArg_Parse(args, "O:_sound_from_pointer", &lptr)) {
    if (lptr) {
      AUD_Sound *sound = BKE_sound_get_factory(PyLong_AsVoidPtr(lptr));

      if (sound) {
        Sound *obj = (Sound *)Sound_empty();
        if (obj) {
          obj->sound = AUD_Sound_copy(sound);
          return (PyObject *)obj;
        }
      }
    }
  }

  Py_RETURN_NONE;
}

static PyMethodDef meth_sound_from_pointer[] = {
    {"_sound_from_pointer",
     (PyCFunction)AUD_getSoundFromPointer,
     METH_O,
     "_sound_from_pointer(pointer)\n\n"
     "Returns the corresponding :class:`Factory` object.\n\n"
     ":arg pointer: The pointer to the bSound object as long.\n"
     ":type pointer: long\n"
     ":return: The corresponding :class:`Factory` object.\n"
     ":rtype: :class:`Factory`"}};

PyObject *AUD_initPython(void)
{
  PyObject *module = PyInit_aud();
  if (module == NULL) {
    printf("Unable to initialise audio\n");
    return NULL;
  }

  PyModule_AddObject(
      module, "_sound_from_pointer", (PyObject *)PyCFunction_New(meth_sound_from_pointer, NULL));
  PyDict_SetItemString(PyImport_GetModuleDict(), "aud", module);

  return module;
}
