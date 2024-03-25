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

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "IMB_colormanagement.hh"
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

/* The image buffer might be stored as an sRGB 8-bit image, while the compositor expects linear
 * float images, so compute a linear float buffer for the image buffer. This will also do linear
 * space conversion and alpha pre-multiplication as needed. We could store those images in sRGB GPU
 * textures and let the GPU do the linear space conversion, but the issues is that we don't control
 * how the GPU does the conversion and so we get tiny differences across CPU and GPU compositing,
 * and potentially even across GPUs/Drivers. Further, if alpha pre-multiplication is needed, we
 * would need to do it ourself, which means alpha pre-multiplication will happen before linear
 * space conversion, which would produce yet another difference. So we just do everything on the
 * CPU, since this is already a cached resource.
 *
 * To avoid conflicts with other threads, create a new image buffer and assign all the necessary
 * information to it, with IB_DO_NOT_TAKE_OWNERSHIP for buffers since a deep copy is not needed.
 *
 * The caller should free the returned image buffer. */
static ImBuf *compute_linear_buffer(ImBuf *image_buffer)
{
  /* Do not pass the flags to the allocation function to avoid buffer allocation, but assign them
   * after to retain important information like precision and alpha mode. */
  ImBuf *linear_image_buffer = IMB_allocImBuf(
      image_buffer->x, image_buffer->y, image_buffer->planes, 0);
  linear_image_buffer->flags = image_buffer->flags;

  /* Assign the float buffer if it exists, as well as its number of channels. */
  IMB_assign_float_buffer(
      linear_image_buffer, image_buffer->float_buffer, IB_DO_NOT_TAKE_OWNERSHIP);
  linear_image_buffer->channels = image_buffer->channels;

  /* If no float buffer exists, assign it then compute a float buffer from it. This is the main
   * call of this function. */
  if (!linear_image_buffer->float_buffer.data) {
    IMB_assign_byte_buffer(
        linear_image_buffer, image_buffer->byte_buffer, IB_DO_NOT_TAKE_OWNERSHIP);
    IMB_float_from_rect(linear_image_buffer);
  }

  /* If the image buffer contained compressed data, assign them as well, but only if the color
   * space of the buffer is linear or data, since we need linear data and can't preprocess the
   * compressed buffer. If not, we fallback to the float buffer already assigned, which is
   * guaranteed to exist as a fallback for compressed textures. */
  const bool is_suitable_compressed_color_space =
      IMB_colormanagement_space_is_data(image_buffer->byte_buffer.colorspace) ||
      IMB_colormanagement_space_is_scene_linear(image_buffer->byte_buffer.colorspace);
  if (image_buffer->ftype == IMB_FTYPE_DDS && is_suitable_compressed_color_space) {
    linear_image_buffer->ftype = IMB_FTYPE_DDS;
    IMB_assign_dds_data(linear_image_buffer, image_buffer->dds_data, IB_DO_NOT_TAKE_OWNERSHIP);
  }

  return linear_image_buffer;
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
  ImBuf *linear_image_buffer = compute_linear_buffer(image_buffer);

  texture_ = IMB_create_gpu_texture("Image Texture", linear_image_buffer, true, true);
  GPU_texture_update_mipmap_chain(texture_);

  IMB_freeImBuf(linear_image_buffer);
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
