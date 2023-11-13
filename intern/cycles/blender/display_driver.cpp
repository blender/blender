/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "GPU_context.h"
#include "GPU_immediate.h"
#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "RE_engine.h"

#include "blender/display_driver.h"

#include "device/device.h"
#include "util/log.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * BlenderDisplayShader.
 */

unique_ptr<BlenderDisplayShader> BlenderDisplayShader::create(BL::RenderEngine &b_engine,
                                                              BL::Scene &b_scene)
{
  if (b_engine.support_display_space_shader(b_scene)) {
    return make_unique<BlenderDisplaySpaceShader>(b_engine, b_scene);
  }

  return make_unique<BlenderFallbackDisplayShader>();
}

int BlenderDisplayShader::get_position_attrib_location()
{
  if (position_attribute_location_ == -1) {
    GPUShader *shader_program = get_shader_program();
    position_attribute_location_ = GPU_shader_get_attribute(shader_program,
                                                            position_attribute_name);
  }
  return position_attribute_location_;
}

int BlenderDisplayShader::get_tex_coord_attrib_location()
{
  if (tex_coord_attribute_location_ == -1) {
    GPUShader *shader_program = get_shader_program();
    tex_coord_attribute_location_ = GPU_shader_get_attribute(shader_program,
                                                             tex_coord_attribute_name);
  }
  return tex_coord_attribute_location_;
}

/* --------------------------------------------------------------------
 * BlenderFallbackDisplayShader.
 */
static GPUShader *compile_fallback_shader(void)
{
  /* NOTE: Compilation errors are logged to console. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_shader_cycles_display_fallback");
  return shader;
}

GPUShader *BlenderFallbackDisplayShader::bind(int width, int height)
{
  create_shader_if_needed();

  if (!shader_program_) {
    return nullptr;
  }

  /* Bind shader now to enable uniform assignment. */
  GPU_shader_bind(shader_program_);
  int slot = 0;
  GPU_shader_uniform_int_ex(shader_program_, image_texture_location_, 1, 1, &slot);
  float size[2];
  size[0] = width;
  size[1] = height;
  GPU_shader_uniform_float_ex(shader_program_, fullscreen_location_, 2, 1, size);
  return shader_program_;
}

void BlenderFallbackDisplayShader::unbind()
{
  GPU_shader_unbind();
}

GPUShader *BlenderFallbackDisplayShader::get_shader_program()
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
    LOG(ERROR) << "Failed to compile fallback shader";
    return;
  }

  image_texture_location_ = GPU_shader_get_uniform(shader_program_, "image_texture");
  if (image_texture_location_ < 0) {
    LOG(ERROR) << "Shader doesn't contain the 'image_texture' uniform.";
    destroy_shader();
    return;
  }

  fullscreen_location_ = GPU_shader_get_uniform(shader_program_, "fullscreen");
  if (fullscreen_location_ < 0) {
    LOG(ERROR) << "Shader doesn't contain the 'fullscreen' uniform.";
    destroy_shader();
    return;
  }
}

void BlenderFallbackDisplayShader::destroy_shader()
{
  if (shader_program_) {
    GPU_shader_free(shader_program_);
    shader_program_ = nullptr;
  }
}

/* --------------------------------------------------------------------
 * BlenderDisplaySpaceShader.
 */

BlenderDisplaySpaceShader::BlenderDisplaySpaceShader(BL::RenderEngine &b_engine,
                                                     BL::Scene &b_scene)
    : b_engine_(b_engine), b_scene_(b_scene)
{
  DCHECK(b_engine_.support_display_space_shader(b_scene_));
}

GPUShader *BlenderDisplaySpaceShader::bind(int /*width*/, int /*height*/)
{
  b_engine_.bind_display_space_shader(b_scene_);
  return GPU_shader_get_bound();
}

void BlenderDisplaySpaceShader::unbind()
{
  b_engine_.unbind_display_space_shader();
}

GPUShader *BlenderDisplaySpaceShader::get_shader_program()
{
  if (!shader_program_) {
    shader_program_ = GPU_shader_get_bound();
  }
  if (!shader_program_) {
    LOG(ERROR) << "Error retrieving shader program for display space shader.";
  }

  return shader_program_;
}

/* --------------------------------------------------------------------
 * DrawTile.
 */

/* Higher level representation of a texture from the graphics library. */
class DisplayGPUTexture {
 public:
  /* Global counter for all allocated GPUTextures used by instances of this class. */
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
    gpu_texture = GPU_texture_create_2d("CyclesBlitTexture",
                                        max(width, 1),
                                        max(height, 1),
                                        1,
                                        GPU_RGBA16F,
                                        GPU_TEXTURE_USAGE_GENERAL,
                                        nullptr);

    if (!gpu_texture) {
      LOG(ERROR) << "Error creating texture.";
      return false;
    }

    GPU_texture_filter_mode(gpu_texture, false);
    GPU_texture_extend_mode(gpu_texture, GPU_SAMPLER_EXTEND_MODE_EXTEND);

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

  /* Texture resource allocated by the GPU module.
   *
   * NOTE: Allocated on the render engine's context. */
  GPUTexture *gpu_texture = nullptr;

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
  /* Global counter for all allocated GPU module PBOs used by instances of this class. */
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

  bool gpu_resources_ensure(const uint new_width, const uint new_height)
  {
    const size_t required_size = sizeof(half4) * new_width * new_height * 4;

    /* Try to re-use the existing PBO if it has usable size. */
    if (gpu_pixel_buffer) {
      if (new_width != width || new_height != height ||
          GPU_pixel_buffer_size(gpu_pixel_buffer) < required_size)
      {
        gpu_resources_destroy();
      }
    }

    /* Update size. */
    width = new_width;
    height = new_height;

    /* Create pixel buffer if not already created. */
    if (!gpu_pixel_buffer) {
      gpu_pixel_buffer = GPU_pixel_buffer_create(required_size);
    }

    if (gpu_pixel_buffer == nullptr) {
      LOG(ERROR) << "Error creating texture pixel buffer object.";
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

    GPU_pixel_buffer_free(gpu_pixel_buffer);
    gpu_pixel_buffer = nullptr;

    reset();

    --num_used;
  }

  /* Pixel Buffer Object allocated by the GPU module.
   *
   * NOTE: Allocated on the render engine's context. */
  GPUPixelBuffer *gpu_pixel_buffer = nullptr;

  /* Dimensions of the PBO. */
  int width = 0;
  int height = 0;

 protected:
  void reset()
  {
    gpu_pixel_buffer = 0;
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

  inline bool ready_to_draw() const
  {
    return texture.gpu_texture != 0;
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

BlenderDisplayDriver::BlenderDisplayDriver(BL::RenderEngine &b_engine,
                                           BL::Scene &b_scene,
                                           const bool background)
    : b_engine_(b_engine),
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
    LOG(ERROR)
        << "Unexpectedly moving to the next tile without any data provided for current tile.";
    return;
  }

  /* Moving to the next tile without giving render data for the current tile is not an expected
   * situation. */
  DCHECK(!need_clear_);
  /* Texture should have been updated from the PBO at this point. */
  DCHECK(!tiles_->current_tile.need_update_texture_pixels);

  tiles_->finished_tiles.tiles.emplace_back(std::move(tiles_->current_tile.tile));
}

bool BlenderDisplayDriver::update_begin(const Params &params,
                                        int texture_width,
                                        int texture_height)
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

  GPU_fence_wait(gpu_render_sync_);

  DrawTile &current_tile = tiles_->current_tile.tile;
  DisplayGPUPixelBuffer &current_tile_buffer_object = tiles_->current_tile.buffer_object;

  /* Clear storage of all finished tiles when display clear is requested.
   * Do it when new tile data is provided to handle the display clear flag in a single place.
   * It also makes the logic reliable from the whether drawing did happen or not point of view. */
  if (need_clear_) {
    tiles_->finished_tiles.gl_resources_destroy_and_clear();
    need_clear_ = false;
  }

  /* Update PBO dimensions if needed.
   *
   * NOTE: Allocate the PBO for the size which will fit the final render resolution (as in,
   * at a resolution divider 1. This was we don't need to recreate graphics interoperability
   * objects which are costly and which are tied to the specific underlying buffer size.
   * The downside of this approach is that when graphics interoperability is not used we are
   * sending too much data to GPU when resolution divider is not 1. */
  /* TODO(sergey): Investigate whether keeping the PBO exact size of the texture makes non-interop
   * mode faster. */
  const int buffer_width = params.size.x;
  const int buffer_height = params.size.y;

  if (!current_tile_buffer_object.gpu_resources_ensure(buffer_width, buffer_height) ||
      !current_tile.texture.gpu_resources_ensure(texture_width, texture_height))
  {
    tiles_->current_tile.gpu_resources_destroy();
    gpu_context_disable();
    return false;
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
    LOG(ERROR) << "Display driver tile pixel buffer unavailable.";
    return;
  }
  GPU_texture_update_sub_from_pixel_buffer(texture.gpu_texture,
                                           GPU_DATA_HALF_FLOAT,
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
   * move the tile from being current to finished immediately after this call.
   *
   * One concern with this approach is that if the update happens more often than drawing then
   * doing the unpack here occupies GPU transfer for no good reason. However, the render scheduler
   * takes care of ensuring updates don't happen that often. In regular applications redraw will
   * happen much more often than this update.
   *
   * On some older GPUs on macOS, there is a driver crash when updating the texture for viewport
   * renders while Blender is drawing. As a workaround update texture during draw, under assumption
   * that there is no graphics interop on macOS and viewport render has a single tile. */
  if (!background_ &&
      GPU_type_matches_ex(GPU_DEVICE_NVIDIA, GPU_OS_MAC, GPU_DRIVER_ANY, GPU_BACKEND_ANY))
  {
    tiles_->current_tile.need_update_texture_pixels = true;
  }
  else {
    update_tile_texture_pixels(tiles_->current_tile);
  }

  /* Ensure GPU fence exists to synchronize upload. */
  GPU_fence_signal(gpu_upload_sync_);

  GPU_flush();

  gpu_context_disable();
}

/* --------------------------------------------------------------------
 * Texture buffer mapping.
 */

half4 *BlenderDisplayDriver::map_texture_buffer()
{
  GPUPixelBuffer *pix_buf = tiles_->current_tile.buffer_object.gpu_pixel_buffer;
  if (!DCHECK_NOTNULL(pix_buf)) {
    LOG(ERROR) << "Display driver tile pixel buffer unavailable.";
    return nullptr;
  }
  half4 *mapped_rgba_pixels = reinterpret_cast<half4 *>(GPU_pixel_buffer_map(pix_buf));
  if (!mapped_rgba_pixels) {
    LOG(ERROR) << "Error mapping BlenderDisplayDriver pixel buffer object.";
  }
  return mapped_rgba_pixels;
}

void BlenderDisplayDriver::unmap_texture_buffer()
{
  GPUPixelBuffer *pix_buf = tiles_->current_tile.buffer_object.gpu_pixel_buffer;
  if (!DCHECK_NOTNULL(pix_buf)) {
    LOG(ERROR) << "Display driver tile pixel buffer unavailable.";
    return;
  }
  GPU_pixel_buffer_unmap(pix_buf);
}

/* --------------------------------------------------------------------
 * Graphics interoperability.
 */

BlenderDisplayDriver::GraphicsInterop BlenderDisplayDriver::graphics_interop_get()
{
  GraphicsInterop interop_dst;

  interop_dst.buffer_width = tiles_->current_tile.buffer_object.width;
  interop_dst.buffer_height = tiles_->current_tile.buffer_object.height;
  interop_dst.opengl_pbo_id = GPU_pixel_buffer_get_native_handle(
      tiles_->current_tile.buffer_object.gpu_pixel_buffer);

  return interop_dst;
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

void BlenderDisplayDriver::clear()
{
  need_clear_ = true;
}

void BlenderDisplayDriver::set_zoom(float zoom_x, float zoom_y)
{
  zoom_ = make_float2(zoom_x, zoom_y);
}

/* Update vertex buffer with new coordinates of vertex positions and texture coordinates.
 * This buffer is used to render texture in the viewport.
 *
 * NOTE: The buffer needs to be bound. */
static void vertex_draw(const DisplayDriver::Params &params,
                        int texcoord_attribute,
                        int position_attribute)
{
  const int x = params.full_offset.x;
  const int y = params.full_offset.y;

  const int width = params.size.x;
  const int height = params.size.y;

  immBegin(GPU_PRIM_TRI_STRIP, 4);

  immAttr2f(texcoord_attribute, 1.0f, 0.0f);
  immVertex2f(position_attribute, x + width, y);

  immAttr2f(texcoord_attribute, 1.0f, 1.0f);
  immVertex2f(position_attribute, x + width, y + height);

  immAttr2f(texcoord_attribute, 0.0f, 0.0f);
  immVertex2f(position_attribute, x, y);

  immAttr2f(texcoord_attribute, 0.0f, 1.0f);
  immVertex2f(position_attribute, x, y + height);

  immEnd();
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
    LOG(ERROR) << "Display driver tile GPU texture resource unavailable.";
    return;
  }

  /* Trick to keep sharp rendering without jagged edges on all GPUs.
   *
   * The idea here is to enforce driver to use linear interpolation when the image is not zoomed
   * in.
   * For the render result with a resolution divider in effect we always use nearest interpolation.
   *
   * Use explicit MIN assignment to make sure the driver does not have an undefined behavior at
   * the zoom level 1. The MAG filter is always NEAREST. */
  const float zoomed_width = draw_tile.params.size.x * zoom.x;
  const float zoomed_height = draw_tile.params.size.y * zoom.y;
  if (texture.width != draw_tile.params.size.x || texture.height != draw_tile.params.size.y) {
    /* Resolution divider is different from 1, force nearest interpolation. */
    GPU_texture_bind_ex(texture.gpu_texture, GPUSamplerState::default_sampler(), 0);
  }
  else if (zoomed_width - draw_tile.params.size.x > 0.5f ||
           zoomed_height - draw_tile.params.size.y > 0.5f)
  {
    GPU_texture_bind_ex(texture.gpu_texture, GPUSamplerState::default_sampler(), 0);
  }
  else {
    GPU_texture_bind_ex(texture.gpu_texture, {GPU_SAMPLER_FILTERING_LINEAR}, 0);
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

  GPU_fence_wait(gpu_upload_sync_);
  GPU_fence_wait(gpu_render_sync_);

  gpu_context_disable();
}

void BlenderDisplayDriver::draw(const Params &params)
{
  gpu_context_lock();

  if (need_clear_) {
    /* Texture is requested to be cleared and was not yet cleared.
     *
     * Do early return which should be equivalent of drawing all-zero texture.
     * Watch out for the lock though so that the clear happening during update is properly
     * synchronized here. */
    gpu_context_unlock();
    return;
  }

  GPU_fence_wait(gpu_upload_sync_);
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);

  GPUShader *active_shader = display_shader_->bind(params.full_size.x, params.full_size.y);

  GPUVertFormat *format = immVertexFormat();
  const int texcoord_attribute = GPU_vertformat_attr_add(
      format, display_shader_->tex_coord_attribute_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  const int position_attribute = GPU_vertformat_attr_add(
      format, display_shader_->position_attribute_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* Note: Shader is bound again through IMM to register this shader with the IMM module
   * and perform required setup for IMM rendering. This is required as the IMM module
   * needs to be aware of which shader is bound, and the main display shader
   * is bound externally. */
  immBindShader(active_shader);

  if (tiles_->current_tile.need_update_texture_pixels) {
    update_tile_texture_pixels(tiles_->current_tile);
    tiles_->current_tile.need_update_texture_pixels = false;
  }

  draw_tile(zoom_, texcoord_attribute, position_attribute, tiles_->current_tile.tile);

  for (const DrawTile &tile : tiles_->finished_tiles.tiles) {
    draw_tile(zoom_, texcoord_attribute, position_attribute, tile);
  }

  /* Reset IMM shader bind state. */
  immUnbindProgram();

  display_shader_->unbind();

  GPU_blend(GPU_BLEND_NONE);

  GPU_fence_signal(gpu_render_sync_);
  GPU_flush();

  gpu_context_unlock();

  VLOG_DEVICE_STATS << "Display driver number of textures: " << DisplayGPUTexture::num_used;
  VLOG_DEVICE_STATS << "Display driver number of PBOs: " << DisplayGPUPixelBuffer::num_used;
}

void BlenderDisplayDriver::gpu_context_create()
{
  if (!RE_engine_gpu_context_create(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data))) {
    LOG(ERROR) << "Error creating GPU context.";
    return;
  }

  /* Create global GPU resources for display driver. */
  if (!gpu_resources_create()) {
    LOG(ERROR) << "Error creating GPU resources for Cycles Display Driver.";
    return;
  }
}

bool BlenderDisplayDriver::gpu_context_enable()
{
  return RE_engine_gpu_context_enable(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
}

void BlenderDisplayDriver::gpu_context_disable()
{
  RE_engine_gpu_context_disable(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
}

void BlenderDisplayDriver::gpu_context_destroy()
{
  RE_engine_gpu_context_destroy(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
}

void BlenderDisplayDriver::gpu_context_lock()
{
  RE_engine_gpu_context_lock(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
}

void BlenderDisplayDriver::gpu_context_unlock()
{
  RE_engine_gpu_context_unlock(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
}

bool BlenderDisplayDriver::gpu_resources_create()
{
  /* Ensure context is active for resource creation. */
  if (!gpu_context_enable()) {
    LOG(ERROR) << "Error enabling GPU context.";
    return false;
  }

  gpu_upload_sync_ = GPU_fence_create();
  gpu_render_sync_ = GPU_fence_create();

  if (!DCHECK_NOTNULL(gpu_upload_sync_) || !DCHECK_NOTNULL(gpu_render_sync_)) {
    LOG(ERROR) << "Error creating GPU synchronization primitives.";
    assert(0);
    return false;
  }

  gpu_context_disable();
  return true;
}

void BlenderDisplayDriver::gpu_resources_destroy()
{
  gpu_context_enable();

  tiles_->current_tile.gpu_resources_destroy();
  tiles_->finished_tiles.gl_resources_destroy_and_clear();

  /* Fences. */
  if (gpu_render_sync_) {
    GPU_fence_free(gpu_render_sync_);
    gpu_render_sync_ = nullptr;
  }
  if (gpu_upload_sync_) {
    GPU_fence_free(gpu_upload_sync_);
    gpu_upload_sync_ = nullptr;
  }

  gpu_context_disable();

  gpu_context_destroy();
}

CCL_NAMESPACE_END
