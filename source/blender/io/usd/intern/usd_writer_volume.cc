/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_volume.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_utils.hh"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>

#include "DNA_scene_types.h"
#include "DNA_volume_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_report.hh"
#include "BKE_volume.hh"

#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DEG_depsgraph_query.hh"

namespace blender::io::usd {

static bool has_varying_modifiers(const Object *ob)
{
  /* These modifiers may vary the Volume either over time or by deformation/transformation. */
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);
  while (md) {
    if (ELEM(md->type,
             eModifierType_Nodes,
             eModifierType_VolumeDisplace,
             eModifierType_MeshToVolume))
    {
      return true;
    }
    md = md->next;
  }

  return false;
}

USDVolumeWriter::USDVolumeWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

bool USDVolumeWriter::check_is_animated(const HierarchyContext &context) const
{
  const Volume *volume = static_cast<Volume *>(context.object->data);
  return volume->is_sequence || has_varying_modifiers(context.object);
}

void USDVolumeWriter::do_write(HierarchyContext &context)
{
  Volume *volume = static_cast<Volume *>(context.object->data);
  if (!BKE_volume_load(volume, usd_export_context_.bmain)) {
    return;
  }

  const int num_grids = BKE_volume_num_grids(volume);
  if (!num_grids) {
    return;
  }

  const bool has_modifiers = has_varying_modifiers(context.object);
  auto vdb_file_path = resolve_vdb_file(volume, has_modifiers);
  if (!vdb_file_path.has_value()) {
    BKE_reportf(reports(),
                RPT_WARNING,
                "USD Export: failed to resolve .vdb file for object: %s",
                volume->id.name + 2);
    return;
  }

  if (usd_export_context_.export_params.relative_paths) {
    if (auto relative_vdb_file_path = construct_vdb_relative_file_path(*vdb_file_path)) {
      vdb_file_path = relative_vdb_file_path;
    }
    else {
      BKE_reportf(reports(),
                  RPT_WARNING,
                  "USD Export: couldn't construct relative file path for .vdb file, absolute path "
                  "will be used instead");
    }
  }

  const pxr::UsdTimeCode time = get_export_time_code();
  const pxr::SdfPath &volume_path = usd_export_context_.usd_path;
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  pxr::UsdVolVolume usd_volume = pxr::UsdVolVolume::Define(stage, volume_path);

  for (const int i : IndexRange(num_grids)) {
    const bke::VolumeGridData *grid = BKE_volume_grid_get(volume, i);
    const std::string grid_name = bke::volume_grid::get_name(*grid);
    const std::string grid_id = make_safe_name(grid_name,
                                               usd_export_context_.export_params.allow_unicode);
    const pxr::SdfPath grid_path = volume_path.AppendPath(pxr::SdfPath(grid_id));
    pxr::UsdVolOpenVDBAsset usd_grid = pxr::UsdVolOpenVDBAsset::Define(stage, grid_path);

    pxr::TfToken grid_name_token = pxr::TfToken(grid_name);
    pxr::SdfAssetPath asset_path = pxr::SdfAssetPath(*vdb_file_path);
    pxr::UsdAttribute attr_field = usd_grid.CreateFieldNameAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_file = usd_grid.CreateFilePathAttr(pxr::VtValue(), true);
    if (!attr_field.HasValue()) {
      attr_field.Set(grid_name_token, pxr::UsdTimeCode::Default());
    }
    if (!attr_file.HasValue()) {
      attr_file.Set(asset_path, pxr::UsdTimeCode::Default());
    }

    usd_value_writer_.SetAttribute(attr_field, grid_name_token, time);
    usd_value_writer_.SetAttribute(attr_file, asset_path, time);

    usd_volume.CreateFieldRelationship(pxr::TfToken(grid_id), grid_path);
  }

  this->author_extent(usd_volume, BKE_volume_min_max(volume), time);

  BKE_volume_unload(volume);
}

std::optional<std::string> USDVolumeWriter::resolve_vdb_file(const Volume *volume,
                                                             bool has_modifiers) const
{
  std::optional<std::string> vdb_file_path;

  const bool needs_vdb_save = volume->filepath[0] == '\0' || has_modifiers;
  if (needs_vdb_save) {
    /* Entering this section means that the Volume object contains OpenVDB data that is not
     * obtained solely from external `.vdb` files but is generated or modified inside of Blender.
     * Write this data as a new `.vdb` files. */

    vdb_file_path = construct_vdb_file_path(volume);
    if (!BKE_volume_save(
            volume, usd_export_context_.bmain, nullptr, vdb_file_path.value_or("").c_str()))
    {
      return std::nullopt;
    }
  }

  if (!vdb_file_path.has_value()) {
    vdb_file_path = BKE_volume_grids_frame_filepath(volume);
    if (vdb_file_path->empty()) {
      return std::nullopt;
    }
  }

  return vdb_file_path;
}

std::optional<std::string> USDVolumeWriter::construct_vdb_file_path(const Volume *volume) const
{
  const std::string usd_file_path = get_export_file_path();
  if (usd_file_path.empty()) {
    return std::nullopt;
  }

  char usd_directory_path[FILE_MAX];
  char usd_file_name[FILE_MAXFILE];
  BLI_path_split_dir_file(usd_file_path.c_str(),
                          usd_directory_path,
                          sizeof(usd_directory_path),
                          usd_file_name,
                          sizeof(usd_file_name));

  if (usd_directory_path[0] == '\0' || usd_file_name[0] == '\0') {
    return std::nullopt;
  }

  const char *vdb_directory_name = "volumes";

  char vdb_directory_path[FILE_MAX];
  STRNCPY(vdb_directory_path, usd_directory_path);
  BLI_strncat(vdb_directory_path, vdb_directory_name, sizeof(vdb_directory_path));
  BLI_dir_create_recursive(vdb_directory_path);

  const Scene *scene = DEG_get_input_scene(usd_export_context_.depsgraph);
  const int max_frame_digits = std::max(2, integer_digits_i(abs(scene->r.efra)));

  char vdb_file_name[FILE_MAXFILE];
  STRNCPY(vdb_file_name, volume->id.name + 2);
  const pxr::UsdTimeCode time = get_export_time_code();
  if (!time.IsDefault()) {
    const int frame = int(time.GetValue());
    BLI_path_frame(vdb_file_name, sizeof(vdb_file_name), frame, max_frame_digits);
  }
  BLI_strncat(vdb_file_name, ".vdb", sizeof(vdb_file_name));

  char vdb_file_path[FILE_MAX];
  BLI_path_join(vdb_file_path, sizeof(vdb_file_path), vdb_directory_path, vdb_file_name);

  return vdb_file_path;
}

std::optional<std::string> USDVolumeWriter::construct_vdb_relative_file_path(
    const std::string &vdb_file_path) const
{
  const std::string usd_file_path = get_export_file_path();
  if (usd_file_path.empty()) {
    return std::nullopt;
  }

  char relative_path[FILE_MAX];
  STRNCPY(relative_path, vdb_file_path.c_str());
  BLI_path_rel(relative_path, usd_file_path.c_str());
  if (!BLI_path_is_rel(relative_path)) {
    return std::nullopt;
  }

  /* Following code was written with an assumption that Blender's relative paths start with
   * `//` characters as well as have OS dependent slashes. Inside of USD files those relative
   * paths should start with either `./` or `../` characters and have always forward slashes (`/`)
   * separating directories. This is the convention used in USD documentation (and it seems
   * to be used in other DCC packages as well). */
  std::string relative_path_processed = pxr::TfNormPath(relative_path + 2);
  if (relative_path_processed[0] != '.') {
    relative_path_processed.insert(0, "./");
  }

  return relative_path_processed;
}

}  // namespace blender::io::usd
