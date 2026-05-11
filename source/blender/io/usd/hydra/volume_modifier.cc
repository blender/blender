/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "volume_modifier.hh"

#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"

#include "BKE_mesh.h"
#include "BKE_modifier.hh"

#include "DNA_fluid_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "util.hh"

namespace blender::io::hydra {

const FluidModifierData *fluid_gas_domain_modifier(const Object *object,
                                                   const Depsgraph *depsgraph)
{
  if (object->type != OB_MESH) {
    return nullptr;
  }
  const ModifierData *md = BKE_modifiers_findby_type(object, eModifierType_Fluid);
  if (!md) {
    return nullptr;
  }
  const FluidModifierData *fmd = reinterpret_cast<const FluidModifierData *>(
      BKE_modifier_get_evaluated(const_cast<Depsgraph *>(depsgraph),
                                 const_cast<Object *>(object),
                                 const_cast<ModifierData *>(md)));
  if (!fmd || !(fmd->type & MOD_FLUID_TYPE_DOMAIN) || fmd->domain->type != FLUID_DOMAIN_TYPE_GAS) {
    return nullptr;
  }
  return fmd;
}

/* On-disk VDB cache file path for the given frame. */
static std::string fluid_cache_file_path(const std::string &directory, const int frame)
{
  char file_path[FILE_MAX];
  char file_name[32];
  SNPRINTF_UTF8(file_name, "%s_####%s", FLUID_NAME_DATA, FLUID_DOMAIN_EXTENSION_OPENVDB);
  BLI_path_frame(file_name, sizeof(file_name), frame, 0);
  BLI_path_join(file_path, sizeof(file_path), directory.c_str(), FLUID_DOMAIN_DIR_DATA, file_name);
  return file_path;
}

static pxr::GfMatrix4d volume_modifier_geometry_transform(const Object *object,
                                                          const FluidModifierData *fmd)
{
  pxr::GfMatrix4d transform = pxr::GfMatrix4d().SetScale(
      pxr::GfVec3d(fmd->domain->scale / fmd->domain->global_size[0],
                   fmd->domain->scale / fmd->domain->global_size[1],
                   fmd->domain->scale / fmd->domain->global_size[2]));
  transform *= pxr::GfMatrix4d().SetTranslate(pxr::GfVec3d(-1, -1, -1));

  float texspace_loc[3] = {0.0f, 0.0f, 0.0f};
  float texspace_scale[3] = {1.0f, 1.0f, 1.0f};
  BKE_mesh_texspace_get(id_cast<Mesh *>(object->data), texspace_loc, texspace_scale);
  transform *= pxr::GfMatrix4d(1.0f).SetScale(pxr::GfVec3d(texspace_scale)) *
               pxr::GfMatrix4d(1.0f).SetTranslate(pxr::GfVec3d(texspace_loc));

  return transform;
}

std::string build_volume_fields_from_modifier(const Object *object,
                                              const FluidModifierData *fmd,
                                              const int frame,
                                              const pxr::SdfPath &volume_path,
                                              pxr::GfMatrix4d *r_geometry_xform,
                                              Vector<VolumeFieldDescriptor> *r_fields)
{
  *r_geometry_xform = volume_modifier_geometry_transform(object, fmd);

  /* Fields are only published for OpenVDB-format fluid caches. Other
   * formats still emit the volume Rprim (suppressing the carrier mesh)
   * with no bindings. */
  if ((fmd->domain->cache_data_format & FLUID_DOMAIN_FILE_OPENVDB) == 0) {
    return {};
  }

  static const pxr::TfToken grid_tokens[] = {pxr::TfToken("density", pxr::TfToken::Immortal),
                                             pxr::TfToken("flame", pxr::TfToken::Immortal),
                                             pxr::TfToken("shadow", pxr::TfToken::Immortal),
                                             pxr::TfToken("temperature", pxr::TfToken::Immortal),
                                             pxr::TfToken("velocity", pxr::TfToken::Immortal)};
  for (const pxr::TfToken &grid_name : grid_tokens) {
    VolumeFieldDescriptor f;
    f.name = grid_name;
    f.field_path = volume_path.AppendElementString("VF_" + grid_name.GetString());
    r_fields->append(f);
  }

  return fluid_cache_file_path(fmd->domain->cache_directory, frame);
}

}  // namespace blender::io::hydra
