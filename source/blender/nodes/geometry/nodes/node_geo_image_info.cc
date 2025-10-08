/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_image.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_image_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Image>("Image").optional_label();
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
  Image *image = params.extract_input<Image *>("Image");
  const int frame = params.extract_input<int>("Frame");
  if (!image) {
    params.set_default_remaining_outputs();
    return;
  }

  ImageUser image_user;
  BKE_imageuser_default(&image_user);
  image_user.frames = std::numeric_limits<int>::max();
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
    MovieReader *anim = ianim->anim;
    if (anim) {
      frames = MOV_get_duration_frames(anim, IMB_TC_NONE);
      fps = MOV_get_fps(anim);
    }
  }

  params.set_output("Frame Count", frames);
  params.set_output("FPS", fps);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImageInfo", GEO_NODE_IMAGE_INFO);
  ntype.ui_name = "Image Info";
  ntype.ui_description = "Retrieve information about an image";
  ntype.enum_name_legacy = "IMAGE_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_image_info_cc
