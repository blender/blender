/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_scene_delegate.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_appdir.h"

#include "DEG_depsgraph_query.h"

#include "usd.h"
#include "usd.hh"

namespace blender::io::hydra {

USDSceneDelegate::USDSceneDelegate(pxr::HdRenderIndex *render_index,
                                   pxr::SdfPath const &delegate_id)
    : render_index_(render_index), delegate_id_(delegate_id)
{
  /* Temporary directory to write any additional files to, like image or VDB files. */
  char unique_name[FILE_MAXFILE];
  SNPRINTF(unique_name, "%p", this);

  char dir_path[FILE_MAX];
  BLI_path_join(
      dir_path, sizeof(dir_path), BKE_tempdir_session(), "usd_scene_delegate", unique_name);
  BLI_dir_create_recursive(dir_path);

  char file_path[FILE_MAX];
  BLI_path_join(file_path, sizeof(file_path), dir_path, "scene.usdc");

  temp_dir_ = dir_path;
  temp_file_ = file_path;
}

USDSceneDelegate::~USDSceneDelegate()
{
  BLI_delete(temp_dir_.c_str(), true, true);
}

void USDSceneDelegate::populate(Depsgraph *depsgraph)
{
  USDExportParams params;
  params.use_instancing = true;
  params.relative_paths = false;  /* Unnecessary. */
  params.export_textures = false; /* Don't copy all textures, is slow. */
  params.evaluation_mode = DEG_get_mode(depsgraph);

  /* Create clean directory for export. */
  BLI_delete(temp_dir_.c_str(), true, true);
  BLI_dir_create_recursive(temp_dir_.c_str());

  /* Free previous delegate and stage first to save memory. */
  delegate_.reset();
  stage_.Reset();

  /* Convert depsgraph to stage + additional file in temp directory. */
  stage_ = io::usd::export_to_stage(params, depsgraph, temp_file_.c_str());
  delegate_ = std::make_unique<pxr::UsdImagingDelegate>(render_index_, delegate_id_);
  delegate_->Populate(stage_->GetPseudoRoot());
}

}  // namespace blender::io::hydra
