/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines utility functions that use the RNA API, from PyDrivers.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_fcurve_driver.h"

#include "RNA_access.hh"

#include "bpy_rna.h"

#include "bpy_rna_driver.h" /* own include */

PyObject *pyrna_driver_get_variable_value(const AnimationEvalContext *anim_eval_context,
                                          ChannelDriver *driver,
                                          DriverVar *dvar,
                                          DriverTarget *dtar)
{
  PyObject *driver_arg = nullptr;
  PointerRNA ptr;
  PropertyRNA *prop = nullptr;
  int index;

  if (driver_get_variable_property(anim_eval_context, driver, dvar, dtar, &ptr, &prop, &index)) {
    if (prop) {
      if (index != -1) {
        if (index < RNA_property_array_length(&ptr, prop) && index >= 0) {
          /* object, property & index */
          driver_arg = pyrna_array_index(&ptr, prop, index);
        }
        else {
          /* out of range, pass */
        }
      }
      else {
        /* object & property */
        const PropertyType type = RNA_property_type(prop);
        if (type == PROP_ENUM) {
          /* Note that enum's are converted to strings by default,
           * we want to avoid that, see: #52213 */
          driver_arg = PyLong_FromLong(RNA_property_enum_get(&ptr, prop));
        }
        else {
          driver_arg = pyrna_prop_to_py(&ptr, prop);
        }
      }
    }
    else {
      /* object only */
      driver_arg = pyrna_struct_CreatePyObject(&ptr);
    }
  }
  else {
    /* can't resolve path, pass */
  }

  return driver_arg;
}

PyObject *pyrna_driver_self_from_anim_rna(PathResolvedRNA *anim_rna)
{
  return pyrna_struct_CreatePyObject(&anim_rna->ptr);
}

bool pyrna_driver_is_equal_anim_rna(const PathResolvedRNA *anim_rna, const PyObject *py_anim_rna)
{
  if (BPy_StructRNA_Check(py_anim_rna)) {
    const PointerRNA *ptr_a = &anim_rna->ptr;
    const PointerRNA *ptr_b = &(((const BPy_StructRNA *)py_anim_rna)->ptr);

    if ((ptr_a->owner_id == ptr_b->owner_id) && (ptr_a->type == ptr_b->type) &&
        (ptr_a->data == ptr_b->data))
    {
      return true;
    }
  }
  return false;
}
