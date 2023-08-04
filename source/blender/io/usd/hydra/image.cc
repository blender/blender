/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "image.h"

#include <pxr/imaging/hio/imageRegistry.h>

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "BKE_appdir.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_image_save.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "hydra_scene_delegate.h"

namespace blender::io::hydra {

static std::string get_cache_file(const std::string &file_name, bool mkdir = true)
{
  char dir_path[FILE_MAX];
  BLI_path_join(dir_path, sizeof(dir_path), BKE_tempdir_session(), "hydra", "image_cache");
  if (mkdir) {
    BLI_dir_create_recursive(dir_path);
  }

  char file_path[FILE_MAX];
  BLI_path_join(file_path, sizeof(file_path), dir_path, file_name.c_str());
  return file_path;
}

static std::string cache_image_file(
    Main *bmain, Scene *scene, Image *image, ImageUser *iuser, bool check_exist)
{
  std::string file_path;
  ImageSaveOptions opts;
  if (BKE_image_save_options_init(&opts, bmain, scene, image, iuser, false, false)) {
    char file_name[32];
    const char *r_ext = BLI_path_extension_or_end(image->id.name);
    if (!pxr::HioImageRegistry::GetInstance().IsSupportedImageFile(image->id.name)) {
      BKE_image_path_ext_from_imformat(&scene->r.im_format, &r_ext);
      opts.im_format = scene->r.im_format;
    }

    snprintf(file_name, sizeof(file_name), "img_%p%s", image, r_ext);

    file_path = get_cache_file(file_name);
    if (check_exist && BLI_exists(file_path.c_str())) {
      return file_path;
    }

    opts.save_copy = true;
    STRNCPY(opts.filepath, file_path.c_str());
    if (BKE_image_save(nullptr, bmain, image, iuser, &opts)) {
      CLOG_INFO(LOG_HYDRA_SCENE, 1, "%s -> %s", image->id.name, file_path.c_str());
    }
    else {
      CLOG_ERROR(LOG_HYDRA_SCENE, "Can't save %s", file_path.c_str());
      file_path = "";
    }
  }
  BKE_image_save_options_free(&opts);
  return file_path;
}

std::string cache_or_get_image_file(Main *bmain, Scene *scene, Image *image, ImageUser *iuser)
{
  std::string file_path;
  if (image->source == IMA_SRC_GENERATED) {
    file_path = cache_image_file(bmain, scene, image, iuser, false);
  }
  else if (BKE_image_has_packedfile(image)) {
    file_path = cache_image_file(bmain, scene, image, iuser, true);
  }
  else {
    char str[FILE_MAX];
    BKE_image_user_file_path_ex(bmain, iuser, image, str, false, true);
    file_path = str;

    if (!pxr::HioImageRegistry::GetInstance().IsSupportedImageFile(file_path)) {
      file_path = cache_image_file(bmain, scene, image, iuser, true);
    }
  }

  CLOG_INFO(LOG_HYDRA_SCENE, 1, "%s -> %s", image->id.name, file_path.c_str());
  return file_path;
}

std::string cache_image_color(float color[4])
{
  char name[128];
  snprintf(name,
           sizeof(name),
           "color_%02d%02d%02d.hdr",
           int(color[0] * 255),
           int(color[1] * 255),
           int(color[2] * 255));
  std::string file_path = get_cache_file(name);
  if (BLI_exists(file_path.c_str())) {
    return file_path;
  }

  ImBuf *ibuf = IMB_allocImBuf(4, 4, 32, IB_rectfloat);
  IMB_rectfill(ibuf, color);
  ibuf->ftype = IMB_FTYPE_RADHDR;

  if (IMB_saveiff(ibuf, file_path.c_str(), IB_rectfloat)) {
    CLOG_INFO(LOG_HYDRA_SCENE, 1, "%s", file_path.c_str());
  }
  else {
    CLOG_ERROR(LOG_HYDRA_SCENE, "Can't save %s", file_path.c_str());
    file_path = "";
  }
  IMB_freeImBuf(ibuf);

  return file_path;
}

}  // namespace blender::io::hydra
