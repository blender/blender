/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_assert.h"
#include "BLI_hash.hh"
#include "BLI_listbase.h"

#include "RE_pipeline.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_image.h"
#include "BKE_lib_id.hh"

#include "DNA_ID.h"
#include "DNA_image_types.h"

#include "COM_cached_image.hh"
#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Cached Image Key.
 */

CachedImageKey::CachedImageKey(ImageUser image_user, std::string pass_name)
    : image_user(image_user), pass_name(pass_name)
{
}

uint64_t CachedImageKey::hash() const
{
  return get_default_hash(image_user.framenr, image_user.layer, image_user.view, pass_name);
}

bool operator==(const CachedImageKey &a, const CachedImageKey &b)
{
  return a.image_user.framenr == b.image_user.framenr &&
         a.image_user.layer == b.image_user.layer && a.image_user.view == b.image_user.view &&
         a.pass_name == b.pass_name;
}

/* --------------------------------------------------------------------
 * Cached Image.
 */

/* Returns a new texture of the given format and precision preprocessed using the given shader. The
 * input texture is freed. */
static GPUTexture *preprocess_texture(Context &context,
                                      GPUTexture *input_texture,
                                      eGPUTextureFormat target_format,
                                      ResultPrecision precision,
                                      const char *shader_name)
{
  const int2 size = int2(GPU_texture_width(input_texture), GPU_texture_height(input_texture));

  GPUTexture *preprocessed_texture = GPU_texture_create_2d(
      "Cached Image", size.x, size.y, 1, target_format, GPU_TEXTURE_USAGE_GENERAL, nullptr);

  GPUShader *shader = context.get_shader(shader_name, precision);
  GPU_shader_bind(shader);

  const int input_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
  GPU_texture_bind(input_texture, input_unit);

  const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
  GPU_texture_image_bind(preprocessed_texture, image_unit);

  compute_dispatch_threads_at_least(shader, size);

  GPU_shader_unbind();
  GPU_texture_unbind(input_texture);
  GPU_texture_image_unbind(preprocessed_texture);
  GPU_texture_free(input_texture);

  return preprocessed_texture;
}

/* Compositor images are expected to be always pre-multiplied, so identify if the GPU texture
 * returned by the IMB module is straight and needs to be pre-multiplied. An exception is when
 * the image has an alpha mode of channel packed or alpha ignore, in which case, we always ignore
 * pre-multiplication. */
static bool should_premultiply_alpha(Image *image, ImBuf *image_buffer)
{
  if (ELEM(image->alpha_mode, IMA_ALPHA_CHANNEL_PACKED, IMA_ALPHA_IGNORE)) {
    return false;
  }

  return !BKE_image_has_gpu_texture_premultiplied_alpha(image, image_buffer);
}

/* Get a suitable texture format supported by the compositor given the format of the texture
 * returned by the IMB module. See imb_gpu_get_format for the formats that needs to be handled. */
static eGPUTextureFormat get_compatible_texture_format(eGPUTextureFormat original_format)
{
  switch (original_format) {
    case GPU_R16F:
    case GPU_R32F:
    case GPU_RGBA16F:
    case GPU_RGBA32F:
      return original_format;
    case GPU_R8:
      return GPU_R16F;
    case GPU_RGBA8:
    case GPU_SRGB8_A8:
      return GPU_RGBA16F;
    default:
      break;
  }

  BLI_assert_unreachable();
  return original_format;
}

/* Get the selected render layer selected assuming the image is a multilayer image. */
static RenderLayer *get_render_layer(Image *image, ImageUser &image_user)
{
  const ListBase *layers = &image->rr->layers;
  return static_cast<RenderLayer *>(BLI_findlink(layers, image_user.layer));
}

/* Get the index of the pass with the given name in the selected render layer's passes list
 * assuming the image is a multilayer image. */
static int get_pass_index(Image *image, ImageUser &image_user, const char *name)
{
  const RenderLayer *render_layer = get_render_layer(image, image_user);
  return BLI_findstringindex(&render_layer->passes, name, offsetof(RenderPass, name));
}

/* Get the index of the view selected in the image user. If the image is not a multi-view image
 * or only has a single view, then zero is returned. Otherwise, if the image is a multi-view
 * image, the index of the selected view is returned. However, note that the value of the view
 * member of the image user is not the actual index of the view. More specifically, the index 0
 * is reserved to denote the special mode of operation "All", which dynamically selects the view
 * whose name matches the view currently being rendered. It follows that the views are then
 * indexed starting from 1. So for non zero view values, the actual index of the view is the
 * value of the view member of the image user minus 1. */
static int get_view_index(Context &context, Image *image, ImageUser &image_user)
{
  /* The image is not a multi-view image, so just return zero. */
  if (!BKE_image_is_multiview(image)) {
    return 0;
  }

  const ListBase *views = &image->rr->views;
  /* There is only one view and its index is 0. */
  if (BLI_listbase_count_at_most(views, 2) < 2) {
    return 0;
  }

  const int view = image_user.view;
  /* The view is not zero, which means it is manually specified and the actual index is then the
   * view value minus 1. */
  if (view != 0) {
    return view - 1;
  }

  /* Otherwise, the view value is zero, denoting the special mode of operation "All", which finds
   * the index of the view whose name matches the view currently being rendered. */
  const char *view_name = context.get_view_name().data();
  const int matched_view = BLI_findstringindex(views, view_name, offsetof(RenderView, name));

  /* No view matches the view currently being rendered, so fallback to the first view. */
  if (matched_view == -1) {
    return 0;
  }

  return matched_view;
}

/* Get a copy of the image user that is appropriate to retrieve the needed image buffer from the
 * image. This essentially sets the appropriate frame, pass, and view that corresponds to the
 * given context and pass name. */
static ImageUser compute_image_user_for_pass(Context &context,
                                             Image *image,
                                             const ImageUser *image_user,
                                             const char *pass_name)
{
  ImageUser image_user_for_pass = *image_user;

  /* Set the needed view. */
  image_user_for_pass.view = get_view_index(context, image, image_user_for_pass);

  /* Set the needed pass. */
  if (BKE_image_is_multilayer(image)) {
    image_user_for_pass.pass = get_pass_index(image, image_user_for_pass, pass_name);
    BKE_image_multilayer_index(image->rr, &image_user_for_pass);
  }
  else {
    BKE_image_multiview_index(image, &image_user_for_pass);
  }

  return image_user_for_pass;
}

CachedImage::CachedImage(Context &context,
                         Image *image,
                         ImageUser *image_user,
                         const char *pass_name)
{
  /* We can't retrieve the needed image buffer yet, because we still need to assign the pass index
   * to the image user in order to acquire the image buffer corresponding to the given pass name.
   * However, in order to compute the pass index, we need the render result structure of the image
   * to be initialized. So we first acquire a dummy image buffer since it initializes the image
   * render result as a side effect. We also use that as a mean of validation, since we can early
   * exit if the returned image buffer is nullptr. This image buffer can be immediately released.
   * Since it carries no important information. */
  ImBuf *initial_image_buffer = BKE_image_acquire_ibuf(image, image_user, nullptr);
  BKE_image_release_ibuf(image, initial_image_buffer, nullptr);
  if (!initial_image_buffer) {
    return;
  }

  ImageUser image_user_for_pass = compute_image_user_for_pass(
      context, image, image_user, pass_name);

  ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user_for_pass, nullptr);
  const bool is_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(image, image_buffer);
  texture_ = IMB_create_gpu_texture("Image Texture", image_buffer, true, is_premultiplied);
  GPU_texture_update_mipmap_chain(texture_);

  const eGPUTextureFormat original_format = GPU_texture_format(texture_);
  const eGPUTextureFormat target_format = get_compatible_texture_format(original_format);
  const ResultType result_type = Result::type(target_format);
  const ResultPrecision precision = Result::precision(target_format);

  /* The GPU image returned by the IMB module can be in a format not supported by the compositor,
   * or it might need pre-multiplication, so preprocess them first. */
  if (result_type == ResultType::Color && should_premultiply_alpha(image, image_buffer)) {
    texture_ = preprocess_texture(
        context, texture_, target_format, precision, "compositor_premultiply_alpha");
  }
  else if (original_format != target_format) {
    const char *conversion_shader_name = result_type == ResultType::Float ?
                                             "compositor_convert_float_to_float" :
                                             "compositor_convert_color_to_color";
    texture_ = preprocess_texture(
        context, texture_, target_format, precision, conversion_shader_name);
  }

  /* Set the alpha to 1 using swizzling if alpha is ignored. */
  if (result_type == ResultType::Color && image->alpha_mode == IMA_ALPHA_IGNORE) {
    GPU_texture_swizzle_set(texture_, "rgb1");
  }

  BKE_image_release_ibuf(image, image_buffer, nullptr);
}

CachedImage::~CachedImage()
{
  GPU_TEXTURE_FREE_SAFE(texture_);
}

GPUTexture *CachedImage::texture()
{
  return texture_;
}

/* --------------------------------------------------------------------
 * Cached Image Container.
 */

void CachedImageContainer::reset()
{
  /* First, delete all cached images that are no longer needed. */
  for (auto &cached_images_for_id : map_.values()) {
    cached_images_for_id.remove_if([](auto item) { return !item.value->needed; });
  }
  map_.remove_if([](auto item) { return item.value.is_empty(); });

  /* Second, reset the needed status of the remaining cached images to false to ready them to
   * track their needed status for the next evaluation. */
  for (auto &cached_images_for_id : map_.values()) {
    for (auto &value : cached_images_for_id.values()) {
      value->needed = false;
    }
  }
}

GPUTexture *CachedImageContainer::get(Context &context,
                                      Image *image,
                                      const ImageUser *image_user,
                                      const char *pass_name)
{
  if (!image || !image_user) {
    return nullptr;
  }

  /* Compute the effective frame number of the image if it was animated. */
  ImageUser image_user_for_frame = *image_user;
  BKE_image_user_frame_calc(image, &image_user_for_frame, context.get_frame_number());

  const CachedImageKey key(image_user_for_frame, pass_name);

  auto &cached_images_for_id = map_.lookup_or_add_default(image->id.name);

  /* Invalidate the cache for that image ID if it was changed and reset the recalculate flag. */
  if (context.query_id_recalc_flag(reinterpret_cast<ID *>(image)) & ID_RECALC_ALL) {
    cached_images_for_id.clear();
  }

  auto &cached_image = *cached_images_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<CachedImage>(context, image, &image_user_for_frame, pass_name);
  });

  cached_image.needed = true;
  return cached_image.texture();
}

}  // namespace blender::realtime_compositor
