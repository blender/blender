/* SPDX-FileCopyrightText: 2009-2011 Jörg Hermann Müller
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "bpy_audaspace.hh"

#ifdef WITH_AUDASPACE_PY

#  include "BKE_sound.hh"

#  include <python/PyAPI.h>
#  include <python/PySound.h>

namespace blender {

static PyObject *AUD_getSoundFromPointer(PyObject * /*self*/, PyObject *args)
{
  PyObject *res = nullptr;
  if (PyArg_Parse(args, "O:_sound_from_pointer", &res)) {
    if (res) {
      AUD_Sound sound = BKE_sound_get_factory(PyLong_AsVoidPtr(res));
      if (sound) {
        Sound *obj = (Sound *)Sound_empty();
        if (obj) {
          obj->sound = new AUD_Sound(sound);
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

PyObject *BPyInit_audaspace()
{
  PyObject *module = PyInit_aud();
  if (module == nullptr) {
    printf("Unable to initialize audio\n");
    return nullptr;
  }

  PyModule_AddObject(
      module, "_sound_from_pointer", PyCFunction_New(meth_sound_from_pointer, nullptr));
  PyDict_SetItemString(PyImport_GetModuleDict(), "aud", module);

  return module;
}

}  // namespace blender

#endif  // WITH_AUDASPACE_PY
