/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include <Python.h>

namespace blender {

/* GPU Device Python object structure */
struct BPyGPUDevice {
  PyObject_HEAD
  int index;
  const char *identifier;
  const char *name;
};

extern PyTypeObject BPyGPU_DeviceType;

[[nodiscard]] PyObject *bpygpu_platform_init();

}  // namespace blender
