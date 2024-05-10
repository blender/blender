/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory>
#include <utility>

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_asset_types.h"
#include "DNA_space_types.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"

#include "BKE_asset.hh"
#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.h"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "MEM_guardedalloc.h"

namespace blender::bke {

static ID *asset_link_id(Main &global_main,
                         const ID_Type id_type,
                         const char *filepath,
                         const char *asset_name)
{
  /* Load asset from asset library. */
  LibraryLink_Params lapp_params{};
  lapp_params.bmain = &global_main;
  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_flag_set(lapp_context, BLO_LIBLINK_FORCE_INDIRECT, true);

  BKE_blendfile_link_append_context_library_add(lapp_context, filepath, nullptr);

  BlendfileLinkAppendContextItem *lapp_item = BKE_blendfile_link_append_context_item_add(
      lapp_context, asset_name, id_type, nullptr);
  BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, lapp_item, 0);

  BKE_blendfile_link(lapp_context, nullptr);

  ID *local_asset = BKE_blendfile_link_append_context_item_newid_get(lapp_context, lapp_item);

  BKE_blendfile_link_append_context_free(lapp_context);

  /* Verify that the name matches. It must for referencing the same asset again to work.  */
  BLI_assert(local_asset == nullptr || STREQ(local_asset->name + 2, asset_name));

  /* Tag library as being editable. */
  if (local_asset && local_asset->lib) {
    local_asset->lib->runtime.tag |= LIBRARY_ASSET_EDITABLE;

    /* Simple check, based on being a writable .asset.blend file in a user asset library. */
    if (StringRef(filepath).endswith(BLENDER_ASSET_FILE_SUFFIX) &&
        BKE_preferences_asset_library_containing_path(&U, filepath) &&
        BLI_file_is_writable(filepath))
    {
      local_asset->lib->runtime.tag |= LIBRARY_ASSET_FILE_WRITABLE;
    }
  }

  return local_asset;
}

static std::string asset_root_path_for_save(const bUserAssetLibrary &user_library,
                                            const ID_Type id_type)
{
  BLI_assert(user_library.dirpath[0] != '\0');

  char libpath[FILE_MAX];
  BLI_strncpy(libpath, user_library.dirpath, sizeof(libpath));
  BLI_path_slash_native(libpath);
  BLI_path_normalize(libpath);

  /* Capitalize folder name. Ideally this would already available in
   * the type info to work correctly with multiple words. */
  const IDTypeInfo *id_type_info = BKE_idtype_get_info_from_idcode(id_type);
  std::string name = id_type_info->name_plural;
  name[0] = BLI_toupper_ascii(name[0]);

  return std::string(libpath) + SEP + "Saved" + SEP + name;
}

static std::string asset_blendfile_path_for_save(const bUserAssetLibrary &user_library,
                                                 const StringRef base_name,
                                                 const ID_Type id_type,
                                                 ReportList &reports)
{
  std::string root_path = asset_root_path_for_save(user_library, id_type);
  BLI_assert(!root_path.empty());

  if (!BLI_dir_create_recursive(root_path.c_str())) {
    BKE_report(&reports, RPT_ERROR, "Failed to create asset library directory to save asset");
    return "";
  }

  /* Make sure filename only contains valid characters for filesystem. */
  char base_name_filesafe[FILE_MAXFILE];
  BLI_strncpy(base_name_filesafe,
              base_name.data(),
              std::min(sizeof(base_name_filesafe), size_t(base_name.size() + 1)));
  BLI_path_make_safe_filename(base_name_filesafe);

  const std::string filepath = root_path + SEP + base_name_filesafe + BLENDER_ASSET_FILE_SUFFIX;

  if (!BLI_is_file(filepath.c_str())) {
    return filepath;
  }

  /* Avoid overwriting existing file by adding number suffix. */
  for (int i = 1;; i++) {
    const std::string filepath = root_path + SEP + base_name_filesafe + "_" + std::to_string(i++) +
                                 BLENDER_ASSET_FILE_SUFFIX;
    if (!BLI_is_file((filepath.c_str()))) {
      return filepath;
    }
  }

  return "";
}

static void asset_main_create_expander(void * /*handle*/, Main * /*bmain*/, void *vid)
{
  ID *id = static_cast<ID *>(vid);

  if (id && (id->tag & LIB_TAG_DOIT) == 0) {
    id->tag |= LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT;
  }
}

static Main *asset_main_create_from_ID(Main &bmain_src, ID &id_asset, ID **id_asset_new)
{
  /* Tag asset ID and its dependencies. */
  ID *id_src;
  FOREACH_MAIN_ID_BEGIN (&bmain_src, id_src) {
    id_src->tag &= ~(LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT);
  }
  FOREACH_MAIN_ID_END;

  id_asset.tag |= LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT;

  BLO_expand_main(nullptr, &bmain_src, asset_main_create_expander);

  /* Create main and copy all tagged datablocks. */
  Main *bmain_dst = BKE_main_new();
  STRNCPY(bmain_dst->filepath, bmain_src.filepath);

  blender::bke::id::IDRemapper id_remapper;

  FOREACH_MAIN_ID_BEGIN (&bmain_src, id_src) {
    if (id_src->tag & LIB_TAG_DOIT) {
      /* Note that this will not copy Library datablocks, and all copied
       * datablocks will become local as a result. */
      ID *id_dst = BKE_id_copy_ex(bmain_dst,
                                  id_src,
                                  nullptr,
                                  LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_DEG_TAG |
                                      ((id_src == &id_asset) ? LIB_ID_COPY_ASSET_METADATA : 0));
      id_remapper.add(id_src, id_dst);
      if (id_src == &id_asset) {
        *id_asset_new = id_dst;
      }
    }
    else {
      id_remapper.add(id_src, nullptr);
    }

    id_src->tag &= ~(LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT);
  }
  FOREACH_MAIN_ID_END;

  /* Remap datablock pointers. */
  BKE_libblock_remap_multiple_raw(bmain_dst, id_remapper, ID_REMAP_SKIP_USER_CLEAR);

  /* Compute reference counts. */
  ID *id_dst;
  FOREACH_MAIN_ID_BEGIN (bmain_dst, id_dst) {
    id_dst->tag &= ~LIB_TAG_NO_USER_REFCOUNT;
  }
  FOREACH_MAIN_ID_END;
  BKE_main_id_refcount_recompute(bmain_dst, false);

  return bmain_dst;
}

static bool asset_write_in_library(Main &bmain,
                                   const ID &id_const,
                                   const StringRef name,
                                   const StringRefNull filepath,
                                   std::string &final_full_file_path,
                                   ReportList &reports)
{
  ID &id = const_cast<ID &>(id_const);

  ID *new_id = nullptr;
  Main *new_main = asset_main_create_from_ID(bmain, id, &new_id);

  std::string new_name = name;
  BKE_libblock_rename(new_main, new_id, new_name.c_str());
  id_fake_user_set(new_id);

  BlendFileWriteParams blend_file_write_params{};
  blend_file_write_params.remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE;

  BKE_packedfile_pack_all(new_main, nullptr, false);

  const int write_flags = G_FILE_COMPRESS;
  const bool success = BLO_write_file(
      new_main, filepath.c_str(), write_flags, &blend_file_write_params, &reports);

  if (success) {
    const IDTypeInfo *idtype = BKE_idtype_get_info_from_id(&id);
    final_full_file_path = std::string(filepath) + SEP + std::string(idtype->name) + SEP + name;
  }

  BKE_main_free(new_main);

  return success;
}

static void asset_reload(Main &global_main, Library *lib, ReportList &reports)
{
  /* Fill fresh main database with same datablock as before. */
  LibraryLink_Params lapp_params{};
  lapp_params.bmain = &global_main;
  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_flag_set(
      lapp_context, BLO_LIBLINK_FORCE_INDIRECT | BLO_LIBLINK_USE_PLACEHOLDERS, true);

  BKE_blendfile_link_append_context_library_add(lapp_context, lib->runtime.filepath_abs, nullptr);
  BKE_blendfile_library_relocate(lapp_context, &reports, lib, true);
  BKE_blendfile_link_append_context_free(lapp_context);

  /* Clear temporary tag from reloaction. */
  BKE_main_id_tag_all(&global_main, LIB_TAG_PRE_EXISTING, false);

  /* Recreate dependency graph to include new IDs. */
  DEG_relations_tag_update(&global_main);
}

/**
 * Public API
 */
std::optional<std::string> asset_edit_id_save_as(Main &global_main,
                                                 const ID &id,
                                                 const StringRef name,
                                                 const bUserAssetLibrary &user_library,
                                                 ReportList &reports)
{
  const std::string filepath = asset_blendfile_path_for_save(
      user_library, name, GS(id.name), reports);

  std::string final_full_asset_filepath;
  const bool success = asset_write_in_library(
      global_main, id, name, filepath, final_full_asset_filepath, reports);
  if (!success) {
    BKE_report(&reports, RPT_ERROR, "Failed to write to asset library");
    return std::nullopt;
  }

  BKE_reportf(&reports, RPT_INFO, "Saved \"%s\"", filepath.c_str());

  return final_full_asset_filepath;
}

bool asset_edit_id_save(Main &global_main, const ID &id, ReportList &reports)
{
  if (!asset_edit_id_is_editable(id)) {
    return false;
  }

  std::string final_full_asset_filepath;
  const bool success = asset_write_in_library(global_main,
                                              id,
                                              id.name + 2,
                                              id.lib->runtime.filepath_abs,
                                              final_full_asset_filepath,
                                              reports);

  if (!success) {
    BKE_report(&reports, RPT_ERROR, "Failed to write to asset library");
    return false;
  }

  return true;
}

bool asset_edit_id_revert(Main &global_main, ID &id, ReportList &reports)
{
  if (!asset_edit_id_is_editable(id)) {
    return false;
  }

  /* Reload entire main, including texture dependencies. This relies on there
   * being only a single asset per blend file. */
  asset_reload(global_main, id.lib, reports);

  return true;
}

bool asset_edit_id_delete(Main &global_main, ID &id, ReportList &reports)
{
  if (!asset_edit_id_is_editable(id)) {
    return false;
  }

  if (BLI_delete(id.lib->runtime.filepath_abs, false, false) != 0) {
    BKE_report(&reports, RPT_ERROR, "Failed to delete asset library file");
    return false;
  }

  BKE_id_delete(&global_main, &id);

  return true;
}

ID *asset_edit_id_from_weak_reference(Main &global_main,
                                      const ID_Type id_type,
                                      const AssetWeakReference &weak_ref)
{
  /* Don't do this in file load. */
  BLI_assert(!global_main.is_locked_for_linking);

  char asset_full_path_buffer[FILE_MAX_LIBEXTRA];
  char *asset_lib_path, *asset_group, *asset_name;

  AS_asset_full_path_explode_from_weak_ref(
      &weak_ref, asset_full_path_buffer, &asset_lib_path, &asset_group, &asset_name);
  if (asset_lib_path == nullptr && asset_group == nullptr && asset_name == nullptr) {
    return nullptr;
  }

  BLI_assert(asset_name != nullptr);

  /* Test if asset has been loaded already. */
  ID *local_asset = BKE_libblock_find_name_and_library_filepath(
      &global_main, id_type, asset_name, asset_lib_path);
  if (local_asset) {
    return local_asset;
  }

  /* If weak reference resolves to a null library path, assume we are in local asset case. */
  return asset_link_id(global_main, id_type, asset_lib_path, asset_name);
}

std::optional<AssetWeakReference> asset_edit_weak_reference_from_id(ID &id)
{
  if (!asset_edit_id_is_editable(id)) {
    return std::nullopt;
  }

  bUserAssetLibrary *user_library = BKE_preferences_asset_library_containing_path(
      &U, id.lib->runtime.filepath_abs);

  const short idcode = GS(id.name);

  if (user_library && user_library->dirpath[0]) {
    return asset_weak_reference_for_user_library(
        *user_library, idcode, id.name + 2, id.lib->runtime.filepath_abs);
  }
  else {
    return asset_weak_reference_for_essentials(idcode, id.name + 2, id.lib->runtime.filepath_abs);
  }
}

bool asset_edit_id_is_editable(const ID &id)
{
  return (id.lib && (id.lib->runtime.tag & LIBRARY_ASSET_EDITABLE));
}

bool asset_edit_id_is_writable(const ID &id)
{
  return asset_edit_id_is_editable(id) && (id.lib->runtime.tag & LIBRARY_ASSET_FILE_WRITABLE);
}

}  // namespace blender::bke
