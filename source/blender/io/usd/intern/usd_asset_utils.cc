/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_asset_utils.hh"
#include "usd.hh"

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/packageUtils.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/writableAsset.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/stage.h>

#include "BKE_appdir.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "WM_api.hh"

#include <string_view>

namespace blender::io::usd {

constexpr char UDIM_PATTERN[] = "<UDIM>";
constexpr char UDIM_PATTERN2[] = "%3CUDIM%3E";

/* Maximum range of UDIM tiles, per the
 * UsdPreviewSurface specifications.  See
 * https://graphics.pixar.com/usd/release/spec_usdpreviewsurface.html#texture-reader
 */
constexpr int UDIM_START_TILE = 1001;
constexpr int UDIM_END_TILE = 1100;

/**
 * The following is copied from `_SplitUdimPattern()` in
 * USD library source file `materialParamsUtils.cpp`.
 * Split a UDIM file path such as `/someDir/myFile.<UDIM>.exr` into a
 * prefix `/someDir/myFile.` and suffix `.exr`.
 */
static std::pair<std::string, std::string> split_udim_pattern(const std::string &path)
{
  std::string_view patterns[]{UDIM_PATTERN, UDIM_PATTERN2};
  for (const std::string_view pattern : patterns) {
    const std::string::size_type pos = path.find(pattern);
    if (pos != std::string::npos) {
      return {path.substr(0, pos), path.substr(pos + pattern.size())};
    }
  }

  return {};
}

/* Return the asset file base name, with special handling of
 * package relative paths. */
static std::string get_asset_base_name(const std::string &src_path, ReportList *reports)
{
  char base_name[FILE_MAXFILE];

  if (pxr::ArIsPackageRelativePath(src_path)) {
    std::pair<std::string, std::string> split = pxr::ArSplitPackageRelativePathInner(src_path);
    if (split.second.empty()) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "%s: Couldn't determine package-relative file name from path %s",
                  __func__,
                  src_path.c_str());
      return src_path;
    }
    BLI_path_split_file_part(split.second.c_str(), base_name, sizeof(base_name));
  }
  else {
    BLI_path_split_file_part(src_path.c_str(), base_name, sizeof(base_name));
  }

  return base_name;
}

/* Copy an asset to a destination directory. */
static std::string copy_asset_to_directory(const std::string &src_path,
                                           const char *dest_dir_path,
                                           eUSDTexNameCollisionMode name_collision_mode,
                                           ReportList *reports)
{
  std::string base_name = get_asset_base_name(src_path, reports);

  char dest_file_path[FILE_MAX];
  BLI_path_join(dest_file_path, sizeof(dest_file_path), dest_dir_path, base_name.c_str());
  BLI_path_normalize(dest_file_path);

  if (name_collision_mode == USD_TEX_NAME_COLLISION_USE_EXISTING && BLI_is_file(dest_file_path)) {
    return dest_file_path;
  }

  if (!copy_asset(src_path, dest_file_path, name_collision_mode, reports)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "%s: Couldn't copy file %s to %s",
                __func__,
                src_path.c_str(),
                dest_file_path);
    return src_path;
  }

  return dest_file_path;
}

static std::string copy_udim_asset_to_directory(const std::string &src_path,
                                                const char *dest_dir_path,
                                                eUSDTexNameCollisionMode name_collision_mode,
                                                ReportList *reports)
{
  /* Get prefix and suffix from udim pattern. */
  std::pair<std::string, std::string> splitPath = split_udim_pattern(src_path);
  if (splitPath.first.empty() || splitPath.second.empty()) {
    BKE_reportf(
        reports, RPT_ERROR, "%s: Couldn't split UDIM pattern %s", __func__, src_path.c_str());
    return src_path;
  }

  /* Copy the individual UDIM tiles.  Since there is currently no way to query the contents
   * of a directory using the USD resolver, we must take a brute force approach.  We iterate
   * over the allowed range of tile indices and copy any tiles that exist.  The USDPreviewSurface
   * specification stipulates "a maximum of ten tiles in the U direction" and that
   * "the tiles must be within the range [1001, 1100] (as of specification version 2.5)". See
   * https://graphics.pixar.com/usd/release/spec_usdpreviewsurface.html#texture-reader
   */
  for (int i = UDIM_START_TILE; i <= UDIM_END_TILE; ++i) {
    const std::string src_udim = splitPath.first + std::to_string(i) + splitPath.second;
    if (asset_exists(src_udim)) {
      copy_asset_to_directory(src_udim, dest_dir_path, name_collision_mode, reports);
    }
  }

  const std::string src_file_name = get_asset_base_name(src_path, reports);
  char ret_udim_path[FILE_MAX];
  BLI_path_join(ret_udim_path, sizeof(ret_udim_path), dest_dir_path, src_file_name.c_str());

  /* Blender only recognizes the <UDIM> pattern, not the
   * alternative UDIM_PATTERN2, so we make sure the returned
   * path has the former. */
  splitPath = split_udim_pattern(ret_udim_path);
  if (splitPath.first.empty() || splitPath.second.empty()) {
    BKE_reportf(reports, RPT_ERROR, "%s: Couldn't split UDIM pattern %s", __func__, ret_udim_path);
    return ret_udim_path;
  }

  return splitPath.first + UDIM_PATTERN + splitPath.second;
}

bool copy_asset(const std::string &src,
                const std::string &dst,
                eUSDTexNameCollisionMode name_collision_mode,
                ReportList *reports)
{
  const pxr::ArResolver &ar = pxr::ArGetResolver();

  if (name_collision_mode != USD_TEX_NAME_COLLISION_OVERWRITE) {
    if (!ar.Resolve(dst).IsEmpty()) {
      /* The asset exists, so this is a no-op. */
      BKE_reportf(
          reports, RPT_INFO, "%s: Will not overwrite existing asset %s", __func__, dst.c_str());
      return true;
    }
  }

  pxr::ArResolvedPath src_path = ar.Resolve(src);

  if (src_path.IsEmpty()) {
    BKE_reportf(reports, RPT_ERROR, "%s: Cannot resolve path %s", __func__, src.c_str());
    return false;
  }

  pxr::ArResolvedPath dst_path = ar.ResolveForNewAsset(dst);

  if (dst_path.IsEmpty()) {
    BKE_reportf(
        reports, RPT_ERROR, "%s: Cannot resolve path %s for writing", __func__, dst.c_str());
    return false;
  }

  if (src_path == dst_path) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot copy %s. The source and destination paths are the same",
                __func__,
                src_path.GetPathString().c_str());
    return false;
  }

  std::string why_not;
  if (!ar.CanWriteAssetToPath(dst_path, &why_not)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot write to asset %s: %s",
                __func__,
                dst_path.GetPathString().c_str(),
                why_not.c_str());
    return false;
  }

  std::shared_ptr<pxr::ArAsset> src_asset = ar.OpenAsset(src_path);
  if (!src_asset) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot open source asset %s",
                __func__,
                src_path.GetPathString().c_str());
    return false;
  }

  const size_t size = src_asset->GetSize();

  if (size == 0) {
    BKE_reportf(reports,
                RPT_WARNING,
                "%s: Will not copy zero size source asset %s",
                __func__,
                src_path.GetPathString().c_str());
    return false;
  }

  std::shared_ptr<const char> buf = src_asset->GetBuffer();

  if (!buf) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Null buffer for source asset %s",
                __func__,
                src_path.GetPathString().c_str());
    return false;
  }

  std::shared_ptr<pxr::ArWritableAsset> dst_asset = ar.OpenAssetForWrite(
      dst_path, pxr::ArResolver::WriteMode::Replace);
  if (!dst_asset) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot open destination asset %s for writing",
                __func__,
                src_path.GetPathString().c_str());
    return false;
  }

  size_t bytes_written = dst_asset->Write(src_asset->GetBuffer().get(), src_asset->GetSize(), 0);

  if (bytes_written == 0) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Error writing to destination asset %s",
                __func__,
                dst_path.GetPathString().c_str());
  }

  if (!dst_asset->Close()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Couldn't close destination asset %s",
                __func__,
                dst_path.GetPathString().c_str());
    return false;
  }

  return bytes_written > 0;
}

bool asset_exists(const std::string &path)
{
  return !pxr::ArGetResolver().Resolve(path).IsEmpty();
}

std::string import_asset(const std::string &src,
                         const char *import_dir,
                         eUSDTexNameCollisionMode name_collision_mode,
                         ReportList *reports)
{
  if (import_dir[0] == '\0') {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Texture import directory path empty, couldn't import %s",
                __func__,
                src.c_str());
    return src;
  }

  char dest_dir_path[FILE_MAXDIR];
  STRNCPY(dest_dir_path, import_dir);

  if (BLI_path_is_rel(import_dir)) {
    const char *basepath = BKE_main_blendfile_path_from_global();
    if (basepath[0] == '\0') {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s: import directory is relative "
                  "but the blend file path is empty. "
                  "Please save the blend file before importing the USD "
                  "or provide an absolute import directory path. "
                  "Cannot import %s",
                  __func__,
                  src.c_str());
      return src;
    }
    char path_temp[FILE_MAX];
    STRNCPY(path_temp, dest_dir_path);
    BLI_path_abs(path_temp, basepath);
    STRNCPY(dest_dir_path, path_temp);
  }

  BLI_path_normalize(dest_dir_path);

  if (!BLI_dir_create_recursive(dest_dir_path)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Couldn't create texture import directory %s",
                __func__,
                dest_dir_path);
    return src;
  }

  if (is_udim_path(src)) {
    return copy_udim_asset_to_directory(src, dest_dir_path, name_collision_mode, reports);
  }

  return copy_asset_to_directory(src, dest_dir_path, name_collision_mode, reports);
}

/**
 * Returns true if the parent directory of the given path exists on the
 * file system.
 *
 * \param path: input file path
 * \return true if the parent directory exists
 */
static bool parent_dir_exists_on_file_system(const std::string &path)
{
  char dir_path[FILE_MAX];
  BLI_path_split_dir_part(path.c_str(), dir_path, FILE_MAX);
  return BLI_is_dir(dir_path);
}

bool is_udim_path(const std::string &path)
{
  return path.find(UDIM_PATTERN) != std::string::npos ||
         path.find(UDIM_PATTERN2) != std::string::npos;
}

std::string get_export_textures_dir(const pxr::UsdStageRefPtr stage)
{
  pxr::SdfLayerHandle layer = stage->GetRootLayer();

  if (layer->IsAnonymous()) {
    WM_global_reportf(RPT_WARNING,
                      "%s: Cannot generate a textures directory path for anonymous stage",
                      __func__);
    return "";
  }

  const pxr::ArResolvedPath &stage_path = layer->GetResolvedPath();

  if (stage_path.empty()) {
    WM_global_reportf(RPT_WARNING, "%s: Cannot get resolved path for stage", __func__);
    return "";
  }

  const pxr::ArResolver &ar = pxr::ArGetResolver();

  /* Resolve the `./textures` relative path, with the stage path as an anchor. */
  std::string textures_dir = ar.CreateIdentifierForNewAsset("./textures", stage_path);

  /* If parent of the stage path exists as a file system directory, try to create the
   * textures directory. */
  if (parent_dir_exists_on_file_system(stage_path.GetPathString())) {
    BLI_dir_create_recursive(textures_dir.c_str());
  }

  return textures_dir;
}

bool should_import_asset(const std::string &path)
{
  if (path.empty()) {
    return false;
  }

  if (BLI_path_is_rel(path.c_str())) {
    return false;
  }

  if (pxr::ArIsPackageRelativePath(path)) {
    return true;
  }

  if (is_udim_path(path) && parent_dir_exists_on_file_system(path)) {
    return false;
  }

  return !BLI_is_file(path.c_str()) && asset_exists(path);
}

bool paths_equal(const std::string &path1, const std::string &path2)
{
  BLI_assert_msg(!BLI_path_is_rel(path1.c_str()) && !BLI_path_is_rel(path2.c_str()),
                 "Paths arguments must be absolute");

  const pxr::ArResolver &ar = pxr::ArGetResolver();
  return ar.ResolveForNewAsset(path1) == ar.ResolveForNewAsset(path2);
}

const char *temp_textures_dir()
{
  static bool inited = false;

  static char temp_dir[FILE_MAXDIR] = {'\0'};

  if (!inited) {
    BLI_path_join(temp_dir, sizeof(temp_dir), BKE_tempdir_session(), "usd_textures_tmp", SEP_STR);
    inited = true;
  }

  return temp_dir;
}

bool write_to_path(const void *data, size_t size, const std::string &path, ReportList *reports)
{
  BLI_assert(data);
  if (size == 0) {
    return false;
  }

  const pxr::ArResolver &ar = pxr::ArGetResolver();
  pxr::ArResolvedPath resolved_path = ar.ResolveForNewAsset(path);

  if (resolved_path.IsEmpty()) {
    BKE_reportf(reports, RPT_ERROR, "Cannot resolve path %s for writing", path.c_str());
    return false;
  }

  std::string why_not;
  if (!ar.CanWriteAssetToPath(resolved_path, &why_not)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot write to asset %s: %s",
                resolved_path.GetPathString().c_str(),
                why_not.c_str());
    return false;
  }

  std::shared_ptr<pxr::ArWritableAsset> dst_asset = ar.OpenAssetForWrite(
      resolved_path, pxr::ArResolver::WriteMode::Replace);
  if (!dst_asset) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot open destination asset %s for writing",
                resolved_path.GetPathString().c_str());
    return false;
  }

  size_t bytes_written = dst_asset->Write(data, size, 0);

  if (bytes_written == 0) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Error writing to destination asset %s",
                resolved_path.GetPathString().c_str());
  }

  if (!dst_asset->Close()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Couldn't close destination asset %s",
                resolved_path.GetPathString().c_str());
    return false;
  }

  return bytes_written > 0;
}

void ensure_usd_source_path_prop(const std::string &path, ID *id)
{
  if (!id || path.empty()) {
    return;
  }

  if (pxr::ArIsPackageRelativePath(path)) {
    /* Don't record package-relative paths (e.g., images in USDZ
     * archives). */
    return;
  }

  IDProperty *idgroup = IDP_EnsureProperties(id);

  if (!idgroup) {
    return;
  }

  const StringRef prop_name = "usd_source_path";

  if (IDP_GetPropertyFromGroup(idgroup, prop_name)) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.string.str = path.c_str();
  /* Note length includes null terminator. */
  val.string.len = path.size() + 1;
  val.string.subtype = IDP_STRING_SUB_UTF8;

  IDProperty *prop = IDP_New(IDP_STRING, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

std::string get_usd_source_path(ID *id)
{
  if (!id) {
    return "";
  }

  const IDProperty *idgroup = IDP_EnsureProperties(id);
  if (!idgroup) {
    return "";
  }

  const StringRef prop_name = "usd_source_path";
  const IDProperty *prop = IDP_GetPropertyFromGroup(idgroup, prop_name);
  if (!prop) {
    return "";
  }

  return static_cast<const char *>(prop->data.pointer);
}

std::string get_relative_path(const std::string &path, const std::string &anchor)
{
  if (path.empty() || anchor.empty()) {
    return path;
  }

  if (path == anchor) {
    return path;
  }

  if (BLI_path_is_rel(path.c_str())) {
    return path;
  }

  if (pxr::ArIsPackageRelativePath(path)) {
    return path;
  }

  if (BLI_is_file(path.c_str()) && BLI_is_file(anchor.c_str())) {
    /* Treat the paths as standard files. */
    char rel_path[FILE_MAX];
    STRNCPY(rel_path, path.c_str());
    BLI_path_rel(rel_path, anchor.c_str());
    if (!BLI_path_is_rel(rel_path)) {
      return path;
    }
    BLI_string_replace_char(rel_path, '\\', '/');
    return rel_path + 2;
  }

  /* If we got here, the paths may be URIs or files on the file system. */

  /* We don't have a library to compute relative paths for URIs
   * so we use the standard file-system calls to do so. This
   * may not work for all URIs in theory, but is probably sufficient
   * for the subset of URIs we are likely to encounter in practice
   * currently.
   * TODO(makowalski): provide better utilities for this. */

  const pxr::ArResolver &ar = pxr::ArGetResolver();

  std::string resolved_path = ar.Resolve(path);
  std::string resolved_anchor = ar.Resolve(anchor);

  if (resolved_path.empty() || resolved_anchor.empty()) {
    return path;
  }

  std::string prefix = pxr::TfStringGetCommonPrefix(path, anchor);
  if (prefix.empty()) {
    return path;
  }

  std::replace(prefix.begin(), prefix.end(), '\\', '/');

  size_t last_slash_pos = prefix.find_last_of('/');
  if (last_slash_pos == std::string::npos) {
    /* Unexpected: The prefix doesn't contain a slash,
     * so this was not an absolute path. */
    return path;
  }

  /* Replace the common prefix up to the last slash with
   * a fake root directory to allow computing the relative path
   * excluding the URI.  We omit the URI because it might not
   * be handled correctly by the standard file-system path computations. */
  resolved_path = "/root" + resolved_path.substr(last_slash_pos);
  resolved_anchor = "/root" + resolved_anchor.substr(last_slash_pos);

  char anchor_parent_dir[FILE_MAX];
  BLI_path_split_dir_part(resolved_anchor.c_str(), anchor_parent_dir, FILE_MAX);

  if (anchor_parent_dir[0] == '\0') {
    return path;
  }

  char result_path[FILE_MAX];
  STRNCPY(result_path, resolved_path.c_str());
  BLI_path_rel(result_path, anchor_parent_dir);

  if (BLI_path_is_rel(result_path)) {
    /* Strip the Blender relative path marker, and set paths to Unix-style. */
    BLI_string_replace_char(result_path, '\\', '/');
    return std::string(result_path + 2);
  }

  return path;
}

void USD_path_abs(char *path, const char *basepath, bool for_import)
{
  if (!BLI_path_is_rel(path)) {
    pxr::ArResolvedPath resolved_path = for_import ? pxr::ArGetResolver().Resolve(path) :
                                                     pxr::ArGetResolver().ResolveForNewAsset(path);

    const std::string &path_str = resolved_path.GetPathString();

    if (!path_str.empty()) {
      if (path_str.length() < FILE_MAX) {
        BLI_strncpy(path, path_str.c_str(), FILE_MAX);
        return;
      }
      WM_global_reportf(RPT_ERROR,
                        "In %s: resolved path %s exceeds path buffer length.",
                        __func__,
                        path_str.c_str());
    }
  }

  /* If we got here, the path couldn't be resolved by the ArResolver, so we
   * fall back on the standard Blender absolute path resolution. */
  BLI_path_abs(path, basepath);
}

}  // namespace blender::io::usd
