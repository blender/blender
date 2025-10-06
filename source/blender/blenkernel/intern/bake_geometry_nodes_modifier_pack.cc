/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_bake_geometry_nodes_modifier_pack.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.hh"
#include "BKE_report.hh"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "MOD_nodes.hh"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DEG_depsgraph.hh"

namespace blender::bke::bake {

static Vector<NodesModifierBakeFile> pack_files_from_directory(const StringRefNull directory,
                                                               ReportList *reports)
{
  if (!BLI_is_dir(directory.c_str())) {
    BKE_reportf(reports, RPT_ERROR, "%s is not a directory", directory.c_str());
    return {};
  }

  direntry *dir_entries = nullptr;
  const int dir_entries_num = BLI_filelist_dir_contents(directory.c_str(), &dir_entries);
  BLI_SCOPED_DEFER([&]() { BLI_filelist_free(dir_entries, dir_entries_num); });

  Vector<NodesModifierBakeFile> bake_files;
  for (const int i : IndexRange(dir_entries_num)) {
    const direntry &dir_entry = dir_entries[i];
    const StringRefNull dir_entry_path = dir_entry.path;
    const StringRefNull name = dir_entry.relname;
    if (FILENAME_IS_CURRPAR(name.c_str())) {
      continue;
    }
    NodesModifierBakeFile bake_file;
    bake_file.name = BLI_strdup_null(name.c_str());
    bake_file.packed_file = BKE_packedfile_new(reports, dir_entry_path.c_str(), "");
    if (bake_file.packed_file) {
      bake_files.append(bake_file);
    }
  }

  return bake_files;
}

NodesModifierPackedBake *pack_bake_from_disk(const BakePath &bake_path, ReportList *reports)
{
  const Vector<NodesModifierBakeFile> meta_bake_files = pack_files_from_directory(
      bake_path.meta_dir, reports);
  if (meta_bake_files.is_empty()) {
    return nullptr;
  }

  const Vector<NodesModifierBakeFile> blob_bake_files = pack_files_from_directory(
      bake_path.blobs_dir, reports);

  NodesModifierPackedBake *packed_bake = MEM_callocN<NodesModifierPackedBake>(__func__);
  packed_bake->meta_files_num = meta_bake_files.size();
  packed_bake->blob_files_num = blob_bake_files.size();

  packed_bake->meta_files = MEM_calloc_arrayN<NodesModifierBakeFile>(packed_bake->meta_files_num,
                                                                     __func__);
  packed_bake->blob_files = MEM_calloc_arrayN<NodesModifierBakeFile>(packed_bake->blob_files_num,
                                                                     __func__);

  uninitialized_copy_n(meta_bake_files.data(), meta_bake_files.size(), packed_bake->meta_files);
  uninitialized_copy_n(blob_bake_files.data(), blob_bake_files.size(), packed_bake->blob_files);

  return packed_bake;
}

bool unpack_bake_to_disk(const NodesModifierPackedBake &packed_bake,
                         const BakePath &bake_path,
                         ReportList *reports)
{
  auto unpack_file = [&](const StringRefNull directory, const NodesModifierBakeFile &bake_file) {
    char file_path[FILE_MAX];
    BLI_path_join(file_path, sizeof(file_path), directory.c_str(), bake_file.name);
    if (!BLI_file_ensure_parent_dir_exists(file_path)) {
      BKE_reportf(reports, RPT_ERROR, "Cannot ensure directory: %s", directory.c_str());
      return false;
    }
    fstream fs(file_path, std::ios::out | std::ios::binary);
    fs.write(static_cast<const char *>(bake_file.packed_file->data), bake_file.packed_file->size);
    if (fs.bad()) {
      BKE_reportf(reports, RPT_ERROR, "Cannot write file: %s", file_path);
      return false;
    }
    return true;
  };

  for (const NodesModifierBakeFile &bake_file :
       Span{packed_bake.meta_files, packed_bake.meta_files_num})
  {
    if (!unpack_file(bake_path.meta_dir, bake_file)) {
      return false;
    }
  }
  for (const NodesModifierBakeFile &bake_file :
       Span{packed_bake.blob_files, packed_bake.blob_files_num})
  {
    if (!unpack_file(bake_path.blobs_dir, bake_file)) {
      return false;
    }
  }
  return true;
}

PackGeometryNodesBakeResult pack_geometry_nodes_bake(Main &bmain,
                                                     ReportList *reports,
                                                     Object &object,
                                                     NodesModifierData &nmd,
                                                     NodesModifierBake &bake)
{
  if (bake.packed) {
    return PackGeometryNodesBakeResult::PackedAlready;
  }
  const std::optional<bake::BakePath> bake_path = get_node_bake_path(bmain, object, nmd, bake.id);
  if (!bake_path) {
    return PackGeometryNodesBakeResult::NoDataFound;
  }
  bake.packed = bake::pack_bake_from_disk(*bake_path, reports);
  if (!bake.packed) {
    return PackGeometryNodesBakeResult::NoDataFound;
  }
  nmd.runtime->cache->reset_cache(bake.id);
  bake.bake_target = NODES_MODIFIER_BAKE_TARGET_PACKED;
  DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);
  return PackGeometryNodesBakeResult::Success;
}

static bool directory_is_empty(const blender::StringRefNull path)
{
  direntry *entries = nullptr;
  const int entries_num = BLI_filelist_dir_contents(path.c_str(), &entries);
  BLI_SCOPED_DEFER([&]() { BLI_filelist_free(entries, entries_num); });
  for (const int i : IndexRange(entries_num)) {
    const direntry &entry = entries[i];
    if (FILENAME_IS_CURRPAR(entry.relname)) {
      continue;
    }
    return false;
  }
  return true;
}

static bool disk_bake_exists(const blender::bke::bake::BakePath &path)
{
  return !directory_is_empty(path.meta_dir);
}

UnpackGeometryNodesBakeResult unpack_geometry_nodes_bake(Main &bmain,
                                                         ReportList *reports,
                                                         Object &object,
                                                         NodesModifierData &nmd,
                                                         NodesModifierBake &bake,
                                                         ePF_FileStatus how)
{
  if (!bake.packed) {
    return UnpackGeometryNodesBakeResult::NoPackedData;
  }
  if (StringRef(BKE_main_blendfile_path(&bmain)).is_empty()) {
    BKE_report(reports, RPT_ERROR, "Can only unpack bake if the current .blend file is saved");
    return UnpackGeometryNodesBakeResult::BlendFileNotSaved;
  }

  DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);

  auto prepare_local_path = [&]() {
    const std::string directory = bake::get_default_node_bake_directory(
        bmain, object, nmd, bake.id);
    bake.flag |= NODES_MODIFIER_BAKE_CUSTOM_PATH;
    MEM_SAFE_FREE(bake.directory);
    bake.directory = BLI_strdup(directory.c_str());
    const char *base_path = ID_BLEND_PATH(&bmain, &object.id);
    char absolute_dir[FILE_MAX];
    STRNCPY(absolute_dir, directory.c_str());
    BLI_path_abs(absolute_dir, base_path);
    return bake::BakePath::from_single_root(absolute_dir);
  };
  auto prepare_original_path = [&]() {
    if (const std::optional<bake::BakePath> bake_path = bake::get_node_bake_path(
            bmain, object, nmd, bake.id))
    {
      return *bake_path;
    }
    return prepare_local_path();
  };
  auto delete_bake_on_disk = [&](const bake::BakePath &bake_path) {
    BLI_delete(bake_path.meta_dir.c_str(), true, true);
    BLI_delete(bake_path.blobs_dir.c_str(), true, true);
  };
  auto free_packed_bake = [&]() {
    blender::nodes_modifier_packed_bake_free(bake.packed);
    bake.packed = nullptr;
    nmd.runtime->cache->reset_cache(bake.id);
  };
  auto finalize_on_success = [&]() {
    bake.bake_target = NODES_MODIFIER_BAKE_TARGET_DISK;
    return UnpackGeometryNodesBakeResult::Success;
  };

  switch (how) {
    case PF_USE_ORIGINAL: {
      const bake::BakePath bake_path = prepare_original_path();
      if (!disk_bake_exists(bake_path)) {
        delete_bake_on_disk(bake_path);
        if (!bake::unpack_bake_to_disk(*bake.packed, bake_path, reports)) {
          return UnpackGeometryNodesBakeResult::Error;
        }
      }
      free_packed_bake();
      return finalize_on_success();
    }
    case PF_WRITE_ORIGINAL: {
      const bake::BakePath bake_path = prepare_original_path();
      delete_bake_on_disk(bake_path);
      if (!bake::unpack_bake_to_disk(*bake.packed, bake_path, reports)) {
        return UnpackGeometryNodesBakeResult::Error;
      }
      free_packed_bake();
      return finalize_on_success();
    }
    case PF_USE_LOCAL: {
      const bake::BakePath bake_path = prepare_local_path();
      if (!disk_bake_exists(bake_path)) {
        delete_bake_on_disk(bake_path);
        if (!bake::unpack_bake_to_disk(*bake.packed, bake_path, reports)) {
          return UnpackGeometryNodesBakeResult::Error;
        }
      }
      free_packed_bake();
      return finalize_on_success();
    }
    case PF_WRITE_LOCAL: {
      const bake::BakePath bake_path = prepare_local_path();
      delete_bake_on_disk(bake_path);
      if (!bake::unpack_bake_to_disk(*bake.packed, bake_path, reports)) {
        return UnpackGeometryNodesBakeResult::Error;
      }
      free_packed_bake();
      return finalize_on_success();
    }
    case PF_KEEP: {
      return finalize_on_success();
    }
    case PF_REMOVE: {
      free_packed_bake();
      return finalize_on_success();
    }
    default: {
      break;
    }
  }
  return UnpackGeometryNodesBakeResult::Error;
}

}  // namespace blender::bke::bake
