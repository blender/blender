/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>

#include "BLI_vector.hh"

namespace blender {

struct Depsgraph;
struct FluidModifierData;
struct Object;

namespace io::hydra {

struct VolumeFieldDescriptor {
  pxr::TfToken name;
  pxr::SdfPath field_path;
};

/** Evaluated #FluidModifierData for a mesh object. */
const FluidModifierData *fluid_gas_domain_modifier(const Object *object,
                                                   const Depsgraph *depsgraph);

/** Resolve the VDB cache file path,per-grid fields and transform
 * for a fluid modifier. */
std::string build_volume_fields_from_modifier(const Object *object,
                                              const FluidModifierData *fmd,
                                              int frame,
                                              const pxr::SdfPath &volume_path,
                                              pxr::GfMatrix4d *r_geometry_xform,
                                              Vector<VolumeFieldDescriptor> *r_fields);

}  // namespace io::hydra
}  // namespace blender
