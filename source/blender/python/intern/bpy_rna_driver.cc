/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines utility functions that use the RNA API, from PyDrivers.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"

#include "BLI_utildefines.h"

#include "BKE_fcurve_driver.h"

#include "RNA_access.hh"

#include "bpy_rna.hh"

#include "bpy_rna_driver.hh" /* own include */

PyObject *pyrna_driver_get_variable_value(const AnimationEvalContext *anim_eval_context,
                                          ChannelDriver *driver,
                                          DriverVar *dvar,
                                          DriverTarget *dtar)
{
  PointerRNA ptr;
  PropertyRNA *prop = nullptr;
  int index;

  switch (driver_get_variable_property(
      anim_eval_context, driver, dvar, dtar, true, &ptr, &prop, &index))
  {
    case DRIVER_VAR_PROPERTY_SUCCESS:
      /* object only */
      if (!prop) {
        return pyrna_struct_CreatePyObject(&ptr);
      }

      /* object, property & index */
      if (index >= 0) {
        return pyrna_array_index(&ptr, prop, index);
      }

      /* object & property (enum) */
      if (RNA_property_type(prop) == PROP_ENUM) {
        /* Note that enum's are converted to strings by default,
         * we want to avoid that, see: #52213 */
        return PyLong_FromLong(RNA_property_enum_get(&ptr, prop));
      }

      /* object & property */
      return pyrna_prop_to_py(&ptr, prop);

    case DRIVER_VAR_PROPERTY_FALLBACK:
      return PyFloat_FromDouble(dtar->fallback_value);

    case DRIVER_VAR_PROPERTY_INVALID:
    case DRIVER_VAR_PROPERTY_INVALID_INDEX:
      /* can't resolve path, pass */
      return nullptr;
  }

  return nullptr;
}

PyObject *pyrna_driver_self_from_anim_rna(PathResolvedRNA *anim_rna)
{
  return pyrna_struct_CreatePyObject(&anim_rna->ptr);
}

bool pyrna_driver_is_equal_anim_rna(const PathResolvedRNA *anim_rna, const PyObject *py_anim_rna)
{
  if (BPy_StructRNA_Check(py_anim_rna)) {
    const PointerRNA *ptr_a = &anim_rna->ptr;
    const PointerRNA *ptr_b = &reinterpret_cast<const BPy_StructRNA *>(py_anim_rna)->ptr.value();

    if ((ptr_a->owner_id == ptr_b->owner_id) && (ptr_a->type == ptr_b->type) &&
        (ptr_a->data == ptr_b->data))
    {
      return true;
    }
  }
  return false;
}
