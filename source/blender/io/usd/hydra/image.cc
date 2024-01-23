/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "image.h"

#include <pxr/imaging/hio/imageRegistry.h>

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_image_save.h"
#include "BKE_main.hh"
#include "BKE_packedFile.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "hydra_scene_delegate.h"

namespace blender::io::hydra {

std::string image_cache_file_path()
{
  char dir_path[FILE_MAX];
  BLI_path_join(dir_path, sizeof(dir_path), BKE_tempdir_session(), "hydra", "image_cache");
  return dir_path;
}

static std::string get_cache_file(const std::string &file_name, bool mkdir = true)
{
  std::string dir_path = image_cache_file_path();
  if (mkdir) {
    BLI_dir_create_recursive(dir_path.c_str());
  }

  char file_path[FILE_MAX];
  BLI_path_join(file_path, sizeof(file_path), dir_path.c_str(), file_name.c_str());
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

    SNPRINTF(file_name, "img_%p%s", image, r_ext);

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
  char str[FILE_MAX];
  std::string file_path;
  bool do_check_extension = false;
  if (image->source == IMA_SRC_GENERATED) {
    file_path = cache_image_file(bmain, scene, image, iuser, false);
  }
  else if (BKE_image_has_packedfile(image)) {
    do_check_extension = true;
    std::string dir_path = image_cache_file_path();
    char *cached_path;
    char subfolder[FILE_MAXDIR];
    SNPRINTF(subfolder, "unpack_%p", image);
    LISTBASE_FOREACH (ImagePackedFile *, ipf, &image->packedfiles) {
      char path[FILE_MAX];
      BLI_path_join(
          path, sizeof(path), dir_path.c_str(), subfolder, BLI_path_basename(ipf->filepath));
      cached_path = BKE_packedfile_unpack_to_file(nullptr,
                                                  BKE_main_blendfile_path(bmain),
                                                  dir_path.c_str(),
                                                  path,
                                                  ipf->packedfile,
                                                  PF_WRITE_LOCAL);

      /* Take first successfully unpacked image. */
      if (cached_path != nullptr) {
        if (file_path.empty()) {
          file_path = cached_path;
        }
        MEM_freeN(cached_path);
      }
    }
  }
  else {
    do_check_extension = true;
    BKE_image_user_file_path_ex(bmain, iuser, image, str, false, true);
    file_path = str;
  }

  if (do_check_extension && !pxr::HioImageRegistry::GetInstance().IsSupportedImageFile(file_path))
  {
    file_path = cache_image_file(bmain, scene, image, iuser, true);
  }

  CLOG_INFO(LOG_HYDRA_SCENE, 1, "%s -> %s", image->id.name, file_path.c_str());
  return file_path;
}

std::string cache_image_color(float color[4])
{
  char name[128];
  SNPRINTF(name,
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
