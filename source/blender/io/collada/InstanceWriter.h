/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "COLLADASWBindMaterial.h"

#include "DNA_object_types.h"

class InstanceWriter {
 protected:
  void add_material_bindings(COLLADASW::BindMaterial &bind_material,
                             Object *ob,
                             bool active_uv_only);
};
