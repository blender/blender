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

#include "blender/blender_display_driver.h"

#include "device/device.h"
#include "util/util_logging.h"
#include "util/util_opengl.h"

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
 * BlenderDisplayDriver.
 */

BlenderDisplayDriver::BlenderDisplayDriver(BL::RenderEngine &b_engine, BL::Scene &b_scene)
    : b_engine_(b_engine), display_shader_(BlenderDisplayShader::create(b_engine, b_scene))
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

  if (!gl_texture_resources_ensure()) {
    gl_context_disable();
    return false;
  }

  /* Update texture dimensions if needed. */
  if (texture_.width != texture_width || texture_.height != texture_height) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_.gl_id);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA16F, texture_width, texture_height, 0, GL_RGBA, GL_HALF_FLOAT, 0);
    texture_.width = texture_width;
    texture_.height = texture_height;
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Texture did change, and no pixel storage was provided. Tag for an explicit zeroing out to
     * avoid undefined content. */
    texture_.need_clear = true;
  }

  /* Update PBO dimensions if needed.
   *
   * NOTE: Allocate the PBO for the the size which will fit the final render resolution (as in,
   * at a resolution divider 1. This was we don't need to recreate graphics interoperability
   * objects which are costly and which are tied to the specific underlying buffer size.
   * The downside of this approach is that when graphics interoperability is not used we are
   * sending too much data to GPU when resolution divider is not 1. */
  /* TODO(sergey): Investigate whether keeping the PBO exact size of the texture makes non-interop
   * mode faster. */
  const int buffer_width = params.full_size.x;
  const int buffer_height = params.full_size.y;
  if (texture_.buffer_width != buffer_width || texture_.buffer_height != buffer_height) {
    const size_t size_in_bytes = sizeof(half4) * buffer_width * buffer_height;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture_.gl_pbo_id);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size_in_bytes, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    texture_.buffer_width = buffer_width;
    texture_.buffer_height = buffer_height;
  }

  /* New content will be provided to the texture in one way or another, so mark this in a
   * centralized place. */
  texture_.need_update = true;

  texture_.params = params;

  return true;
}

void BlenderDisplayDriver::update_end()
{
  gl_upload_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

  gl_context_disable();
}

/* --------------------------------------------------------------------
 * Texture buffer mapping.
 */

half4 *BlenderDisplayDriver::map_texture_buffer()
{
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture_.gl_pbo_id);

  half4 *mapped_rgba_pixels = reinterpret_cast<half4 *>(
      glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
  if (!mapped_rgba_pixels) {
    LOG(ERROR) << "Error mapping BlenderDisplayDriver pixel buffer object.";
  }

  if (texture_.need_clear) {
    const int64_t texture_width = texture_.width;
    const int64_t texture_height = texture_.height;
    memset(reinterpret_cast<void *>(mapped_rgba_pixels),
           0,
           texture_width * texture_height * sizeof(half4));
    texture_.need_clear = false;
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

  interop_dst.buffer_width = texture_.buffer_width;
  interop_dst.buffer_height = texture_.buffer_height;
  interop_dst.opengl_pbo_id = texture_.gl_pbo_id;

  interop_dst.need_clear = texture_.need_clear;
  texture_.need_clear = false;

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
  texture_.need_clear = true;
}

void BlenderDisplayDriver::set_zoom(float zoom_x, float zoom_y)
{
  zoom_ = make_float2(zoom_x, zoom_y);
}

void BlenderDisplayDriver::draw(const Params &params)
{
  /* See do_update_begin() for why no locking is required here. */
  const bool transparent = true;  // TODO(sergey): Derive this from Film.

  if (!gl_draw_resources_ensure()) {
    return;
  }

  if (use_gl_context_) {
    gl_context_mutex_.lock();
  }

  if (texture_.need_clear) {
    /* Texture is requested to be cleared and was not yet cleared.
     *
     * Do early return which should be equivalent of drawing all-zero texture.
     * Watch out for the lock though so that the clear happening during update is properly
     * synchronized here. */
    gl_context_mutex_.unlock();
    return;
  }

  if (gl_upload_sync_) {
    glWaitSync((GLsync)gl_upload_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  if (transparent) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  display_shader_->bind(params.full_size.x, params.full_size.y);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_.gl_id);

  /* Trick to keep sharp rendering without jagged edges on all GPUs.
   *
   * The idea here is to enforce driver to use linear interpolation when the image is not zoomed
   * in.
   * For the render result with a resolution divider in effect we always use nearest interpolation.
   *
   * Use explicit MIN assignment to make sure the driver does not have an undefined behavior at
   * the zoom level 1. The MAG filter is always NEAREST. */
  const float zoomed_width = params.size.x * zoom_.x;
  const float zoomed_height = params.size.y * zoom_.y;
  if (texture_.width != params.size.x || texture_.height != params.size.y) {
    /* Resolution divider is different from 1, force nearest interpolation. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else if (zoomed_width - params.size.x > 0.5f || zoomed_height - params.size.y > 0.5f) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  texture_update_if_needed();
  vertex_buffer_update(params);

  /* TODO(sergey): Does it make sense/possible to cache/reuse the VAO? */
  GLuint vertex_array_object;
  glGenVertexArrays(1, &vertex_array_object);
  glBindVertexArray(vertex_array_object);

  const int texcoord_attribute = display_shader_->get_tex_coord_attrib_location();
  const int position_attribute = display_shader_->get_position_attrib_location();

  glEnableVertexAttribArray(texcoord_attribute);
  glEnableVertexAttribArray(position_attribute);

  glVertexAttribPointer(
      texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)0);
  glVertexAttribPointer(position_attribute,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        4 * sizeof(float),
                        (const GLvoid *)(sizeof(float) * 2));

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glDeleteVertexArrays(1, &vertex_array_object);

  display_shader_->unbind();

  if (transparent) {
    glDisable(GL_BLEND);
  }

  gl_render_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

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

bool BlenderDisplayDriver::gl_draw_resources_ensure()
{
  if (!texture_.gl_id) {
    /* If there is no texture allocated, there is nothing to draw. Inform the draw call that it can
     * can not continue. Note that this is not an unrecoverable error, so once the texture is known
     * we will come back here and create all the GPU resources needed for draw. */
    return false;
  }

  if (gl_draw_resource_creation_attempted_) {
    return gl_draw_resources_created_;
  }
  gl_draw_resource_creation_attempted_ = true;

  if (!vertex_buffer_) {
    glGenBuffers(1, &vertex_buffer_);
    if (!vertex_buffer_) {
      LOG(ERROR) << "Error creating vertex buffer.";
      return false;
    }
  }

  gl_draw_resources_created_ = true;

  return true;
}

void BlenderDisplayDriver::gl_resources_destroy()
{
  gl_context_enable();

  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
  }

  if (texture_.gl_pbo_id) {
    glDeleteBuffers(1, &texture_.gl_pbo_id);
    texture_.gl_pbo_id = 0;
  }

  if (texture_.gl_id) {
    glDeleteTextures(1, &texture_.gl_id);
    texture_.gl_id = 0;
  }

  gl_context_disable();

  gl_context_dispose();
}

bool BlenderDisplayDriver::gl_texture_resources_ensure()
{
  if (texture_.creation_attempted) {
    return texture_.is_created;
  }
  texture_.creation_attempted = true;

  DCHECK(!texture_.gl_id);
  DCHECK(!texture_.gl_pbo_id);

  /* Create texture. */
  glGenTextures(1, &texture_.gl_id);
  if (!texture_.gl_id) {
    LOG(ERROR) << "Error creating texture.";
    return false;
  }

  /* Configure the texture. */
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_.gl_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  /* Create PBO for the texture. */
  glGenBuffers(1, &texture_.gl_pbo_id);
  if (!texture_.gl_pbo_id) {
    LOG(ERROR) << "Error creating texture pixel buffer object.";
    return false;
  }

  /* Creation finished with a success. */
  texture_.is_created = true;

  return true;
}

void BlenderDisplayDriver::texture_update_if_needed()
{
  if (!texture_.need_update) {
    return;
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture_.gl_pbo_id);
  glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0, texture_.width, texture_.height, GL_RGBA, GL_HALF_FLOAT, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  texture_.need_update = false;
}

void BlenderDisplayDriver::vertex_buffer_update(const Params & /*params*/)
{
  /* Draw at the parameters for which the texture has been updated for. This allows to always draw
   * texture during bordered-rendered camera view without flickering. The validness of the display
   * parameters for a texture is guaranteed by the initial "clear" state which makes drawing to
   * have an early output.
   *
   * Such approach can cause some extra "jelly" effect during panning, but it is not more jelly
   * than overlay of selected objects. Also, it's possible to redraw texture at an intersection of
   * the texture draw parameters and the latest updated draw parameters (although, complexity of
   * doing it might not worth it. */
  const int x = texture_.params.full_offset.x;
  const int y = texture_.params.full_offset.y;

  const int width = texture_.params.size.x;
  const int height = texture_.params.size.y;

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

CCL_NAMESPACE_END
