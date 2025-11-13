/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DNA_ID.h"
#include "DNA_asset_types.h"
#include "DNA_space_enums.h"
#include "DNA_userdef_types.h"

#include "AS_asset_library.hh"

#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

namespace blender::bke {

static ID *asset_link_id(Main &global_main,
                         const ID_Type id_type,
                         const char *filepath,
                         const char *asset_name,
                         ReportList *reports = nullptr)
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

  BKE_blendfile_link_append_context_init_done(lapp_context);

  BKE_blendfile_link(lapp_context, reports);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  ID *local_asset = BKE_blendfile_link_append_context_item_newid_get(lapp_context, lapp_item);

  BKE_blendfile_link_append_context_free(lapp_context);

  /* Verify that the name matches. It must for referencing the same asset again to work. */
  BLI_assert(local_asset == nullptr || STREQ(local_asset->name + 2, asset_name));

  /* Tag library as being editable. */
  if (local_asset && local_asset->lib) {
    local_asset->lib->runtime->tag |= LIBRARY_ASSET_EDITABLE;

    if ((local_asset->lib->runtime->tag & LIBRARY_IS_ASSET_EDIT_FILE) &&
        StringRef(filepath).endswith(BLENDER_ASSET_FILE_SUFFIX) &&
        BKE_preferences_asset_library_containing_path(&U, filepath) &&
        BLI_file_is_writable(filepath))
    {
      local_asset->lib->runtime->tag |= LIBRARY_ASSET_FILE_WRITABLE;
    }
  }

  return local_asset;
}

static std::string asset_root_path_for_save(const bUserAssetLibrary &user_library,
                                            const ID_Type id_type)
{
  BLI_assert(user_library.dirpath[0] != '\0');

  char libpath[FILE_MAX];
  STRNCPY(libpath, user_library.dirpath);
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

  /* Make sure filename only contains valid characters for file-system. */
  char base_name_filesafe[FILE_MAXFILE];
  BLI_strncpy(base_name_filesafe,
              base_name.data(),
              std::min(sizeof(base_name_filesafe), size_t(base_name.size() + 1)));
  BLI_path_make_safe_filename(base_name_filesafe);

  /* FIXME: MAX_ID_NAME & FILE_MAXFILE
   *
   * This already does not respect the FILE_MAXFILE max length of filenames for the final filepath
   * it seems?
   */
  {
    const std::string filepath = root_path + SEP + base_name_filesafe + BLENDER_ASSET_FILE_SUFFIX;
    if (!BLI_is_file(filepath.c_str())) {
      return filepath;
    }
  }

  /* Avoid overwriting existing file by adding number suffix. */
  for (int i = 1;; i++) {
    const std::string filepath = root_path + SEP + base_name_filesafe + "_" + std::to_string(i++) +
                                 BLENDER_ASSET_FILE_SUFFIX;
    if (!BLI_is_file(filepath.c_str())) {
      return filepath;
    }
  }

  return "";
}

static bool asset_write_in_library(Main &bmain,
                                   const ID &id_const,
                                   const StringRef name,
                                   const StringRefNull filepath,
                                   std::string &final_full_file_path,
                                   ReportList &reports)
{
  using namespace blender::bke::blendfile;

  ID &id = const_cast<ID &>(id_const);

  /* This is not expected to ever happen currently from this codepath. */
  BLI_assert(!ID_IS_PACKED(&id));

  PartialWriteContext lib_write_ctx{bmain};
  ID *new_id = lib_write_ctx.id_add(&id,
                                    {(PartialWriteContext::IDAddOperations::MAKE_LOCAL |
                                      PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                                      PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES)});
  if (!new_id) {
    BKE_reportf(&reports,
                RPT_ERROR,
                "Could not create a copy of ID '%s' to write it in the library",
                id.name);
    return false;
  }

  std::string new_name = name;
  BKE_libblock_rename(lib_write_ctx.bmain, *new_id, new_name);

  BKE_packedfile_pack_all(&lib_write_ctx.bmain, nullptr, false);
  lib_write_ctx.bmain.is_asset_edit_file = true;

  const int write_flags = G_FILE_COMPRESS | G_FILE_ASSET_EDIT_FILE;
  const int remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE;
  const bool success = lib_write_ctx.write(filepath.c_str(), write_flags, remap_mode, reports);

  if (success) {
    const IDTypeInfo *idtype = BKE_idtype_get_info_from_id(&id);
    final_full_file_path = std::string(filepath) + SEP + std::string(idtype->name) + SEP + name;
  }

  return success;
}

static ID *asset_reload(Main &global_main, ID &id, ReportList *reports)
{
  BLI_assert(ID_IS_LINKED(&id));

  const std::string name = BKE_id_name(id);
  const std::string filepath = id.lib->runtime->filepath_abs;
  const ID_Type id_type = GS(id.name);

  /* TODO: There's no API to reload a single data block (and its dependencies) yet. For now
   * deleting the brush and re-linking it is the best way to get reloading to work. */
  BKE_id_delete(&global_main, &id);
  ID *new_id = asset_link_id(global_main, id_type, filepath.c_str(), name.c_str(), reports);

  /* Recreate dependency graph to include new IDs. */
  DEG_relations_tag_update(&global_main);

  return new_id;
}

static AssetWeakReference asset_weak_reference_for_user_library(
    const bUserAssetLibrary &user_library,
    const short idcode,
    const char *idname,
    const char *filepath)
{
  AssetWeakReference weak_ref;
  weak_ref.asset_library_type = ASSET_LIBRARY_CUSTOM;
  weak_ref.asset_library_identifier = BLI_strdup(user_library.name);

  /* BLI_path_rel requires a trailing slash. */
  char user_library_dirpath[FILE_MAX];
  STRNCPY(user_library_dirpath, user_library.dirpath);
  BLI_path_slash_ensure(user_library_dirpath, sizeof(user_library_dirpath));

  char relative_filepath[FILE_MAX];
  STRNCPY(relative_filepath, filepath);
  BLI_path_rel(relative_filepath, user_library_dirpath);
  const char *asset_blend_path = relative_filepath + 2; /* Strip out // prefix. */

  weak_ref.relative_asset_identifier = BLI_sprintfN(
      "%s/%s/%s", asset_blend_path, BKE_idtype_idcode_to_name(idcode), idname);

  return weak_ref;
}

static AssetWeakReference asset_weak_reference_for_essentials(const short idcode,
                                                              const char *idname,
                                                              const char *filepath)
{
  AssetWeakReference weak_ref;
  weak_ref.asset_library_type = ASSET_LIBRARY_ESSENTIALS;
  weak_ref.relative_asset_identifier = BLI_sprintfN("%s/%s/%s/%s",
                                                    BKE_idtype_idcode_to_name_plural(idcode),
                                                    BLI_path_basename(filepath),
                                                    BKE_idtype_idcode_to_name(idcode),
                                                    idname);

  return weak_ref;
}

std::optional<std::string> asset_edit_id_save_as(Main &global_main,
                                                 const ID &id,
                                                 const StringRefNull name,
                                                 const bUserAssetLibrary &user_library,
                                                 AssetWeakReference &r_weak_ref,
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

  r_weak_ref = asset_weak_reference_for_user_library(
      user_library, GS(id.name), name.c_str(), filepath.c_str());

  BKE_reportf(&reports, RPT_INFO, "Saved \"%s\"", filepath.c_str());

  return final_full_asset_filepath;
}

bool asset_edit_id_save(Main &global_main, const ID &id, ReportList &reports)
{
  if (!asset_edit_id_is_writable(id)) {
    return false;
  }

  std::string final_full_asset_filepath;
  const bool success = asset_write_in_library(global_main,
                                              id,
                                              id.name + 2,
                                              id.lib->runtime->filepath_abs,
                                              final_full_asset_filepath,
                                              reports);

  if (!success) {
    BKE_report(&reports, RPT_ERROR, "Failed to write to asset library");
    return false;
  }

  return true;
}

ID *asset_edit_id_revert(Main &global_main, ID &id, ReportList &reports)
{
  if (!asset_edit_id_is_editable(id)) {
    return nullptr;
  }

  return asset_reload(global_main, id, &reports);
}

bool asset_edit_id_delete(Main &global_main, ID &id, ReportList &reports)
{
  if (asset_edit_id_is_writable(id)) {
    if (BLI_delete(id.lib->runtime->filepath_abs, false, false) != 0) {
      BKE_report(&reports, RPT_ERROR, "Failed to delete asset library file");
      return false;
    }
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

  /* If this is the same file as we have open, use local datablock. */
  if (asset_lib_path && STREQ(asset_lib_path, global_main.filepath)) {
    asset_lib_path = nullptr;
  }

  BLI_assert(asset_name != nullptr);

  /* Test if asset has been loaded already. */
  ID *local_asset = BKE_libblock_find_name_and_library_filepath(
      &global_main, id_type, asset_name, asset_lib_path);
  if (local_asset) {
    return local_asset;
  }

  /* Try linking in the required file. */
  if (asset_lib_path == nullptr) {
    return nullptr;
  }

  return asset_link_id(global_main, id_type, asset_lib_path, asset_name);
}

std::optional<AssetWeakReference> asset_edit_weak_reference_from_id(const ID &id)
{
  /* Brush is local to the file. */
  if (!id.lib) {
    AssetWeakReference weak_ref;

    weak_ref.asset_library_type = eAssetLibraryType::ASSET_LIBRARY_LOCAL;
    weak_ref.relative_asset_identifier = BLI_sprintfN(
        "%s/%s", BKE_idtype_idcode_to_name(GS(id.name)), id.name + 2);

    return weak_ref;
  }

  if (!asset_edit_id_is_editable(id)) {
    return std::nullopt;
  }

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_containing_path(
      &U, id.lib->runtime->filepath_abs);

  const short idcode = GS(id.name);

  if (user_library && user_library->dirpath[0]) {
    return asset_weak_reference_for_user_library(
        *user_library, idcode, id.name + 2, id.lib->runtime->filepath_abs);
  }

  return asset_weak_reference_for_essentials(idcode, id.name + 2, id.lib->runtime->filepath_abs);
}

bool asset_edit_id_is_editable(const ID &id)
{
  return (id.lib && (id.lib->runtime->tag & LIBRARY_ASSET_EDITABLE));
}

bool asset_edit_id_is_writable(const ID &id)
{
  return asset_edit_id_is_editable(id) && (id.lib->runtime->tag & LIBRARY_ASSET_FILE_WRITABLE);
}

ID *asset_edit_id_find_local(Main &global_main, ID &id)
{
  if (!asset_edit_id_is_editable(id)) {
    return &id;
  }

  return BKE_main_library_weak_reference_find(&global_main, id.lib->filepath, id.name);
}

ID *asset_edit_id_ensure_local(Main &global_main, ID &id)
{
  ID *local_id = asset_edit_id_find_local(global_main, id);
  if (local_id) {
    return local_id;
  }

  /* Make local and create weak library reference for reuse. */
  BKE_lib_id_make_local(&global_main,
                        &id,
                        LIB_ID_MAKELOCAL_FORCE_COPY | LIB_ID_MAKELOCAL_INDIRECT |
                            LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR);
  BLI_assert(id.newid != nullptr);
  BKE_main_library_weak_reference_add(id.newid, id.lib->filepath, id.name);

  return id.newid;
}

}  // namespace blender::bke
