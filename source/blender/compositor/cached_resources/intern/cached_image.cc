/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "RE_pipeline.h"

#include "GPU_texture.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_cryptomatte.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"

#include "DNA_ID.h"
#include "DNA_image_types.h"

#include "COM_cached_image.hh"
#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

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

/* Get the render layer in the given render result specified by the given image user. */
static RenderLayer *get_render_layer(const RenderResult *render_result,
                                     const ImageUser &image_user)
{
  const ListBase *layers = &render_result->layers;
  return static_cast<RenderLayer *>(BLI_findlink(layers, image_user.layer));
}

/* Get the index of the pass with the given name in the render layer specified by the given image
 * user in the given render result. */
static int get_pass_index(const RenderResult *render_result,
                          const ImageUser &image_user,
                          const char *name)
{
  const RenderLayer *render_layer = get_render_layer(render_result, image_user);
  return BLI_findstringindex(&render_layer->passes, name, offsetof(RenderPass, name));
}

/* Get the render pass in the given render layer specified by the given image user. */
static RenderPass *get_render_pass(const RenderLayer *render_layer, const ImageUser &image_user)
{
  return static_cast<RenderPass *>(BLI_findlink(&render_layer->passes, image_user.pass));
}

/* Get the index of the view selected in the image user. If the image is not a multi-view image
 * or only has a single view, then zero is returned. Otherwise, if the image is a multi-view
 * image, the index of the selected view is returned. However, note that the value of the view
 * member of the image user is not the actual index of the view. More specifically, the index 0
 * is reserved to denote the special mode of operation "All", which dynamically selects the view
 * whose name matches the view currently being rendered. It follows that the views are then
 * indexed starting from 1. So for non zero view values, the actual index of the view is the
 * value of the view member of the image user minus 1. */
static int get_view_index(const Context &context,
                          const RenderResult *render_result,
                          const ImageUser &image_user)
{
  /* The image is not a multi-view image, so just return zero. */
  if (!render_result) {
    return 0;
  }

  const ListBase *views = &render_result->views;
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
 * given context and pass name. If the image is a multi-layer image, then the render_result
 * argument should be set, otherwise, it is ignored. */
static ImageUser compute_image_user_for_pass(const Context &context,
                                             const Image *image,
                                             const RenderResult *render_result,
                                             const ImageUser *image_user,
                                             const char *pass_name)
{
  ImageUser image_user_for_pass = *image_user;

  /* Set the needed view. */
  image_user_for_pass.view = get_view_index(context, render_result, image_user_for_pass);

  /* Set the needed pass. */
  if (BKE_image_is_multilayer(image)) {
    image_user_for_pass.pass = get_pass_index(render_result, image_user_for_pass, pass_name);
    BKE_image_multilayer_index(const_cast<RenderResult *>(render_result), &image_user_for_pass);
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
    IMB_float_from_byte(linear_image_buffer);
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

/* Returns the appropriate result type for the given image buffer, which represents the pass in the
 * given render result with the given image user. The type is determined based on the channels
 * count of the buffer for simple images, while channel IDs are also considered for multi-layer
 * images since 3-channel passes can be RGB without alpha and 4-channel passes can be XYZW 4D
 * vectors. */
static ResultType get_result_type(const RenderResult *render_result,
                                  const ImageUser &image_user,
                                  const ImBuf *image_buffer)
{
  if (!render_result) {
    return Result::float_type(image_buffer->channels);
  }

  const RenderLayer *render_layer = get_render_layer(render_result, image_user);
  if (!render_layer) {
    return Result::float_type(image_buffer->channels);
  }

  const RenderPass *render_pass = get_render_pass(render_layer, image_user);
  if (!render_pass) {
    return Result::float_type(image_buffer->channels);
  }

  switch (render_pass->channels) {
    case 1:
      return ResultType::Float;
    case 2:
      return ResultType::Float2;
    case 3:
      if (STR_ELEM(render_pass->chan_id, "RGB", "rgb")) {
        return ResultType::Color;
      }
      else {
        return ResultType::Float3;
      }
    case 4:
      if (STR_ELEM(render_pass->chan_id, "RGBA", "rgba")) {
        return ResultType::Color;
      }
      else {
        return ResultType::Float4;
      }
    default:
      break;
  }

  BLI_assert_unreachable();
  return ResultType::Float;
}

CachedImage::CachedImage(Context &context,
                         Image *image,
                         ImageUser *image_user,
                         const char *pass_name)
    : result(context)
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

  RenderResult *render_result = BKE_image_acquire_renderresult(nullptr, image);

  ImageUser image_user_for_pass = compute_image_user_for_pass(
      context, image, render_result, image_user, pass_name);

  this->populate_meta_data(render_result, image_user_for_pass);

  BKE_image_release_renderresult(nullptr, image, render_result);

  ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user_for_pass, nullptr);
  ImBuf *linear_image_buffer = compute_linear_buffer(image_buffer);

  const bool use_half_float = linear_image_buffer->foptions.flag & OPENEXR_HALF;
  this->result.set_precision(use_half_float ? ResultPrecision::Half : ResultPrecision::Full);

  this->result.set_type(get_result_type(render_result, image_user_for_pass, linear_image_buffer));

  /* For GPU, we wrap the texture returned by IMB module and free it ourselves in destructor. For
   * CPU, we allocate the result and copy to it from the image buffer. */
  if (context.use_gpu()) {
    texture_ = IMB_create_gpu_texture("Image Texture", linear_image_buffer, true, true);
    GPU_texture_update_mipmap_chain(texture_);
    this->result.wrap_external(texture_);
  }
  else {
    const int2 size = int2(image_buffer->x, image_buffer->y);
    Result buffer_result(
        context, Result::float_type(image_buffer->channels), ResultPrecision::Full);
    buffer_result.wrap_external(linear_image_buffer->float_buffer.data, size);
    this->result.allocate_texture(size, false);
    parallel_for(size, [&](const int2 texel) {
      this->result.store_pixel_generic_type(texel, buffer_result.load_pixel_generic_type(texel));
    });
  }

  IMB_freeImBuf(linear_image_buffer);
  BKE_image_release_ibuf(image, image_buffer, nullptr);
}

void CachedImage::populate_meta_data(const RenderResult *render_result,
                                     const ImageUser &image_user)
{
  if (!render_result) {
    return;
  }

  const RenderLayer *render_layer = get_render_layer(render_result, image_user);
  if (!render_layer) {
    return;
  }

  const RenderPass *render_pass = get_render_pass(render_layer, image_user);
  if (!render_pass) {
    return;
  }

  /* We assume the given pass is a Cryptomatte pass and retrieve its full name. If it wasn't a
   * Cryptomatte pass, the checks below will fail anyways. */
  const bool is_named_layer = render_layer->name[0] != '\0';
  const std::string layer_prefix = is_named_layer ? std::string(render_layer->name) + "." : "";
  const std::string combined_pass_name = layer_prefix + render_pass->name;
  StringRef cryptomatte_layer_name = bke::cryptomatte::BKE_cryptomatte_extract_layer_name(
      combined_pass_name);

  struct StampCallbackData {
    std::string cryptomatte_layer_name;
    compositor::MetaData *meta_data;
  };

  /* Go over the stamp data and add any Cryptomatte related meta data. */
  StampCallbackData callback_data = {cryptomatte_layer_name, &this->result.meta_data};
  BKE_stamp_info_callback(
      &callback_data,
      render_result->stamp_data,
      [](void *user_data, const char *key, char *value, int /*value_length*/) {
        StampCallbackData *data = static_cast<StampCallbackData *>(user_data);

        const std::string manifest_key = bke::cryptomatte::BKE_cryptomatte_meta_data_key(
            data->cryptomatte_layer_name, "manifest");
        if (key == manifest_key) {
          data->meta_data->cryptomatte.manifest = value;
        }

        const std::string hash_key = bke::cryptomatte::BKE_cryptomatte_meta_data_key(
            data->cryptomatte_layer_name, "hash");
        if (key == hash_key) {
          data->meta_data->cryptomatte.hash = value;
        }

        const std::string conversion_key = bke::cryptomatte::BKE_cryptomatte_meta_data_key(
            data->cryptomatte_layer_name, "conversion");
        if (key == conversion_key) {
          data->meta_data->cryptomatte.conversion = value;
        }
      },
      false);
}

CachedImage::~CachedImage()
{
  this->result.release();
  GPU_TEXTURE_FREE_SAFE(texture_);
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
  update_counts_.remove_if([&](auto item) { return !map_.contains(item.key); });

  /* Second, reset the needed status of the remaining cached images to false to ready them to
   * track their needed status for the next evaluation. */
  for (auto &cached_images_for_id : map_.values()) {
    for (auto &value : cached_images_for_id.values()) {
      value->needed = false;
    }
  }
}

Result CachedImageContainer::get(Context &context,
                                 Image *image,
                                 const ImageUser *image_user,
                                 const char *pass_name)
{
  if (!image || !image_user) {
    return Result(context);
  }

  /* Compute the effective frame number of the image if it was animated. */
  ImageUser image_user_for_frame = *image_user;
  BKE_image_user_frame_calc(image, &image_user_for_frame, context.get_frame_number());

  const CachedImageKey key(image_user_for_frame, pass_name);

  const std::string library_key = image->id.lib ? image->id.lib->id.name : "";
  const std::string id_key = std::string(image->id.name) + library_key;
  auto &cached_images_for_id = map_.lookup_or_add_default(id_key);

  /* Invalidate the cache for that image if it was changed since it was cached. */
  if (!cached_images_for_id.is_empty() &&
      image->runtime->update_count != update_counts_.lookup(id_key))
  {
    cached_images_for_id.clear();
  }

  auto &cached_image = *cached_images_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<CachedImage>(context, image, &image_user_for_frame, pass_name);
  });

  /* Store the current update count to later compare to and check if the image changed. */
  update_counts_.add_overwrite(id_key, image->runtime->update_count);

  cached_image.needed = true;
  return cached_image.result;
}

}  // namespace blender::compositor
