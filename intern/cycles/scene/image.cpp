/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/image.h"
#include "device/device.h"
#include "scene/colorspace.h"
#include "scene/image_oiio.h"
#include "scene/image_vdb.h"
#include "scene/scene.h"
#include "scene/stats.h"

#include "util/foreach.h"
#include "util/image.h"
#include "util/image_impl.h"
#include "util/log.h"
#include "util/path.h"
#include "util/progress.h"
#include "util/task.h"
#include "util/texture.h"
#include "util/unique_ptr.h"

#ifdef WITH_OSL
#  include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

namespace {

/* Some helpers to silence warning in templated function. */
bool isfinite(uchar /*value*/)
{
  return true;
}
bool isfinite(half /*value*/)
{
  return true;
}
bool isfinite(uint16_t /*value*/)
{
  return true;
}

const char *name_from_type(ImageDataType type)
{
  switch (type) {
    case IMAGE_DATA_TYPE_FLOAT4:
      return "float4";
    case IMAGE_DATA_TYPE_BYTE4:
      return "byte4";
    case IMAGE_DATA_TYPE_HALF4:
      return "half4";
    case IMAGE_DATA_TYPE_FLOAT:
      return "float";
    case IMAGE_DATA_TYPE_BYTE:
      return "byte";
    case IMAGE_DATA_TYPE_HALF:
      return "half";
    case IMAGE_DATA_TYPE_USHORT4:
      return "ushort4";
    case IMAGE_DATA_TYPE_USHORT:
      return "ushort";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
      return "nanovdb_float";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
      return "nanovdb_float3";
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
      return "nanovdb_fpn";
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
      return "nanovdb_fp16";
    case IMAGE_DATA_NUM_TYPES:
      assert(!"System enumerator type, should never be used");
      return "";
  }
  assert(!"Unhandled image data type");
  return "";
}

}  // namespace

/* Image Handle */

ImageHandle::ImageHandle() : manager(NULL)
{
}

ImageHandle::ImageHandle(const ImageHandle &other)
    : tile_slots(other.tile_slots), manager(other.manager)
{
  /* Increase image user count. */
  foreach (const int slot, tile_slots) {
    manager->add_image_user(slot);
  }
}

ImageHandle &ImageHandle::operator=(const ImageHandle &other)
{
  clear();
  manager = other.manager;
  tile_slots = other.tile_slots;

  foreach (const int slot, tile_slots) {
    manager->add_image_user(slot);
  }

  return *this;
}

ImageHandle::~ImageHandle()
{
  clear();
}

void ImageHandle::clear()
{
  foreach (const int slot, tile_slots) {
    manager->remove_image_user(slot);
  }

  tile_slots.clear();
  manager = NULL;
}

bool ImageHandle::empty() const
{
  return tile_slots.empty();
}

int ImageHandle::num_tiles() const
{
  return tile_slots.size();
}

ImageMetaData ImageHandle::metadata()
{
  if (tile_slots.empty()) {
    return ImageMetaData();
  }

  ImageManager::Image *img = manager->images[tile_slots.front()];
  manager->load_image_metadata(img);
  return img->metadata;
}

int ImageHandle::svm_slot(const int tile_index) const
{
  if (tile_index >= tile_slots.size()) {
    return -1;
  }

  if (manager->osl_texture_system) {
    ImageManager::Image *img = manager->images[tile_slots[tile_index]];
    if (!img->loader->osl_filepath().empty()) {
      return -1;
    }
  }

  return tile_slots[tile_index];
}

vector<int4> ImageHandle::get_svm_slots() const
{
  const size_t num_nodes = divide_up(tile_slots.size(), 2);

  vector<int4> svm_slots;
  svm_slots.reserve(num_nodes);
  for (size_t i = 0; i < num_nodes; i++) {
    int4 node;

    int slot = tile_slots[2 * i];
    node.x = manager->images[slot]->loader->get_tile_number();
    node.y = slot;

    if ((2 * i + 1) < tile_slots.size()) {
      slot = tile_slots[2 * i + 1];
      node.z = manager->images[slot]->loader->get_tile_number();
      node.w = slot;
    }
    else {
      node.z = -1;
      node.w = -1;
    }

    svm_slots.push_back(node);
  }

  return svm_slots;
}

device_texture *ImageHandle::image_memory(const int tile_index) const
{
  if (tile_index >= tile_slots.size()) {
    return NULL;
  }

  ImageManager::Image *img = manager->images[tile_slots[tile_index]];
  return img ? img->mem : NULL;
}

VDBImageLoader *ImageHandle::vdb_loader(const int tile_index) const
{
  if (tile_index >= tile_slots.size()) {
    return NULL;
  }

  ImageManager::Image *img = manager->images[tile_slots[tile_index]];

  if (img == NULL) {
    return NULL;
  }

  ImageLoader *loader = img->loader;

  if (loader == NULL) {
    return NULL;
  }

  if (loader->is_vdb_loader()) {
    return dynamic_cast<VDBImageLoader *>(loader);
  }

  return NULL;
}

bool ImageHandle::operator==(const ImageHandle &other) const
{
  return manager == other.manager && tile_slots == other.tile_slots;
}

/* Image MetaData */

ImageMetaData::ImageMetaData()
    : channels(0),
      width(0),
      height(0),
      depth(0),
      byte_size(0),
      type(IMAGE_DATA_NUM_TYPES),
      colorspace(u_colorspace_raw),
      colorspace_file_format(""),
      use_transform_3d(false),
      compress_as_srgb(false)
{
}

bool ImageMetaData::operator==(const ImageMetaData &other) const
{
  return channels == other.channels && width == other.width && height == other.height &&
         depth == other.depth && use_transform_3d == other.use_transform_3d &&
         (!use_transform_3d || transform_3d == other.transform_3d) && type == other.type &&
         colorspace == other.colorspace && compress_as_srgb == other.compress_as_srgb;
}

bool ImageMetaData::is_float() const
{
  return (type == IMAGE_DATA_TYPE_FLOAT || type == IMAGE_DATA_TYPE_FLOAT4 ||
          type == IMAGE_DATA_TYPE_HALF || type == IMAGE_DATA_TYPE_HALF4);
}

void ImageMetaData::detect_colorspace()
{
  /* Convert used specified color spaces to one we know how to handle. */
  colorspace = ColorSpaceManager::detect_known_colorspace(
      colorspace, colorspace_file_hint.c_str(), colorspace_file_format, is_float());

  if (colorspace == u_colorspace_raw) {
    /* Nothing to do. */
  }
  else if (colorspace == u_colorspace_srgb) {
    /* Keep sRGB colorspace stored as sRGB, to save memory and/or loading time
     * for the common case of 8bit sRGB images like PNG. */
    compress_as_srgb = true;
  }
  else {
    /* If colorspace conversion needed, use half instead of short so we can
     * represent HDR values that might result from conversion. */
    if (type == IMAGE_DATA_TYPE_BYTE || type == IMAGE_DATA_TYPE_USHORT) {
      type = IMAGE_DATA_TYPE_HALF;
    }
    else if (type == IMAGE_DATA_TYPE_BYTE4 || type == IMAGE_DATA_TYPE_USHORT4) {
      type = IMAGE_DATA_TYPE_HALF4;
    }
  }
}

/* Image Loader */

ImageLoader::ImageLoader()
{
}

ustring ImageLoader::osl_filepath() const
{
  return ustring();
}

int ImageLoader::get_tile_number() const
{
  return 0;
}

bool ImageLoader::equals(const ImageLoader *a, const ImageLoader *b)
{
  if (a == NULL && b == NULL) {
    return true;
  }
  else {
    return (a && b && typeid(*a) == typeid(*b) && a->equals(*b));
  }
}

bool ImageLoader::is_vdb_loader() const
{
  return false;
}

/* Image Manager */

ImageManager::ImageManager(const DeviceInfo &info)
{
  need_update_ = true;
  osl_texture_system = NULL;
  animation_frame = 0;

  /* Set image limits */
  features.has_nanovdb = info.has_nanovdb;
}

ImageManager::~ImageManager()
{
  for (size_t slot = 0; slot < images.size(); slot++)
    assert(!images[slot]);
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
  osl_texture_system = texture_system;
}

bool ImageManager::set_animation_frame_update(int frame)
{
  if (frame != animation_frame) {
    thread_scoped_lock device_lock(images_mutex);
    animation_frame = frame;

    for (size_t slot = 0; slot < images.size(); slot++) {
      if (images[slot] && images[slot]->params.animated)
        return true;
    }
  }

  return false;
}

void ImageManager::load_image_metadata(Image *img)
{
  if (!img->need_metadata) {
    return;
  }

  thread_scoped_lock image_lock(img->mutex);
  if (!img->need_metadata) {
    return;
  }

  ImageMetaData &metadata = img->metadata;
  metadata = ImageMetaData();
  metadata.colorspace = img->params.colorspace;

  if (img->loader->load_metadata(features, metadata)) {
    assert(metadata.type != IMAGE_DATA_NUM_TYPES);
  }
  else {
    metadata.type = IMAGE_DATA_TYPE_BYTE4;
  }

  metadata.detect_colorspace();

  assert(features.has_nanovdb || (metadata.type != IMAGE_DATA_TYPE_NANOVDB_FLOAT ||
                                  metadata.type != IMAGE_DATA_TYPE_NANOVDB_FLOAT3 ||
                                  metadata.type != IMAGE_DATA_TYPE_NANOVDB_FPN ||
                                  metadata.type != IMAGE_DATA_TYPE_NANOVDB_FP16));

  img->need_metadata = false;
}

ImageHandle ImageManager::add_image(const string &filename, const ImageParams &params)
{
  const int slot = add_image_slot(new OIIOImageLoader(filename), params, false);

  ImageHandle handle;
  handle.tile_slots.push_back(slot);
  handle.manager = this;
  return handle;
}

ImageHandle ImageManager::add_image(const string &filename,
                                    const ImageParams &params,
                                    const array<int> &tiles)
{
  ImageHandle handle;
  handle.manager = this;

  foreach (int tile, tiles) {
    string tile_filename = filename;

    /* Since we don't have information about the exact tile format used in this code location,
     * just attempt all replacement patterns that Blender supports. */
    if (tile != 0) {
      string_replace(tile_filename, "<UDIM>", string_printf("%04d", tile));

      int u = ((tile - 1001) % 10);
      int v = ((tile - 1001) / 10);
      string_replace(tile_filename, "<UVTILE>", string_printf("u%d_v%d", u + 1, v + 1));
    }
    const int slot = add_image_slot(new OIIOImageLoader(tile_filename), params, false);
    handle.tile_slots.push_back(slot);
  }

  return handle;
}

ImageHandle ImageManager::add_image(ImageLoader *loader,
                                    const ImageParams &params,
                                    const bool builtin)
{
  const int slot = add_image_slot(loader, params, builtin);

  ImageHandle handle;
  handle.tile_slots.push_back(slot);
  handle.manager = this;
  return handle;
}

ImageHandle ImageManager::add_image(const vector<ImageLoader *> &loaders,
                                    const ImageParams &params)
{
  ImageHandle handle;
  for (ImageLoader *loader : loaders) {
    const int slot = add_image_slot(loader, params, true);
    handle.tile_slots.push_back(slot);
  }

  handle.manager = this;
  return handle;
}

int ImageManager::add_image_slot(ImageLoader *loader,
                                 const ImageParams &params,
                                 const bool builtin)
{
  Image *img;
  size_t slot;

  thread_scoped_lock device_lock(images_mutex);

  /* Find existing image. */
  for (slot = 0; slot < images.size(); slot++) {
    img = images[slot];
    if (img && ImageLoader::equals(img->loader, loader) && img->params == params) {
      img->users++;
      delete loader;
      return slot;
    }
  }

  /* Find free slot. */
  for (slot = 0; slot < images.size(); slot++) {
    if (!images[slot])
      break;
  }

  if (slot == images.size()) {
    images.resize(images.size() + 1);
  }

  /* Add new image. */
  img = new Image();
  img->params = params;
  img->loader = loader;
  img->need_metadata = true;
  img->need_load = !(osl_texture_system && !img->loader->osl_filepath().empty());
  img->builtin = builtin;
  img->users = 1;
  img->mem = NULL;

  images[slot] = img;

  need_update_ = true;

  return slot;
}

void ImageManager::add_image_user(int slot)
{
  thread_scoped_lock device_lock(images_mutex);
  Image *image = images[slot];
  assert(image && image->users >= 1);

  image->users++;
}

void ImageManager::remove_image_user(int slot)
{
  thread_scoped_lock device_lock(images_mutex);
  Image *image = images[slot];
  assert(image && image->users >= 1);

  /* decrement user count */
  image->users--;

  /* don't remove immediately, rather do it all together later on. one of
   * the reasons for this is that on shader changes we add and remove nodes
   * that use them, but we do not want to reload the image all the time. */
  if (image->users == 0)
    need_update_ = true;
}

static bool image_associate_alpha(ImageManager::Image *img)
{
  /* For typical RGBA images we let OIIO convert to associated alpha,
   * but some types we want to leave the RGB channels untouched. */
  return !(ColorSpaceManager::colorspace_is_data(img->params.colorspace) ||
           img->params.alpha_type == IMAGE_ALPHA_IGNORE ||
           img->params.alpha_type == IMAGE_ALPHA_CHANNEL_PACKED);
}

template<TypeDesc::BASETYPE FileFormat, typename StorageType>
bool ImageManager::file_load_image(Image *img, int texture_limit)
{
  /* Ignore empty images. */
  if (!(img->metadata.channels > 0)) {
    return false;
  }

  /* Get metadata. */
  int width = img->metadata.width;
  int height = img->metadata.height;
  int depth = img->metadata.depth;
  int components = img->metadata.channels;

  /* Read pixels. */
  vector<StorageType> pixels_storage;
  StorageType *pixels;
  const size_t max_size = max(max(width, height), depth);
  if (max_size == 0) {
    /* Don't bother with empty images. */
    return false;
  }

  /* Allocate memory as needed, may be smaller to resize down. */
  if (texture_limit > 0 && max_size > texture_limit) {
    pixels_storage.resize(((size_t)width) * height * depth * 4);
    pixels = &pixels_storage[0];
  }
  else {
    thread_scoped_lock device_lock(device_mutex);
    pixels = (StorageType *)img->mem->alloc(width, height, depth);
  }

  if (pixels == NULL) {
    /* Could be that we've run out of memory. */
    return false;
  }

  const size_t num_pixels = ((size_t)width) * height * depth;
  img->loader->load_pixels(
      img->metadata, pixels, num_pixels * components, image_associate_alpha(img));

  /* The kernel can handle 1 and 4 channel images. Anything that is not a single
   * channel image is converted to RGBA format. */
  bool is_rgba = (img->metadata.type == IMAGE_DATA_TYPE_FLOAT4 ||
                  img->metadata.type == IMAGE_DATA_TYPE_HALF4 ||
                  img->metadata.type == IMAGE_DATA_TYPE_BYTE4 ||
                  img->metadata.type == IMAGE_DATA_TYPE_USHORT4);

  if (is_rgba) {
    const StorageType one = util_image_cast_from_float<StorageType>(1.0f);

    if (components == 2) {
      /* Grayscale + alpha to RGBA. */
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = pixels[i * 2 + 1];
        pixels[i * 4 + 2] = pixels[i * 2 + 0];
        pixels[i * 4 + 1] = pixels[i * 2 + 0];
        pixels[i * 4 + 0] = pixels[i * 2 + 0];
      }
    }
    else if (components == 3) {
      /* RGB to RGBA. */
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = one;
        pixels[i * 4 + 2] = pixels[i * 3 + 2];
        pixels[i * 4 + 1] = pixels[i * 3 + 1];
        pixels[i * 4 + 0] = pixels[i * 3 + 0];
      }
    }
    else if (components == 1) {
      /* Grayscale to RGBA. */
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = one;
        pixels[i * 4 + 2] = pixels[i];
        pixels[i * 4 + 1] = pixels[i];
        pixels[i * 4 + 0] = pixels[i];
      }
    }

    /* Disable alpha if requested by the user. */
    if (img->params.alpha_type == IMAGE_ALPHA_IGNORE) {
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = one;
      }
    }
  }

  if (img->metadata.colorspace != u_colorspace_raw &&
      img->metadata.colorspace != u_colorspace_srgb) {
    /* Convert to scene linear. */
    ColorSpaceManager::to_scene_linear(
        img->metadata.colorspace, pixels, num_pixels, is_rgba, img->metadata.compress_as_srgb);
  }

  /* Make sure we don't have buggy values. */
  if (FileFormat == TypeDesc::FLOAT) {
    /* For RGBA buffers we put all channels to 0 if either of them is not
     * finite. This way we avoid possible artifacts caused by fully changed
     * hue. */
    if (is_rgba) {
      for (size_t i = 0; i < num_pixels; i += 4) {
        StorageType *pixel = &pixels[i * 4];
        if (!isfinite(pixel[0]) || !isfinite(pixel[1]) || !isfinite(pixel[2]) ||
            !isfinite(pixel[3])) {
          pixel[0] = 0;
          pixel[1] = 0;
          pixel[2] = 0;
          pixel[3] = 0;
        }
      }
    }
    else {
      for (size_t i = 0; i < num_pixels; ++i) {
        StorageType *pixel = &pixels[i];
        if (!isfinite(pixel[0])) {
          pixel[0] = 0;
        }
      }
    }
  }

  /* Scale image down if needed. */
  if (pixels_storage.size() > 0) {
    float scale_factor = 1.0f;
    while (max_size * scale_factor > texture_limit) {
      scale_factor *= 0.5f;
    }
    VLOG_WORK << "Scaling image " << img->loader->name() << " by a factor of " << scale_factor
              << ".";
    vector<StorageType> scaled_pixels;
    size_t scaled_width, scaled_height, scaled_depth;
    util_image_resize_pixels(pixels_storage,
                             width,
                             height,
                             depth,
                             is_rgba ? 4 : 1,
                             scale_factor,
                             &scaled_pixels,
                             &scaled_width,
                             &scaled_height,
                             &scaled_depth);

    StorageType *texture_pixels;

    {
      thread_scoped_lock device_lock(device_mutex);
      texture_pixels = (StorageType *)img->mem->alloc(scaled_width, scaled_height, scaled_depth);
    }

    memcpy(texture_pixels, &scaled_pixels[0], scaled_pixels.size() * sizeof(StorageType));
  }

  return true;
}

void ImageManager::device_load_image(Device *device, Scene *scene, int slot, Progress *progress)
{
  if (progress->get_cancel()) {
    return;
  }

  Image *img = images[slot];

  progress->set_status("Updating Images", "Loading " + img->loader->name());

  const int texture_limit = scene->params.texture_limit;

  load_image_metadata(img);
  ImageDataType type = img->metadata.type;

  /* Name for debugging. */
  img->mem_name = string_printf("tex_image_%s_%03d", name_from_type(type), slot);

  /* Free previous texture in slot. */
  if (img->mem) {
    thread_scoped_lock device_lock(device_mutex);
    delete img->mem;
    img->mem = NULL;
  }

  img->mem = new device_texture(
      device, img->mem_name.c_str(), slot, type, img->params.interpolation, img->params.extension);
  img->mem->info.use_transform_3d = img->metadata.use_transform_3d;
  img->mem->info.transform_3d = img->metadata.transform_3d;

  /* Create new texture. */
  if (type == IMAGE_DATA_TYPE_FLOAT4) {
    if (!file_load_image<TypeDesc::FLOAT, float>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      float *pixels = (float *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
      pixels[1] = TEX_IMAGE_MISSING_G;
      pixels[2] = TEX_IMAGE_MISSING_B;
      pixels[3] = TEX_IMAGE_MISSING_A;
    }
  }
  else if (type == IMAGE_DATA_TYPE_FLOAT) {
    if (!file_load_image<TypeDesc::FLOAT, float>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      float *pixels = (float *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
    }
  }
  else if (type == IMAGE_DATA_TYPE_BYTE4) {
    if (!file_load_image<TypeDesc::UINT8, uchar>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      uchar *pixels = (uchar *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 255);
      pixels[1] = (TEX_IMAGE_MISSING_G * 255);
      pixels[2] = (TEX_IMAGE_MISSING_B * 255);
      pixels[3] = (TEX_IMAGE_MISSING_A * 255);
    }
  }
  else if (type == IMAGE_DATA_TYPE_BYTE) {
    if (!file_load_image<TypeDesc::UINT8, uchar>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      uchar *pixels = (uchar *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 255);
    }
  }
  else if (type == IMAGE_DATA_TYPE_HALF4) {
    if (!file_load_image<TypeDesc::HALF, half>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      half *pixels = (half *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
      pixels[1] = TEX_IMAGE_MISSING_G;
      pixels[2] = TEX_IMAGE_MISSING_B;
      pixels[3] = TEX_IMAGE_MISSING_A;
    }
  }
  else if (type == IMAGE_DATA_TYPE_USHORT) {
    if (!file_load_image<TypeDesc::USHORT, uint16_t>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      uint16_t *pixels = (uint16_t *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 65535);
    }
  }
  else if (type == IMAGE_DATA_TYPE_USHORT4) {
    if (!file_load_image<TypeDesc::USHORT, uint16_t>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      uint16_t *pixels = (uint16_t *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 65535);
      pixels[1] = (TEX_IMAGE_MISSING_G * 65535);
      pixels[2] = (TEX_IMAGE_MISSING_B * 65535);
      pixels[3] = (TEX_IMAGE_MISSING_A * 65535);
    }
  }
  else if (type == IMAGE_DATA_TYPE_HALF) {
    if (!file_load_image<TypeDesc::HALF, half>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      thread_scoped_lock device_lock(device_mutex);
      half *pixels = (half *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
    }
  }
#ifdef WITH_NANOVDB
  else if (type == IMAGE_DATA_TYPE_NANOVDB_FLOAT || type == IMAGE_DATA_TYPE_NANOVDB_FLOAT3 ||
           type == IMAGE_DATA_TYPE_NANOVDB_FPN || type == IMAGE_DATA_TYPE_NANOVDB_FP16) {
    thread_scoped_lock device_lock(device_mutex);
    void *pixels = img->mem->alloc(img->metadata.byte_size, 0);

    if (pixels != NULL) {
      img->loader->load_pixels(img->metadata, pixels, img->metadata.byte_size, false);
    }
  }
#endif

  {
    thread_scoped_lock device_lock(device_mutex);
    img->mem->copy_to_device();
  }

  /* Cleanup memory in image loader. */
  img->loader->cleanup();
  img->need_load = false;
}

void ImageManager::device_free_image(Device *, int slot)
{
  Image *img = images[slot];
  if (img == NULL) {
    return;
  }

  if (osl_texture_system) {
#ifdef WITH_OSL
    ustring filepath = img->loader->osl_filepath();
    if (!filepath.empty()) {
      ((OSL::TextureSystem *)osl_texture_system)->invalidate(filepath);
    }
#endif
  }

  if (img->mem) {
    thread_scoped_lock device_lock(device_mutex);
    delete img->mem;
  }

  delete img->loader;
  delete img;
  images[slot] = NULL;
}

void ImageManager::device_update(Device *device, Scene *scene, Progress &progress)
{
  if (!need_update()) {
    return;
  }

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->image.times.add_entry({"device_update", time});
    }
  });

  TaskPool pool;
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot];
    if (img && img->users == 0) {
      device_free_image(device, slot);
    }
    else if (img && img->need_load) {
      pool.push(
          function_bind(&ImageManager::device_load_image, this, device, scene, slot, &progress));
    }
  }

  pool.wait_work();

  need_update_ = false;
}

void ImageManager::device_update_slot(Device *device, Scene *scene, int slot, Progress *progress)
{
  Image *img = images[slot];
  assert(img != NULL);

  if (img->users == 0) {
    device_free_image(device, slot);
  }
  else if (img->need_load) {
    device_load_image(device, scene, slot, progress);
  }
}

void ImageManager::device_load_builtin(Device *device, Scene *scene, Progress &progress)
{
  /* Load only builtin images, Blender needs this to load evaluated
   * scene data from depsgraph before it is freed. */
  if (!need_update()) {
    return;
  }

  TaskPool pool;
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot];
    if (img && img->need_load && img->builtin) {
      pool.push(
          function_bind(&ImageManager::device_load_image, this, device, scene, slot, &progress));
    }
  }

  pool.wait_work();
}

void ImageManager::device_free_builtin(Device *device)
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot];
    if (img && img->builtin) {
      device_free_image(device, slot);
    }
  }
}

void ImageManager::device_free(Device *device)
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    device_free_image(device, slot);
  }
  images.clear();
}

void ImageManager::collect_statistics(RenderStats *stats)
{
  foreach (const Image *image, images) {
    if (!image) {
      /* Image may have been freed due to lack of users. */
      continue;
    }
    stats->image.textures.add_entry(
        NamedSizeEntry(image->loader->name(), image->mem->memory_size()));
  }
}

void ImageManager::tag_update()
{
  need_update_ = true;
}

bool ImageManager::need_update() const
{
  return need_update_;
}

CCL_NAMESPACE_END
