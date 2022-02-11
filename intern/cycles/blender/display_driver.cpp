/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "blender/display_driver.h"

#include "device/device.h"
#include "util/log.h"
#include "util/opengl.h"

extern "C" {
struct RenderEngine;

bool RE_engine_has_render_context(struct RenderEngine *engine);
void RE_engine_render_context_enable(struct RenderEngine *engine);
void RE_engine_render_context_disable(struct RenderEngine *engine);

bool DRW_opengl_context_release();
void DRW_opengl_context_activate(bool drw_state);

void *WM_opengl_context_create();
void WM_opengl_context_activate(void *gl_context);
void WM_opengl_context_dispose(void *gl_context);
void WM_opengl_context_release(void *context);
}

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
    const uint shader_program = get_shader_program();
    position_attribute_location_ = glGetAttribLocation(shader_program, position_attribute_name);
  }
  return position_attribute_location_;
}

int BlenderDisplayShader::get_tex_coord_attrib_location()
{
  if (tex_coord_attribute_location_ == -1) {
    const uint shader_program = get_shader_program();
    tex_coord_attribute_location_ = glGetAttribLocation(shader_program, tex_coord_attribute_name);
  }
  return tex_coord_attribute_location_;
}

/* --------------------------------------------------------------------
 * BlenderFallbackDisplayShader.
 */

/* TODO move shaders to standalone .glsl file. */
static const char *FALLBACK_VERTEX_SHADER =
    "#version 330\n"
    "uniform vec2 fullscreen;\n"
    "in vec2 texCoord;\n"
    "in vec2 pos;\n"
    "out vec2 texCoord_interp;\n"
    "\n"
    "vec2 normalize_coordinates()\n"
    "{\n"
    "   return (vec2(2.0) * (pos / fullscreen)) - vec2(1.0);\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(normalize_coordinates(), 0.0, 1.0);\n"
    "   texCoord_interp = texCoord;\n"
    "}\n\0";

static const char *FALLBACK_FRAGMENT_SHADER =
    "#version 330\n"
    "uniform sampler2D image_texture;\n"
    "in vec2 texCoord_interp;\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   fragColor = texture(image_texture, texCoord_interp);\n"
    "}\n\0";

static void shader_print_errors(const char *task, const char *log, const char *code)
{
  LOG(ERROR) << "Shader: " << task << " error:";
  LOG(ERROR) << "===== shader string ====";

  stringstream stream(code);
  string partial;

  int line = 1;
  while (getline(stream, partial, '\n')) {
    if (line < 10) {
      LOG(ERROR) << " " << line << " " << partial;
    }
    else {
      LOG(ERROR) << line << " " << partial;
    }
    line++;
  }
  LOG(ERROR) << log;
}

static int compile_fallback_shader(void)
{
  const struct Shader {
    const char *source;
    const GLenum type;
  } shaders[2] = {{FALLBACK_VERTEX_SHADER, GL_VERTEX_SHADER},
                  {FALLBACK_FRAGMENT_SHADER, GL_FRAGMENT_SHADER}};

  const GLuint program = glCreateProgram();

  for (int i = 0; i < 2; i++) {
    const GLuint shader = glCreateShader(shaders[i].type);

    string source_str = shaders[i].source;
    const char *c_str = source_str.c_str();

    glShaderSource(shader, 1, &c_str, NULL);
    glCompileShader(shader);

    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    if (!compile_status) {
      GLchar log[5000];
      GLsizei length = 0;
      glGetShaderInfoLog(shader, sizeof(log), &length, log);
      shader_print_errors("compile", log, c_str);
      return 0;
    }

    glAttachShader(program, shader);
  }

  /* Link output. */
  glBindFragDataLocation(program, 0, "fragColor");

  /* Link and error check. */
  glLinkProgram(program);

  /* TODO(sergey): Find a way to nicely de-duplicate the error checking. */
  GLint link_status;
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);
  if (!link_status) {
    GLchar log[5000];
    GLsizei length = 0;
    /* TODO(sergey): Is it really program passed to glGetShaderInfoLog? */
    glGetShaderInfoLog(program, sizeof(log), &length, log);
    shader_print_errors("linking", log, FALLBACK_VERTEX_SHADER);
    shader_print_errors("linking", log, FALLBACK_FRAGMENT_SHADER);
    return 0;
  }

  return program;
}

void BlenderFallbackDisplayShader::bind(int width, int height)
{
  create_shader_if_needed();

  if (!shader_program_) {
    return;
  }

  glUseProgram(shader_program_);
  glUniform1i(image_texture_location_, 0);
  glUniform2f(fullscreen_location_, width, height);
}

void BlenderFallbackDisplayShader::unbind()
{
}

uint BlenderFallbackDisplayShader::get_shader_program()
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
    return;
  }

  glUseProgram(shader_program_);

  image_texture_location_ = glGetUniformLocation(shader_program_, "image_texture");
  if (image_texture_location_ < 0) {
    LOG(ERROR) << "Shader doesn't contain the 'image_texture' uniform.";
    destroy_shader();
    return;
  }

  fullscreen_location_ = glGetUniformLocation(shader_program_, "fullscreen");
  if (fullscreen_location_ < 0) {
    LOG(ERROR) << "Shader doesn't contain the 'fullscreen' uniform.";
    destroy_shader();
    return;
  }
}

void BlenderFallbackDisplayShader::destroy_shader()
{
  glDeleteProgram(shader_program_);
  shader_program_ = 0;
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

void BlenderDisplaySpaceShader::bind(int /*width*/, int /*height*/)
{
  b_engine_.bind_display_space_shader(b_scene_);
}

void BlenderDisplaySpaceShader::unbind()
{
  b_engine_.unbind_display_space_shader();
}

uint BlenderDisplaySpaceShader::get_shader_program()
{
  if (!shader_program_) {
    glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int *>(&shader_program_));
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
class GLTexture {
 public:
  /* Global counter for all allocated OpenGL textures used by instances of this class. */
  static inline std::atomic<int> num_used = 0;

  GLTexture() = default;

  ~GLTexture()
  {
    assert(gl_id == 0);
  }

  GLTexture(const GLTexture &other) = delete;
  GLTexture &operator=(GLTexture &other) = delete;

  GLTexture(GLTexture &&other) noexcept
      : gl_id(other.gl_id), width(other.width), height(other.height)
  {
    other.reset();
  }

  GLTexture &operator=(GLTexture &&other)
  {
    if (this == &other) {
      return *this;
    }

    gl_id = other.gl_id;
    width = other.width;
    height = other.height;

    other.reset();

    return *this;
  }

  bool gl_resources_ensure()
  {
    if (gl_id) {
      return true;
    }

    /* Create texture. */
    glGenTextures(1, &gl_id);
    if (!gl_id) {
      LOG(ERROR) << "Error creating texture.";
      return false;
    }

    /* Configure the texture. */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Clamp to edge so that precision issues when zoomed out (which forces linear interpolation)
     * does not cause unwanted repetition. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    ++num_used;

    return true;
  }

  void gl_resources_destroy()
  {
    if (!gl_id) {
      return;
    }

    glDeleteTextures(1, &gl_id);

    reset();

    --num_used;
  }

  /* OpenGL resource IDs of the texture.
   *
   * NOTE: Allocated on the render engine's context. */
  uint gl_id = 0;

  /* Dimensions of the texture in pixels. */
  int width = 0;
  int height = 0;

 protected:
  void reset()
  {
    gl_id = 0;
    width = 0;
    height = 0;
  }
};

/* Higher level representation of a Pixel Buffer Object (PBO) from the graphics library. */
class GLPixelBufferObject {
 public:
  /* Global counter for all allocated OpenGL PBOs used by instances of this class. */
  static inline std::atomic<int> num_used = 0;

  GLPixelBufferObject() = default;

  ~GLPixelBufferObject()
  {
    assert(gl_id == 0);
  }

  GLPixelBufferObject(const GLPixelBufferObject &other) = delete;
  GLPixelBufferObject &operator=(GLPixelBufferObject &other) = delete;

  GLPixelBufferObject(GLPixelBufferObject &&other) noexcept
      : gl_id(other.gl_id), width(other.width), height(other.height)
  {
    other.reset();
  }

  GLPixelBufferObject &operator=(GLPixelBufferObject &&other)
  {
    if (this == &other) {
      return *this;
    }

    gl_id = other.gl_id;
    width = other.width;
    height = other.height;

    other.reset();

    return *this;
  }

  bool gl_resources_ensure()
  {
    if (gl_id) {
      return true;
    }

    glGenBuffers(1, &gl_id);
    if (!gl_id) {
      LOG(ERROR) << "Error creating texture pixel buffer object.";
      return false;
    }

    ++num_used;

    return true;
  }

  void gl_resources_destroy()
  {
    if (!gl_id) {
      return;
    }

    glDeleteBuffers(1, &gl_id);

    reset();

    --num_used;
  }

  /* OpenGL resource IDs of the PBO.
   *
   * NOTE: Allocated on the render engine's context. */
  uint gl_id = 0;

  /* Dimensions of the PBO. */
  int width = 0;
  int height = 0;

 protected:
  void reset()
  {
    gl_id = 0;
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

  bool gl_resources_ensure()
  {
    if (!texture.gl_resources_ensure()) {
      gl_resources_destroy();
      return false;
    }

    return true;
  }

  void gl_resources_destroy()
  {
    texture.gl_resources_destroy();
  }

  inline bool ready_to_draw() const
  {
    return texture.gl_id != 0;
  }

  /* Texture which contains pixels of the tile. */
  GLTexture texture;

  /* Display parameters the texture of this tile has been updated for. */
  BlenderDisplayDriver::Params params;
};

class DrawTileAndPBO {
 public:
  bool gl_resources_ensure()
  {
    if (!tile.gl_resources_ensure() || !buffer_object.gl_resources_ensure()) {
      gl_resources_destroy();
      return false;
    }

    return true;
  }

  void gl_resources_destroy()
  {
    tile.gl_resources_destroy();
    buffer_object.gl_resources_destroy();
  }

  DrawTile tile;
  GLPixelBufferObject buffer_object;
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
        tile.gl_resources_destroy();
      }

      tiles.clear();
    }
  } finished_tiles;

  /* OpenGL vertex buffer needed for drawing. */
  uint gl_vertex_buffer = 0;

  bool gl_resources_ensure()
  {
    if (!gl_vertex_buffer) {
      glGenBuffers(1, &gl_vertex_buffer);
      if (!gl_vertex_buffer) {
        LOG(ERROR) << "Error allocating tile VBO.";
        return false;
      }
    }

    return true;
  }

  void gl_resources_destroy()
  {
    if (gl_vertex_buffer) {
      glDeleteBuffers(1, &gl_vertex_buffer);
      gl_vertex_buffer = 0;
    }
  }
};

BlenderDisplayDriver::BlenderDisplayDriver(BL::RenderEngine &b_engine, BL::Scene &b_scene)
    : b_engine_(b_engine),
      display_shader_(BlenderDisplayShader::create(b_engine, b_scene)),
      tiles_(make_unique<Tiles>())
{
  /* Create context while on the main thread. */
  gl_context_create();
}

BlenderDisplayDriver::~BlenderDisplayDriver()
{
  gl_resources_destroy();
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

  tiles_->finished_tiles.tiles.emplace_back(std::move(tiles_->current_tile.tile));
}

bool BlenderDisplayDriver::update_begin(const Params &params,
                                        int texture_width,
                                        int texture_height)
{
  /* Note that it's the responsibility of BlenderDisplayDriver to ensure updating and drawing
   * the texture does not happen at the same time. This is achieved indirectly.
   *
   * When enabling the OpenGL context, it uses an internal mutex lock DST.gl_context_lock.
   * This same lock is also held when do_draw() is called, which together ensure mutual
   * exclusion.
   *
   * This locking is not performed on the Cycles side, because that would cause lock inversion. */
  if (!gl_context_enable()) {
    return false;
  }

  if (gl_render_sync_) {
    glWaitSync((GLsync)gl_render_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  DrawTile &current_tile = tiles_->current_tile.tile;
  GLPixelBufferObject &current_tile_buffer_object = tiles_->current_tile.buffer_object;

  /* Clear storage of all finished tiles when display clear is requested.
   * Do it when new tile data is provided to handle the display clear flag in a single place.
   * It also makes the logic reliable from the whether drawing did happen or not point of view. */
  if (need_clear_) {
    tiles_->finished_tiles.gl_resources_destroy_and_clear();
    need_clear_ = false;
  }

  if (!tiles_->gl_resources_ensure()) {
    tiles_->gl_resources_destroy();
    gl_context_disable();
    return false;
  }

  if (!tiles_->current_tile.gl_resources_ensure()) {
    tiles_->current_tile.gl_resources_destroy();
    gl_context_disable();
    return false;
  }

  /* Update texture dimensions if needed. */
  if (current_tile.texture.width != texture_width ||
      current_tile.texture.height != texture_height) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_tile.texture.gl_id);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA16F, texture_width, texture_height, 0, GL_RGBA, GL_HALF_FLOAT, 0);
    current_tile.texture.width = texture_width;
    current_tile.texture.height = texture_height;
    glBindTexture(GL_TEXTURE_2D, 0);
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
  if (current_tile_buffer_object.width != buffer_width ||
      current_tile_buffer_object.height != buffer_height) {
    const size_t size_in_bytes = sizeof(half4) * buffer_width * buffer_height;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, current_tile_buffer_object.gl_id);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size_in_bytes, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    current_tile_buffer_object.width = buffer_width;
    current_tile_buffer_object.height = buffer_height;
  }

  /* Store an updated parameters of the current tile.
   * In theory it is only needed once per update of the tile, but doing it on every update is
   * the easiest and is not expensive. */
  tiles_->current_tile.tile.params = params;

  return true;
}

static void update_tile_texture_pixels(const DrawTileAndPBO &tile)
{
  const GLTexture &texture = tile.tile.texture;

  DCHECK_NE(tile.buffer_object.gl_id, 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture.gl_id);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, tile.buffer_object.gl_id);

  glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, GL_RGBA, GL_HALF_FLOAT, 0);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
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
   * happen much more often than this update. */
  update_tile_texture_pixels(tiles_->current_tile);

  gl_upload_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

  gl_context_disable();
}

/* --------------------------------------------------------------------
 * Texture buffer mapping.
 */

half4 *BlenderDisplayDriver::map_texture_buffer()
{
  const uint pbo_gl_id = tiles_->current_tile.buffer_object.gl_id;

  DCHECK_NE(pbo_gl_id, 0);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_gl_id);

  half4 *mapped_rgba_pixels = reinterpret_cast<half4 *>(
      glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
  if (!mapped_rgba_pixels) {
    LOG(ERROR) << "Error mapping BlenderDisplayDriver pixel buffer object.";
  }

  return mapped_rgba_pixels;
}

void BlenderDisplayDriver::unmap_texture_buffer()
{
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

/* --------------------------------------------------------------------
 * Graphics interoperability.
 */

BlenderDisplayDriver::GraphicsInterop BlenderDisplayDriver::graphics_interop_get()
{
  GraphicsInterop interop_dst;

  interop_dst.buffer_width = tiles_->current_tile.buffer_object.width;
  interop_dst.buffer_height = tiles_->current_tile.buffer_object.height;
  interop_dst.opengl_pbo_id = tiles_->current_tile.buffer_object.gl_id;

  return interop_dst;
}

void BlenderDisplayDriver::graphics_interop_activate()
{
  gl_context_enable();
}

void BlenderDisplayDriver::graphics_interop_deactivate()
{
  gl_context_disable();
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
static void vertex_buffer_update(const DisplayDriver::Params &params)
{
  const int x = params.full_offset.x;
  const int y = params.full_offset.y;

  const int width = params.size.x;
  const int height = params.size.y;

  /* Invalidate old contents - avoids stalling if the buffer is still waiting in queue to be
   * rendered. */
  glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

  float *vpointer = reinterpret_cast<float *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
  if (!vpointer) {
    return;
  }

  vpointer[0] = 0.0f;
  vpointer[1] = 0.0f;
  vpointer[2] = x;
  vpointer[3] = y;

  vpointer[4] = 1.0f;
  vpointer[5] = 0.0f;
  vpointer[6] = x + width;
  vpointer[7] = y;

  vpointer[8] = 1.0f;
  vpointer[9] = 1.0f;
  vpointer[10] = x + width;
  vpointer[11] = y + height;

  vpointer[12] = 0.0f;
  vpointer[13] = 1.0f;
  vpointer[14] = x;
  vpointer[15] = y + height;

  glUnmapBuffer(GL_ARRAY_BUFFER);
}

static void draw_tile(const float2 &zoom,
                      const int texcoord_attribute,
                      const int position_attribute,
                      const DrawTile &draw_tile,
                      const uint gl_vertex_buffer)
{
  if (!draw_tile.ready_to_draw()) {
    return;
  }

  const GLTexture &texture = draw_tile.texture;

  DCHECK_NE(texture.gl_id, 0);
  DCHECK_NE(gl_vertex_buffer, 0);

  glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buffer);

  /* Draw at the parameters for which the texture has been updated for. This allows to always draw
   * texture during bordered-rendered camera view without flickering. The validness of the display
   * parameters for a texture is guaranteed by the initial "clear" state which makes drawing to
   * have an early output.
   *
   * Such approach can cause some extra "jelly" effect during panning, but it is not more jelly
   * than overlay of selected objects. Also, it's possible to redraw texture at an intersection of
   * the texture draw parameters and the latest updated draw parameters (although, complexity of
   * doing it might not worth it. */
  vertex_buffer_update(draw_tile.params);

  glBindTexture(GL_TEXTURE_2D, texture.gl_id);

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else if (zoomed_width - draw_tile.params.size.x > 0.5f ||
           zoomed_height - draw_tile.params.size.y > 0.5f) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

  glVertexAttribPointer(
      texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)0);
  glVertexAttribPointer(position_attribute,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        4 * sizeof(float),
                        (const GLvoid *)(sizeof(float) * 2));

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
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
  if (!gl_context_enable()) {
    return;
  }

  if (gl_upload_sync_) {
    glWaitSync((GLsync)gl_upload_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  if (gl_render_sync_) {
    glWaitSync((GLsync)gl_render_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  gl_context_disable();
}

void BlenderDisplayDriver::draw(const Params &params)
{
  /* See do_update_begin() for why no locking is required here. */
  const bool transparent = true;  // TODO(sergey): Derive this from Film.

  if (use_gl_context_) {
    gl_context_mutex_.lock();
  }

  if (need_clear_) {
    /* Texture is requested to be cleared and was not yet cleared.
     *
     * Do early return which should be equivalent of drawing all-zero texture.
     * Watch out for the lock though so that the clear happening during update is properly
     * synchronized here. */
    if (use_gl_context_) {
      gl_context_mutex_.unlock();
    }
    return;
  }

  if (gl_upload_sync_) {
    glWaitSync((GLsync)gl_upload_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  if (transparent) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  glActiveTexture(GL_TEXTURE0);

  /* NOTE: The VAO is to be allocated on the drawing context as it is not shared across contexts.
   * Simplest is to allocate it on every redraw so that it is possible to destroy it from a
   * correct context. */
  GLuint vertex_array_object;
  glGenVertexArrays(1, &vertex_array_object);
  glBindVertexArray(vertex_array_object);

  display_shader_->bind(params.full_size.x, params.full_size.y);

  const int texcoord_attribute = display_shader_->get_tex_coord_attrib_location();
  const int position_attribute = display_shader_->get_position_attrib_location();

  glEnableVertexAttribArray(texcoord_attribute);
  glEnableVertexAttribArray(position_attribute);

  draw_tile(zoom_,
            texcoord_attribute,
            position_attribute,
            tiles_->current_tile.tile,
            tiles_->gl_vertex_buffer);

  for (const DrawTile &tile : tiles_->finished_tiles.tiles) {
    draw_tile(zoom_, texcoord_attribute, position_attribute, tile, tiles_->gl_vertex_buffer);
  }

  display_shader_->unbind();

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDeleteVertexArrays(1, &vertex_array_object);

  if (transparent) {
    glDisable(GL_BLEND);
  }

  gl_render_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

  if (VLOG_IS_ON(5)) {
    VLOG(5) << "Number of textures: " << GLTexture::num_used;
    VLOG(5) << "Number of PBOs: " << GLPixelBufferObject::num_used;
  }

  if (use_gl_context_) {
    gl_context_mutex_.unlock();
  }
}

void BlenderDisplayDriver::gl_context_create()
{
  /* When rendering in viewport there is no render context available via engine.
   * Check whether own context is to be created here.
   *
   * NOTE: If the `b_engine_`'s context is not available, we are expected to be on a main thread
   * here. */
  use_gl_context_ = !RE_engine_has_render_context(
      reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));

  if (use_gl_context_) {
    const bool drw_state = DRW_opengl_context_release();
    gl_context_ = WM_opengl_context_create();
    if (gl_context_) {
      /* On Windows an old context is restored after creation, and subsequent release of context
       * generates a Win32 error. Harmless for users, but annoying to have possible misleading
       * error prints in the console. */
#ifndef _WIN32
      WM_opengl_context_release(gl_context_);
#endif
    }
    else {
      LOG(ERROR) << "Error creating OpenGL context.";
    }

    DRW_opengl_context_activate(drw_state);
  }
}

bool BlenderDisplayDriver::gl_context_enable()
{
  if (use_gl_context_) {
    if (!gl_context_) {
      return false;
    }
    gl_context_mutex_.lock();
    WM_opengl_context_activate(gl_context_);
    return true;
  }

  RE_engine_render_context_enable(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
  return true;
}

void BlenderDisplayDriver::gl_context_disable()
{
  if (use_gl_context_) {
    if (gl_context_) {
      WM_opengl_context_release(gl_context_);
      gl_context_mutex_.unlock();
    }
    return;
  }

  RE_engine_render_context_disable(reinterpret_cast<RenderEngine *>(b_engine_.ptr.data));
}

void BlenderDisplayDriver::gl_context_dispose()
{
  if (gl_context_) {
    const bool drw_state = DRW_opengl_context_release();

    WM_opengl_context_activate(gl_context_);
    WM_opengl_context_dispose(gl_context_);

    DRW_opengl_context_activate(drw_state);
  }
}

void BlenderDisplayDriver::gl_resources_destroy()
{
  gl_context_enable();

  tiles_->current_tile.gl_resources_destroy();
  tiles_->finished_tiles.gl_resources_destroy_and_clear();
  tiles_->gl_resources_destroy();

  gl_context_disable();

  gl_context_dispose();
}

CCL_NAMESPACE_END
