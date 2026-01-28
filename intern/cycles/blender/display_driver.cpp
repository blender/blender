/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "GPU_context.hh"
#include "GPU_immediate.hh"
#include "GPU_platform.hh"
#include "GPU_platform_backend_enum.h"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "RE_engine.h"

#include "blender/display_driver.h"

#include "util/log.h"
#include "util/math.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * BlenderDisplayShader.
 */

unique_ptr<BlenderDisplayShader> BlenderDisplayShader::create(blender::RenderEngine &b_engine,
                                                              blender::Scene &b_scene)
{
  /* See #engine_support_display_space_shader in rna_render.cc. */
  if (true) {
    return make_unique<BlenderDisplaySpaceShader>(b_engine, b_scene);
  }

  return make_unique<BlenderFallbackDisplayShader>();
}

int BlenderDisplayShader::get_position_attrib_location()
{
  if (position_attribute_location_ == -1) {
    blender::gpu::Shader *shader_program = get_shader_program();
    position_attribute_location_ = blender::GPU_shader_get_attribute(shader_program,
                                                                     position_attribute_name);
  }
  return position_attribute_location_;
}

int BlenderDisplayShader::get_tex_coord_attrib_location()
{
  if (tex_coord_attribute_location_ == -1) {
    blender::gpu::Shader *shader_program = get_shader_program();
    tex_coord_attribute_location_ = blender::GPU_shader_get_attribute(shader_program,
                                                                      tex_coord_attribute_name);
  }
  return tex_coord_attribute_location_;
}

/* --------------------------------------------------------------------
 * BlenderFallbackDisplayShader.
 */
static blender::gpu::Shader *compile_fallback_shader()
{
  /* NOTE: Compilation errors are logged to console. */
  blender::gpu::Shader *shader = blender::GPU_shader_create_from_info_name(
      "gpu_shader_cycles_display_fallback");
  return shader;
}

BlenderFallbackDisplayShader::~BlenderFallbackDisplayShader()
{
  destroy_shader();
}

blender::gpu::Shader *BlenderFallbackDisplayShader::bind(const int width, const int height)
{
  create_shader_if_needed();

  if (!shader_program_) {
    return nullptr;
  }

  /* Bind shader now to enable uniform assignment. */
  blender::GPU_shader_bind(shader_program_);
  const int slot = 0;
  blender::GPU_shader_uniform_int_ex(shader_program_, image_texture_location_, 1, 1, &slot);
  float size[2];
  size[0] = width;
  size[1] = height;
  blender::GPU_shader_uniform_float_ex(shader_program_, fullscreen_location_, 2, 1, size);
  return shader_program_;
}

void BlenderFallbackDisplayShader::unbind()
{
  blender::GPU_shader_unbind();
}

blender::gpu::Shader *BlenderFallbackDisplayShader::get_shader_program()
{
  return shader_program_;
}

void BlenderFallbackDisplayShader::create_shader_if_needed()
{
  if (shader_program_ || shader_compile_attempted_) {
    return;
  }

  shader_compile_attempted_ = true;

  shader_program_ = compile_fallback_shader();
  if (!shader_program_) {
    LOG_ERROR << "Failed to compile fallback shader";
    return;
  }

  image_texture_location_ = blender::GPU_shader_get_uniform(shader_program_, "image_texture");
  if (image_texture_location_ < 0) {
    LOG_ERROR << "Shader doesn't contain the 'image_texture' uniform.";
    destroy_shader();
    return;
  }

  fullscreen_location_ = blender::GPU_shader_get_uniform(shader_program_, "fullscreen");
  if (fullscreen_location_ < 0) {
    LOG_ERROR << "Shader doesn't contain the 'fullscreen' uniform.";
    destroy_shader();
    return;
  }
}

void BlenderFallbackDisplayShader::destroy_shader()
{
  if (shader_program_) {
    blender::GPU_shader_free(shader_program_);
    shader_program_ = nullptr;
  }
}

/* --------------------------------------------------------------------
 * BlenderDisplaySpaceShader.
 */

BlenderDisplaySpaceShader::BlenderDisplaySpaceShader(blender::RenderEngine &b_engine,
                                                     blender::Scene &b_scene)
    : b_engine_(b_engine), b_scene_(b_scene)
{
}

blender::gpu::Shader *BlenderDisplaySpaceShader::bind(int /*width*/, int /*height*/)
{
  blender::gpu::Shader *shader = blender::GPU_shader_get_builtin_shader(
      blender::GPU_SHADER_3D_IMAGE);
  blender::GPU_shader_bind(shader);
  /** \note "image" binding slot is 0. */
  return blender::GPU_shader_get_bound();
}

void BlenderDisplaySpaceShader::unbind()
{
  blender::GPU_shader_unbind();
}

blender::gpu::Shader *BlenderDisplaySpaceShader::get_shader_program()
{
  if (!shader_program_) {
    shader_program_ = blender::GPU_shader_get_bound();
  }
  if (!shader_program_) {
    LOG_ERROR << "Error retrieving shader program for display space shader.";
  }

  return shader_program_;
}

/* --------------------------------------------------------------------
 * DrawTile.
 */

/* Higher level representation of a texture from the graphics library. */
class DisplayGPUTexture {
 public:
  /* Global counter for all allocated blender::GPUTextures used by instances of this class. */
  static inline std::atomic<int> num_used = 0;

  DisplayGPUTexture() = default;

  ~DisplayGPUTexture()
  {
    assert(gpu_texture == nullptr);
  }

  DisplayGPUTexture(const DisplayGPUTexture &other) = delete;
  DisplayGPUTexture &operator=(DisplayGPUTexture &other) = delete;

  DisplayGPUTexture(DisplayGPUTexture &&other) noexcept
      : gpu_texture(other.gpu_texture), width(other.width), height(other.height)
  {
    other.reset();
  }

  DisplayGPUTexture &operator=(DisplayGPUTexture &&other)
  {
    if (this == &other) {
      return *this;
    }

    gpu_texture = other.gpu_texture;
    width = other.width;
    height = other.height;

    other.reset();

    return *this;
  }

  bool gpu_resources_ensure(const uint texture_width, const uint texture_height)
  {
    if (width != texture_width || height != texture_height) {
      gpu_resources_destroy();
    }

    if (gpu_texture) {
      return true;
    }

    width = texture_width;
    height = texture_height;

    /* Texture must have a minimum size of 1x1. */
    gpu_texture = blender::GPU_texture_create_2d("CyclesBlitTexture",
                                                 max(width, 1),
                                                 max(height, 1),
                                                 1,
                                                 blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
                                                 blender::GPU_TEXTURE_USAGE_GENERAL,
                                                 nullptr);

    if (!gpu_texture) {
      LOG_ERROR << "Error creating texture.";
      return false;
    }

    blender::GPU_texture_filter_mode(gpu_texture, false);
    blender::GPU_texture_extend_mode(gpu_texture, blender::GPU_SAMPLER_EXTEND_MODE_EXTEND);

    ++num_used;

    return true;
  }

  void gpu_resources_destroy()
  {
    if (gpu_texture == nullptr) {
      return;
    }

    GPU_TEXTURE_FREE_SAFE(gpu_texture);

    reset();

    --num_used;
  }

  /* Texture resource allocated by the blender::GPU module.
   *
   * NOTE: Allocated on the render engine's context. */
  blender::gpu::Texture *gpu_texture = nullptr;

  /* Dimensions of the texture in pixels. */
  int width = 0;
  int height = 0;

 protected:
  void reset()
  {
    gpu_texture = nullptr;
    width = 0;
    height = 0;
  }
};

/* Higher level representation of a Pixel Buffer Object (PBO) from the graphics library. */
class DisplayGPUPixelBuffer {
 public:
  /* Global counter for all allocated blender::GPU module PBOs used by instances of this class. */
  static inline std::atomic<int> num_used = 0;

  DisplayGPUPixelBuffer() = default;

  ~DisplayGPUPixelBuffer()
  {
    assert(gpu_pixel_buffer == nullptr);
  }

  DisplayGPUPixelBuffer(const DisplayGPUPixelBuffer &other) = delete;
  DisplayGPUPixelBuffer &operator=(DisplayGPUPixelBuffer &other) = delete;

  DisplayGPUPixelBuffer(DisplayGPUPixelBuffer &&other) noexcept
      : gpu_pixel_buffer(other.gpu_pixel_buffer), width(other.width), height(other.height)
  {
    other.reset();
  }

  DisplayGPUPixelBuffer &operator=(DisplayGPUPixelBuffer &&other)
  {
    if (this == &other) {
      return *this;
    }

    gpu_pixel_buffer = other.gpu_pixel_buffer;
    width = other.width;
    height = other.height;

    other.reset();

    return *this;
  }

  bool gpu_resources_ensure(const uint new_width, const uint new_height, bool &buffer_recreated)
  {
    buffer_recreated = false;

    const size_t required_size = sizeof(half4) * new_width * new_height;

    /* Try to re-use the existing PBO if it has usable size. */
    if (gpu_pixel_buffer) {
      if (new_width != width || new_height != height ||
          blender::GPU_pixel_buffer_size(gpu_pixel_buffer) < required_size)
      {
        buffer_recreated = true;
        gpu_resources_destroy();
      }
    }

    /* Update size. */
    width = new_width;
    height = new_height;

    /* Create pixel buffer if not already created. */
    if (!gpu_pixel_buffer) {
      gpu_pixel_buffer = blender::GPU_pixel_buffer_create(required_size);
      buffer_recreated = true;
    }

    if (gpu_pixel_buffer == nullptr) {
      LOG_ERROR << "Error creating texture pixel buffer object.";
      return false;
    }

    ++num_used;

    return true;
  }

  void gpu_resources_destroy()
  {
    if (!gpu_pixel_buffer) {
      return;
    }

    blender::GPU_pixel_buffer_free(gpu_pixel_buffer);
    gpu_pixel_buffer = nullptr;

    reset();

    --num_used;
  }

  /* Pixel Buffer Object allocated by the blender::GPU module.
   *
   * NOTE: Allocated on the render engine's context. */
  blender::GPUPixelBuffer *gpu_pixel_buffer = nullptr;

  /* Dimensions of the PBO. */
  int width = 0;
  int height = 0;

 protected:
  void reset()
  {
    gpu_pixel_buffer = nullptr;
    width = 0;
    height = 0;
  }
};

class DrawTile {
 public:
  DrawTile() = default;
  ~DrawTile() = default;

  DrawTile(const DrawTile &other) = delete;
  DrawTile &operator=(const DrawTile &other) = delete;

  DrawTile(DrawTile &&other) noexcept = default;

  DrawTile &operator=(DrawTile &&other) = default;

  void gpu_resources_destroy()
  {
    texture.gpu_resources_destroy();
  }

  bool ready_to_draw() const
  {
    return texture.gpu_texture != nullptr;
  }

  /* Texture which contains pixels of the tile. */
  DisplayGPUTexture texture;

  /* Display parameters the texture of this tile has been updated for. */
  BlenderDisplayDriver::Params params;
};

class DrawTileAndPBO {
 public:
  void gpu_resources_destroy()
  {
    tile.gpu_resources_destroy();
    buffer_object.gpu_resources_destroy();
  }

  DrawTile tile;
  DisplayGPUPixelBuffer buffer_object;
  bool need_update_texture_pixels = false;
};

/* --------------------------------------------------------------------
 * BlenderDisplayDriver.
 */

struct BlenderDisplayDriver::Tiles {
  /* Resources of a tile which is being currently rendered. */
  DrawTileAndPBO current_tile;

  /* All tiles which rendering is finished and which content will not be changed. */
  struct {
    vector<DrawTile> tiles;

    void gl_resources_destroy_and_clear()
    {
      for (DrawTile &tile : tiles) {
        tile.gpu_resources_destroy();
      }

      tiles.clear();
    }
  } finished_tiles;
};

BlenderDisplayDriver::BlenderDisplayDriver(blender::RenderEngine &b_engine,
                                           blender::Scene &b_scene,
                                           blender::RegionView3D *b_rv3d,
                                           const bool background)
    : b_engine_(b_engine),
      b_rv3d_(b_rv3d),
      background_(background),
      display_shader_(BlenderDisplayShader::create(b_engine, b_scene)),
      tiles_(make_unique<Tiles>())
{
  /* Create context while on the main thread. */
  gpu_context_create();
}

BlenderDisplayDriver::~BlenderDisplayDriver()
{
  gpu_resources_destroy();
}

/* --------------------------------------------------------------------
 * Update procedure.
 */

void BlenderDisplayDriver::next_tile_begin()
{
  if (!tiles_->current_tile.tile.ready_to_draw()) {
    LOG_ERROR
        << "Unexpectedly moving to the next tile without any data provided for current tile.";
    return;
  }

  /* Moving to the next tile without giving render data for the current tile is not an expected
   * situation. */
  DCHECK(!need_zero_);
  /* Texture should have been updated from the PBO at this point. */
  DCHECK(!tiles_->current_tile.need_update_texture_pixels);

  tiles_->finished_tiles.tiles.emplace_back(std::move(tiles_->current_tile.tile));
}

bool BlenderDisplayDriver::update_begin(const Params &params,
                                        const int texture_width,
                                        const int texture_height)
{
  /* Note that it's the responsibility of BlenderDisplayDriver to ensure updating and drawing
   * the texture does not happen at the same time. This is achieved indirectly.
   *
   * When enabling the OpenGL/GPU context, it uses an internal mutex lock DST.gpu_context_lock.
   * This same lock is also held when do_draw() is called, which together ensure mutual
   * exclusion.
   *
   * This locking is not performed on the Cycles side, because that would cause lock inversion. */
  if (!gpu_context_enable()) {
    return false;
  }

  blender::GPU_fence_wait(gpu_render_sync_);

  DrawTile &current_tile = tiles_->current_tile.tile;
  DisplayGPUPixelBuffer &current_tile_buffer_object = tiles_->current_tile.buffer_object;

  /* Clear storage of all finished tiles when display clear is requested.
   * Do it when new tile data is provided to handle the display clear flag in a single place.
   * It also makes the logic reliable from the whether drawing did happen or not point of view. */
  if (need_zero_) {
    tiles_->finished_tiles.gl_resources_destroy_and_clear();
    need_zero_ = false;
  }

  /* Update PBO dimensions if needed.
   *
   * NOTE: Allocate the PBO for the size which will fit the final render resolution (as in,
   * at a resolution divider 1. This was we don't need to recreate graphics interoperability
   * objects which are costly and which are tied to the specific underlying buffer size.
   * The downside of this approach is that when graphics interoperability is not used we are
   * sending too much data to blender::GPU when resolution divider is not 1. */
  /* TODO(sergey): Investigate whether keeping the PBO exact size of the texture makes non-interop
   * mode faster. */
  const int buffer_width = params.size.x;
  const int buffer_height = params.size.y;
  bool interop_recreated = false;

  if (!current_tile_buffer_object.gpu_resources_ensure(
          buffer_width, buffer_height, interop_recreated) ||
      !current_tile.texture.gpu_resources_ensure(texture_width, texture_height))
  {
    graphics_interop_buffer_.clear();
    tiles_->current_tile.gpu_resources_destroy();
    gpu_context_disable();
    return false;
  }

  if (interop_recreated) {
    graphics_interop_buffer_.clear();
  }

  /* Store an updated parameters of the current tile.
   * In theory it is only needed once per update of the tile, but doing it on every update is
   * the easiest and is not expensive. */
  tiles_->current_tile.tile.params = params;

  return true;
}

static void update_tile_texture_pixels(const DrawTileAndPBO &tile)
{
  const DisplayGPUTexture &texture = tile.tile.texture;

  if (!DCHECK_NOTNULL(tile.buffer_object.gpu_pixel_buffer)) {
    LOG_ERROR << "Display driver tile pixel buffer unavailable.";
    return;
  }
  blender::GPU_texture_update_sub_from_pixel_buffer(texture.gpu_texture,
                                                    blender::GPU_DATA_HALF_FLOAT,
                                                    tile.buffer_object.gpu_pixel_buffer,
                                                    0,
                                                    0,
                                                    0,
                                                    texture.width,
                                                    texture.height,
                                                    0);
}

void BlenderDisplayDriver::update_end()
{
  /* Unpack the PBO into the texture as soon as the new content is provided.
   *
   * This allows to ensure that the unpacking happens while resources like graphics interop (which
   * lifetime is outside of control of the display driver) are still valid, as well as allows to
   * move the tile from being current to finished blender::immediately after this call.
   *
   * One concern with this approach is that if the update happens more often than drawing then
   * doing the unpack here occupies blender::GPU transfer for no good reason. However, the render
   * scheduler takes care of ensuring updates don't happen that often. In regular applications
   * redraw will happen much more often than this update.
   *
   * On some older blender::GPUs on macOS, there is a driver crash when updating the texture for
   * viewport renders while Blender is drawing. As a workaround update texture during draw, under
   * assumption that there is no graphics interop on macOS and viewport render has a single tile.
   */
  if (!background_ && blender::GPU_type_matches_ex(blender::GPU_DEVICE_NVIDIA,
                                                   blender::GPU_OS_MAC,
                                                   blender::GPU_DRIVER_ANY,
                                                   blender::GPU_BACKEND_ANY))
  {
    tiles_->current_tile.need_update_texture_pixels = true;
  }
  else {
    update_tile_texture_pixels(tiles_->current_tile);
  }

  /* Ensure blender::GPU fence exists to synchronize upload. */
  blender::GPU_fence_signal(gpu_upload_sync_);

  blender::GPU_flush();

  gpu_context_disable();

  has_update_cond_.notify_all();
}

/* --------------------------------------------------------------------
 * Texture buffer mapping.
 */

half4 *BlenderDisplayDriver::map_texture_buffer()
{
  /* With multi device rendering, Cycles can switch between using graphics interop
   * and not. For the denoised image it may be able to use graphics interop as that
   * buffer is written to by one device, while the noisy renders can not use it.
   *
   * We need to clear the graphics interop buffer on that switch, as blender::GPU_pixel_buffer_map
   * may recreate the buffer or handle. */
  graphics_interop_buffer_.clear();

  blender::GPUPixelBuffer *pix_buf = tiles_->current_tile.buffer_object.gpu_pixel_buffer;
  if (!DCHECK_NOTNULL(pix_buf)) {
    LOG_ERROR << "Display driver tile pixel buffer unavailable.";
    return nullptr;
  }
  half4 *mapped_rgba_pixels = reinterpret_cast<half4 *>(blender::GPU_pixel_buffer_map(pix_buf));
  if (!mapped_rgba_pixels) {
    LOG_ERROR << "Error mapping BlenderDisplayDriver pixel buffer object.";
  }
  return mapped_rgba_pixels;
}

void BlenderDisplayDriver::unmap_texture_buffer()
{
  blender::GPUPixelBuffer *pix_buf = tiles_->current_tile.buffer_object.gpu_pixel_buffer;
  if (!DCHECK_NOTNULL(pix_buf)) {
    LOG_ERROR << "Display driver tile pixel buffer unavailable.";
    return;
  }
  blender::GPU_pixel_buffer_unmap(pix_buf);
}

/* --------------------------------------------------------------------
 * Graphics interoperability.
 */

GraphicsInteropDevice BlenderDisplayDriver::graphics_interop_get_device()
{
  GraphicsInteropDevice interop_device;

  switch (blender::GPU_backend_get_type()) {
    case blender::GPU_BACKEND_OPENGL:
      interop_device.type = GraphicsInteropDevice::OPENGL;
      break;
    case blender::GPU_BACKEND_VULKAN:
      interop_device.type = GraphicsInteropDevice::VULKAN;
      break;
    case blender::GPU_BACKEND_METAL:
      interop_device.type = GraphicsInteropDevice::METAL;
      break;
    case blender::GPU_BACKEND_NONE:
    case blender::GPU_BACKEND_ANY:
      interop_device.type = GraphicsInteropDevice::NONE;
      break;
  }

  blender::Span<uint8_t> uuid = blender::GPU_platform_uuid();
  interop_device.uuid.resize(uuid.size());
  std::copy_n(uuid.data(), uuid.size(), interop_device.uuid.data());

  return interop_device;
}

void BlenderDisplayDriver::graphics_interop_update_buffer()
{
  if (graphics_interop_buffer_.is_empty()) {
    GraphicsInteropDevice::Type type = GraphicsInteropDevice::NONE;
    switch (blender::GPU_backend_get_type()) {
      case blender::GPU_BACKEND_OPENGL:
        type = GraphicsInteropDevice::OPENGL;
        break;
      case blender::GPU_BACKEND_VULKAN:
        type = GraphicsInteropDevice::VULKAN;
        break;
      case blender::GPU_BACKEND_METAL:
        type = GraphicsInteropDevice::METAL;
        break;
      case blender::GPU_BACKEND_NONE:
      case blender::GPU_BACKEND_ANY:
        break;
    }

    blender::GPUPixelBufferNativeHandle handle = blender::GPU_pixel_buffer_get_native_handle(
        tiles_->current_tile.buffer_object.gpu_pixel_buffer);
    graphics_interop_buffer_.assign(type, handle.handle, handle.size);
  }
}

void BlenderDisplayDriver::graphics_interop_activate()
{
  gpu_context_enable();
}

void BlenderDisplayDriver::graphics_interop_deactivate()
{
  gpu_context_disable();
}

/* --------------------------------------------------------------------
 * Drawing.
 */

void BlenderDisplayDriver::zero()
{
  need_zero_ = true;
}

void BlenderDisplayDriver::set_zoom(const float zoom_x, const float zoom_y)
{
  zoom_ = make_float2(zoom_x, zoom_y);
}

/* Update vertex buffer with new coordinates of vertex positions and texture coordinates.
 * This buffer is used to render texture in the viewport.
 *
 * NOTE: The buffer needs to be bound. */
static void vertex_draw(const DisplayDriver::Params &params,
                        const int texcoord_attribute,
                        const int position_attribute)
{
  const int x = params.full_offset.x;
  const int y = params.full_offset.y;

  const int width = params.size.x;
  const int height = params.size.y;

  blender::immBegin(blender::GPU_PRIM_TRI_STRIP, 4);

  blender::immAttr2f(texcoord_attribute, 1.0f, 0.0f);
  blender::immVertex2f(position_attribute, x + width, y);

  blender::immAttr2f(texcoord_attribute, 1.0f, 1.0f);
  blender::immVertex2f(position_attribute, x + width, y + height);

  blender::immAttr2f(texcoord_attribute, 0.0f, 0.0f);
  blender::immVertex2f(position_attribute, x, y);

  blender::immAttr2f(texcoord_attribute, 0.0f, 1.0f);
  blender::immVertex2f(position_attribute, x, y + height);

  blender::immEnd();
}

static void draw_tile(const float2 &zoom,
                      const int texcoord_attribute,
                      const int position_attribute,
                      const DrawTile &draw_tile)
{
  if (!draw_tile.ready_to_draw()) {
    return;
  }

  const DisplayGPUTexture &texture = draw_tile.texture;

  if (!DCHECK_NOTNULL(texture.gpu_texture)) {
    LOG_ERROR << "Display driver tile blender::GPU texture resource unavailable.";
    return;
  }

  /* Trick to keep sharp rendering without jagged edges on all blender::GPUs.
   *
   * The idea here is to enforce driver to use linear interpolation when the image is zoomed out.
   * For the render result with a resolution divider in effect we always use nearest interpolation.
   *
   * Use explicit MIN assignment to make sure the driver does not have an undefined behavior at
   * the zoom level 1. The MAG filter is always NEAREST. */
  const float zoomed_width = draw_tile.params.size.x * zoom.x;
  const float zoomed_height = draw_tile.params.size.y * zoom.y;
  if (texture.width != draw_tile.params.size.x || texture.height != draw_tile.params.size.y) {
    /* Resolution divider is different from 1, force nearest interpolation. */
    blender::GPU_texture_bind_ex(
        texture.gpu_texture, blender::GPUSamplerState::default_sampler(), 0);
  }
  else if (zoomed_width - draw_tile.params.size.x > -0.5f ||
           zoomed_height - draw_tile.params.size.y > -0.5f)
  {
    blender::GPU_texture_bind_ex(
        texture.gpu_texture, blender::GPUSamplerState::default_sampler(), 0);
  }
  else {
    blender::GPU_texture_bind_ex(texture.gpu_texture, {blender::GPU_SAMPLER_FILTERING_LINEAR}, 0);
  }

  /* Draw at the parameters for which the texture has been updated for. This allows to always draw
   * texture during bordered-rendered camera view without flickering. The validness of the display
   * parameters for a texture is guaranteed by the initial "clear" state which makes drawing to
   * have an early output.
   *
   * Such approach can cause some extra "jelly" effect during panning, but it is not more jelly
   * than overlay of selected objects. Also, it's possible to redraw texture at an intersection of
   * the texture draw parameters and the latest updated draw parameters (although, complexity of
   * doing it might not worth it. */
  vertex_draw(draw_tile.params, texcoord_attribute, position_attribute);
}

void BlenderDisplayDriver::flush()
{
  /* This is called from the render thread that also calls update_begin/end, right before ending
   * the render loop. We wait for any queued PBO and render commands to be done, before destroying
   * the render thread and activating the context in the main thread to destroy resources.
   *
   * If we don't do this, the NVIDIA driver hangs for a few seconds for when ending 3D viewport
   * rendering, for unknown reasons. This was found with NVIDIA driver version 470.73 and a Quadro
   * RTX 6000 on Linux. */
  if (!gpu_context_enable()) {
    return;
  }

  blender::GPU_fence_wait(gpu_upload_sync_);
  blender::GPU_fence_wait(gpu_render_sync_);

  gpu_context_disable();
}

void BlenderDisplayDriver::draw(const Params &params)
{
  if (b_rv3d_ && (b_rv3d_->rflag & (blender::RV3D_NAVIGATING | blender::RV3D_PAINTING))) {
    /* Before drawing, wait that an update to the texture has actually occurred, to synchronize
     * rendering of Cycles with Blender. Use a timeout to prevent user interface in the main thread
     * from becoming unresponsive when rendering is too heavy. */
    thread_scoped_lock lock(has_update_mutex_);
    has_update_cond_.wait_for(lock, std::chrono::milliseconds(33));
    lock.unlock();
  }

  gpu_context_lock();

  if (need_zero_) {
    /* Texture is requested to be cleared and was not yet cleared.
     *
     * Do early return which should be equivalent of drawing all-zero texture.
     * Watch out for the lock though so that the clear happening during update is properly
     * synchronized here. */
    gpu_context_unlock();
    return;
  }

  blender::GPU_fence_wait(gpu_upload_sync_);
  blender::GPU_blend(blender::GPU_BLEND_ALPHA_PREMULT);

  blender::gpu::Shader *active_shader = display_shader_->bind(params.full_size.x,
                                                              params.full_size.y);

  blender::GPUVertFormat *format = blender::immVertexFormat();
  const int texcoord_attribute = blender::GPU_vertformat_attr_add(
      format,
      ccl::BlenderDisplayShader::tex_coord_attribute_name,
      blender::gpu::VertAttrType::SFLOAT_32_32);
  const int position_attribute = blender::GPU_vertformat_attr_add(
      format,
      ccl::BlenderDisplayShader::position_attribute_name,
      blender::gpu::VertAttrType::SFLOAT_32_32);

  /* NOTE: Shader is bound again through IMM to register this shader with the IMM module
   * and perform required setup for IMM rendering. This is required as the IMM module
   * needs to be aware of which shader is bound, and the main display shader
   * is bound externally. */
  blender::immBindShader(active_shader);

  if (tiles_->current_tile.need_update_texture_pixels) {
    update_tile_texture_pixels(tiles_->current_tile);
    tiles_->current_tile.need_update_texture_pixels = false;
  }

  draw_tile(zoom_, texcoord_attribute, position_attribute, tiles_->current_tile.tile);

  for (const DrawTile &tile : tiles_->finished_tiles.tiles) {
    draw_tile(zoom_, texcoord_attribute, position_attribute, tile);
  }

  /* Reset IMM shader bind state. */
  blender::immUnbindProgram();

  display_shader_->unbind();

  blender::GPU_blend(blender::GPU_BLEND_NONE);

  blender::GPU_fence_signal(gpu_render_sync_);
  blender::GPU_flush();

  gpu_context_unlock();

  LOG_TRACE << "Display driver number of textures: " << DisplayGPUTexture::num_used;
  LOG_TRACE << "Display driver number of PBOs: " << DisplayGPUPixelBuffer::num_used;
}

void BlenderDisplayDriver::gpu_context_create()
{
  if (!RE_engine_gpu_context_create(&b_engine_)) {
    LOG_ERROR << "Error creating blender::GPU context.";
    return;
  }

  /* Create global blender::GPU resources for display driver. */
  if (!gpu_resources_create()) {
    LOG_ERROR << "Error creating blender::GPU resources for Display Driver.";
    return;
  }
}

bool BlenderDisplayDriver::gpu_context_enable()
{
  return RE_engine_gpu_context_enable(&b_engine_);
}

void BlenderDisplayDriver::gpu_context_disable()
{
  RE_engine_gpu_context_disable(&b_engine_);
}

void BlenderDisplayDriver::gpu_context_destroy()
{
  RE_engine_gpu_context_destroy(&b_engine_);
}

void BlenderDisplayDriver::gpu_context_lock()
{
  RE_engine_gpu_context_lock(&b_engine_);
}

void BlenderDisplayDriver::gpu_context_unlock()
{
  RE_engine_gpu_context_unlock(&b_engine_);
}

bool BlenderDisplayDriver::gpu_resources_create()
{
  /* Ensure context is active for resource creation. */
  if (!gpu_context_enable()) {
    LOG_ERROR << "Error enabling blender::GPU context.";
    return false;
  }

  gpu_upload_sync_ = blender::GPU_fence_create();
  gpu_render_sync_ = blender::GPU_fence_create();

  if (!DCHECK_NOTNULL(gpu_upload_sync_) || !DCHECK_NOTNULL(gpu_render_sync_)) {
    LOG_ERROR << "Error creating blender::GPU synchronization primitives.";
    assert(0);
    return false;
  }

  gpu_context_disable();
  return true;
}

void BlenderDisplayDriver::gpu_resources_destroy()
{
  gpu_context_enable();

  display_shader_.reset();

  graphics_interop_buffer_.clear();

  tiles_->current_tile.gpu_resources_destroy();
  tiles_->finished_tiles.gl_resources_destroy_and_clear();

  /* Fences. */
  if (gpu_render_sync_) {
    blender::GPU_fence_free(gpu_render_sync_);
    gpu_render_sync_ = nullptr;
  }
  if (gpu_upload_sync_) {
    blender::GPU_fence_free(gpu_upload_sync_);
    gpu_upload_sync_ = nullptr;
  }

  gpu_context_disable();

  gpu_context_destroy();
}

CCL_NAMESPACE_END
