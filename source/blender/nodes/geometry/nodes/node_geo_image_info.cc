/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_image.h"

#include "BLI_path_util.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_image_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Image>("Image").hide_label();
  b.add_input<decl::Int>("Frame").min(0).description(
      "Which frame to use for videos. Note that different frames in videos can "
      "have different resolutions");

  b.add_output<decl::Int>("Width");
  b.add_output<decl::Int>("Height");
  b.add_output<decl::Bool>("Has Alpha").description("Whether the image has an alpha channel");

  b.add_output<decl::Int>("Frame Count")
      .description("The number of animation frames. If a single image, then 1");
  b.add_output<decl::Float>("FPS").description(
      "Animation playback speed in frames per second. If a single image, then 0");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Image *image = params.get_input<Image *>("Image");
  const int frame = params.get_input<int>("Frame");
  if (!image) {
    params.set_default_remaining_outputs();
    return;
  }

  ImageUser image_user;
  BKE_imageuser_default(&image_user);
  image_user.frames = INT_MAX;
  image_user.framenr = BKE_image_is_animated(image) ? frame : 0;

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, &lock);
  BLI_SCOPED_DEFER([&]() { BKE_image_release_ibuf(image, ibuf, lock); });
  if (!ibuf) {
    params.set_default_remaining_outputs();
    return;
  }

  params.set_output("Has Alpha", ELEM(ibuf->planes, 32, 16));
  params.set_output("Width", ibuf->x);
  params.set_output("Height", ibuf->y);

  int frames = 1;
  float fps = 0.0f;

  if (ImageAnim *ianim = static_cast<ImageAnim *>(image->anims.first)) {
    auto *anim = ianim->anim;
    if (anim) {
      frames = IMB_anim_get_duration(anim, IMB_TC_NONE);

      short fps_sec = 0;
      float fps_sec_base = 0.0f;
      IMB_anim_get_fps(anim, &fps_sec, &fps_sec_base, true);
      fps = float(fps_sec) / fps_sec_base;
    }
  }

  params.set_output("Frame Count", frames);
  params.set_output("FPS", fps);
}

}  // namespace blender::nodes::node_geo_image_info_cc

void register_node_type_geo_image_info()
{
  namespace file_ns = blender::nodes::node_geo_image_info_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_IMAGE_INFO, "Image Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  nodeRegisterType(&ntype);
}
