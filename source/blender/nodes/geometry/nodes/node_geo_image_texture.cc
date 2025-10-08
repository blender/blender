/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_image.hh"

#include "BLI_math_vector_types.hh"
#include "BLI_threads.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_image_texture_cc {

NODE_STORAGE_FUNCS(NodeGeometryImageTexture)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Image>("Image").optional_label();
  b.add_input<decl::Vector>("Vector")
      .implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD)
      .description("Texture coordinates from 0 to 1");
  b.add_input<decl::Int>("Frame").min(0).max(MAXFRAMEF);
  b.add_output<decl::Color>("Color").no_muted_links().dependent_field().reference_pass_all();
  b.add_output<decl::Float>("Alpha").no_muted_links().dependent_field().reference_pass_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  layout->prop(ptr, "extension", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryImageTexture *tex = MEM_callocN<NodeGeometryImageTexture>(__func__);
  tex->interpolation = SHD_INTERP_LINEAR;
  tex->extension = SHD_IMAGE_EXTENSION_REPEAT;
  node->storage = tex;
}

class ImageFieldsFunction : public mf::MultiFunction {
 private:
  const int8_t interpolation_;
  const int8_t extension_;
  Image &image_;
  ImageUser image_user_;
  void *image_lock_;
  ImBuf *image_buffer_;

 public:
  ImageFieldsFunction(const int8_t interpolation,
                      const int8_t extension,
                      Image &image,
                      ImageUser image_user)
      : interpolation_(interpolation),
        extension_(extension),
        image_(image),
        image_user_(image_user)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"ImageFunction", signature};
      builder.single_input<float3>("Vector");
      builder.single_output<ColorGeometry4f>("Color");
      builder.single_output<float>("Alpha", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);

    image_buffer_ = BKE_image_acquire_ibuf(&image_, &image_user_, &image_lock_);
    if (image_buffer_ == nullptr) {
      throw std::runtime_error("cannot acquire image buffer");
    }

    if (image_buffer_->float_buffer.data == nullptr) {
      BLI_thread_lock(LOCK_IMAGE);
      if (!image_buffer_->float_buffer.data) {
        IMB_float_from_byte(image_buffer_);
      }
      BLI_thread_unlock(LOCK_IMAGE);
    }

    if (image_buffer_->float_buffer.data == nullptr) {
      BKE_image_release_ibuf(&image_, image_buffer_, image_lock_);
      throw std::runtime_error("cannot get float buffer");
    }
  }

  ~ImageFieldsFunction() override
  {
    BKE_image_release_ibuf(&image_, image_buffer_, image_lock_);
  }

  static int wrap_periodic(int x, const int width)
  {
    x %= width;
    if (x < 0) {
      x += width;
    }
    return x;
  }

  static int wrap_clamp(const int x, const int width)
  {
    return std::clamp(x, 0, width - 1);
  }

  static int wrap_mirror(const int x, const int width)
  {
    const int m = std::abs(x + (x < 0)) % (2 * width);
    if (m >= width) {
      return 2 * width - m - 1;
    }
    return m;
  }

  static float4 image_pixel_lookup(const ImBuf &ibuf, const int px, const int py)
  {
    if (px < 0 || py < 0 || px >= ibuf.x || py >= ibuf.y) {
      return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    return ((const float4 *)ibuf.float_buffer.data)[size_t(px) + size_t(py) * size_t(ibuf.x)];
  }

  static float frac(const float x, int *ix)
  {
    const int i = int(x) - ((x < 0.0f) ? 1 : 0);
    *ix = i;
    return x - float(i);
  }

  static float4 image_cubic_texture_lookup(const ImBuf &ibuf,
                                           const float px,
                                           const float py,
                                           const int extension)
  {
    const int width = ibuf.x;
    const int height = ibuf.y;
    int pix, piy, nix, niy;
    const float tx = frac(px * float(width) - 0.5f, &pix);
    const float ty = frac(py * float(height) - 0.5f, &piy);
    int ppix, ppiy, nnix, nniy;

    switch (extension) {
      case SHD_IMAGE_EXTENSION_REPEAT: {
        pix = wrap_periodic(pix, width);
        piy = wrap_periodic(piy, height);
        ppix = wrap_periodic(pix - 1, width);
        ppiy = wrap_periodic(piy - 1, height);
        nix = wrap_periodic(pix + 1, width);
        niy = wrap_periodic(piy + 1, height);
        nnix = wrap_periodic(pix + 2, width);
        nniy = wrap_periodic(piy + 2, height);
        break;
      }
      case SHD_IMAGE_EXTENSION_CLIP: {
        ppix = pix - 1;
        ppiy = piy - 1;
        nix = pix + 1;
        niy = piy + 1;
        nnix = pix + 2;
        nniy = piy + 2;
        break;
      }
      case SHD_IMAGE_EXTENSION_EXTEND: {
        ppix = wrap_clamp(pix - 1, width);
        ppiy = wrap_clamp(piy - 1, height);
        nix = wrap_clamp(pix + 1, width);
        niy = wrap_clamp(piy + 1, height);
        nnix = wrap_clamp(pix + 2, width);
        nniy = wrap_clamp(piy + 2, height);
        pix = wrap_clamp(pix, width);
        piy = wrap_clamp(piy, height);
        break;
      }
      case SHD_IMAGE_EXTENSION_MIRROR: {
        ppix = wrap_mirror(pix - 1, width);
        ppiy = wrap_mirror(piy - 1, height);
        nix = wrap_mirror(pix + 1, width);
        niy = wrap_mirror(piy + 1, height);
        nnix = wrap_mirror(pix + 2, width);
        nniy = wrap_mirror(piy + 2, height);
        pix = wrap_mirror(pix, width);
        piy = wrap_mirror(piy, height);
        break;
      }
      default:
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    const int xc[4] = {ppix, pix, nix, nnix};
    const int yc[4] = {ppiy, piy, niy, nniy};
    float u[4], v[4];

    u[0] = (((-1.0f / 6.0f) * tx + 0.5f) * tx - 0.5f) * tx + (1.0f / 6.0f);
    u[1] = ((0.5f * tx - 1.0f) * tx) * tx + (2.0f / 3.0f);
    u[2] = ((-0.5f * tx + 0.5f) * tx + 0.5f) * tx + (1.0f / 6.0f);
    u[3] = (1.0f / 6.0f) * tx * tx * tx;

    v[0] = (((-1.0f / 6.0f) * ty + 0.5f) * ty - 0.5f) * ty + (1.0f / 6.0f);
    v[1] = ((0.5f * ty - 1.0f) * ty) * ty + (2.0f / 3.0f);
    v[2] = ((-0.5f * ty + 0.5f) * ty + 0.5f) * ty + (1.0f / 6.0f);
    v[3] = (1.0f / 6.0f) * ty * ty * ty;

    return (v[0] * (u[0] * image_pixel_lookup(ibuf, xc[0], yc[0]) +
                    u[1] * image_pixel_lookup(ibuf, xc[1], yc[0]) +
                    u[2] * image_pixel_lookup(ibuf, xc[2], yc[0]) +
                    u[3] * image_pixel_lookup(ibuf, xc[3], yc[0]))) +
           (v[1] * (u[0] * image_pixel_lookup(ibuf, xc[0], yc[1]) +
                    u[1] * image_pixel_lookup(ibuf, xc[1], yc[1]) +
                    u[2] * image_pixel_lookup(ibuf, xc[2], yc[1]) +
                    u[3] * image_pixel_lookup(ibuf, xc[3], yc[1]))) +
           (v[2] * (u[0] * image_pixel_lookup(ibuf, xc[0], yc[2]) +
                    u[1] * image_pixel_lookup(ibuf, xc[1], yc[2]) +
                    u[2] * image_pixel_lookup(ibuf, xc[2], yc[2]) +
                    u[3] * image_pixel_lookup(ibuf, xc[3], yc[2]))) +
           (v[3] * (u[0] * image_pixel_lookup(ibuf, xc[0], yc[3]) +
                    u[1] * image_pixel_lookup(ibuf, xc[1], yc[3]) +
                    u[2] * image_pixel_lookup(ibuf, xc[2], yc[3]) +
                    u[3] * image_pixel_lookup(ibuf, xc[3], yc[3])));
  }

  static float4 image_linear_texture_lookup(const ImBuf &ibuf,
                                            const float px,
                                            const float py,
                                            const int8_t extension)
  {
    const int width = ibuf.x;
    const int height = ibuf.y;
    int pix, piy, nix, niy;
    const float nfx = frac(px * float(width) - 0.5f, &pix);
    const float nfy = frac(py * float(height) - 0.5f, &piy);

    switch (extension) {
      case SHD_IMAGE_EXTENSION_CLIP: {
        nix = pix + 1;
        niy = piy + 1;
        break;
      }
      case SHD_IMAGE_EXTENSION_EXTEND: {
        nix = wrap_clamp(pix + 1, width);
        niy = wrap_clamp(piy + 1, height);
        pix = wrap_clamp(pix, width);
        piy = wrap_clamp(piy, height);
        break;
      }
      case SHD_IMAGE_EXTENSION_MIRROR:
        nix = wrap_mirror(pix + 1, width);
        niy = wrap_mirror(piy + 1, height);
        pix = wrap_mirror(pix, width);
        piy = wrap_mirror(piy, height);
        break;
      default:
      case SHD_IMAGE_EXTENSION_REPEAT:
        pix = wrap_periodic(pix, width);
        piy = wrap_periodic(piy, height);
        nix = wrap_periodic(pix + 1, width);
        niy = wrap_periodic(piy + 1, height);
        break;
    }

    const float ptx = 1.0f - nfx;
    const float pty = 1.0f - nfy;

    return image_pixel_lookup(ibuf, pix, piy) * ptx * pty +
           image_pixel_lookup(ibuf, nix, piy) * nfx * pty +
           image_pixel_lookup(ibuf, pix, niy) * ptx * nfy +
           image_pixel_lookup(ibuf, nix, niy) * nfx * nfy;
  }

  static float4 image_closest_texture_lookup(const ImBuf &ibuf,
                                             const float px,
                                             const float py,
                                             const int extension)
  {
    const int width = ibuf.x;
    const int height = ibuf.y;
    int ix, iy;
    const float tx = frac(px * float(width), &ix);
    const float ty = frac(py * float(height), &iy);

    switch (extension) {
      case SHD_IMAGE_EXTENSION_REPEAT: {
        ix = wrap_periodic(ix, width);
        iy = wrap_periodic(iy, height);
        return image_pixel_lookup(ibuf, ix, iy);
      }
      case SHD_IMAGE_EXTENSION_CLIP: {
        if (tx < 0.0f || ty < 0.0f || tx > 1.0f || ty > 1.0f) {
          return float4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (ix < 0 || iy < 0 || ix > width || iy > height) {
          return float4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        ATTR_FALLTHROUGH;
      }
      case SHD_IMAGE_EXTENSION_EXTEND: {
        ix = wrap_clamp(ix, width);
        iy = wrap_clamp(iy, height);
        return image_pixel_lookup(ibuf, ix, iy);
      }
      case SHD_IMAGE_EXTENSION_MIRROR: {
        ix = wrap_mirror(ix, width);
        iy = wrap_mirror(iy, height);
        return image_pixel_lookup(ibuf, ix, iy);
      }
      default:
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vectors = params.readonly_single_input<float3>(0, "Vector");
    MutableSpan<ColorGeometry4f> r_color = params.uninitialized_single_output<ColorGeometry4f>(
        1, "Color");
    MutableSpan<float> r_alpha = params.uninitialized_single_output_if_required<float>(2, "Alpha");

    MutableSpan<float4> color_data{reinterpret_cast<float4 *>(r_color.data()), r_color.size()};

    /* Sample image texture. */
    switch (interpolation_) {
      case SHD_INTERP_LINEAR:
        mask.foreach_index([&](const int64_t i) {
          const float3 p = vectors[i];
          color_data[i] = image_linear_texture_lookup(*image_buffer_, p.x, p.y, extension_);
        });
        break;
      case SHD_INTERP_CLOSEST:
        mask.foreach_index([&](const int64_t i) {
          const float3 p = vectors[i];
          color_data[i] = image_closest_texture_lookup(*image_buffer_, p.x, p.y, extension_);
        });
        break;
      case SHD_INTERP_CUBIC:
      case SHD_INTERP_SMART:
        mask.foreach_index([&](const int64_t i) {
          const float3 p = vectors[i];
          color_data[i] = image_cubic_texture_lookup(*image_buffer_, p.x, p.y, extension_);
        });
        break;
    }

    int alpha_mode = image_.alpha_mode;
    if (IMB_colormanagement_space_name_is_data(image_.colorspace_settings.name)) {
      alpha_mode = IMA_ALPHA_CHANNEL_PACKED;
    }

    switch (alpha_mode) {
      case IMA_ALPHA_STRAIGHT: {
        /* #ColorGeometry expects premultiplied alpha, so convert from straight to that. */
        mask.foreach_index([&](const int64_t i) { straight_to_premul_v4(color_data[i]); });
        break;
      }
      case IMA_ALPHA_PREMUL: {
        /* Alpha is premultiplied already, nothing to do. */
        break;
      }
      case IMA_ALPHA_CHANNEL_PACKED: {
        /* Color and alpha channels shouldn't interact with each other, nothing to do. */
        break;
      }
      case IMA_ALPHA_IGNORE: {
        /* The image should be treated as being opaque. */
        mask.foreach_index([&](const int64_t i) { color_data[i].w = 1.0f; });
        break;
      }
    }

    if (!r_alpha.is_empty()) {
      mask.foreach_index([&](const int64_t i) { r_alpha[i] = r_color[i].a; });
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Image *image = params.extract_input<Image *>("Image");
  if (image == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryImageTexture &storage = node_storage(params.node());

  ImageUser image_user;
  BKE_imageuser_default(&image_user);
  image_user.cycl = false;
  image_user.frames = INT_MAX;
  image_user.sfra = 1;
  image_user.framenr = BKE_image_is_animated(image) ? params.extract_input<int>("Frame") : 0;

  std::unique_ptr<ImageFieldsFunction> image_fn;
  try {
    image_fn = std::make_unique<ImageFieldsFunction>(
        storage.interpolation, storage.extension, *image, image_user);
  }
  catch (const std::runtime_error &) {
    params.set_default_remaining_outputs();
    return;
  }

  auto sample_uv = params.extract_input<bke::SocketValueVariant>("Vector");

  std::string error_message;
  bke::SocketValueVariant color;
  bke::SocketValueVariant alpha;
  if (!execute_multi_function_on_value_variant(
          std::move(image_fn), {&sample_uv}, {&color, &alpha}, params.user_data(), error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Color", std::move(color));
  params.set_output("Alpha", std::move(alpha));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImageTexture", GEO_NODE_IMAGE_TEXTURE);
  ntype.ui_name = "Image Texture";
  ntype.ui_description = "Sample values from an image texture";
  ntype.enum_name_legacy = "IMAGE_TEXTURE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryImageTexture", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  ntype.geometry_node_execute = node_geo_exec;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_image_texture_cc
