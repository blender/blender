/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "app/opengl/display_driver.h"
#include "app/opengl/shader.h"

#include "util/log.h"
#include "util/string.h"

#include <SDL.h>
#include <epoxy/gl.h>

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * OpenGLDisplayDriver.
 */

OpenGLDisplayDriver::OpenGLDisplayDriver(const function<bool()> &gl_context_enable,
                                         const function<void()> &gl_context_disable)
    : gl_context_enable_(gl_context_enable), gl_context_disable_(gl_context_disable)
{
}

OpenGLDisplayDriver::~OpenGLDisplayDriver() {}

/* --------------------------------------------------------------------
 * Update procedure.
 */

void OpenGLDisplayDriver::next_tile_begin()
{
  /* Assuming no tiles used in interactive display. */
}

bool OpenGLDisplayDriver::update_begin(const Params &params, int texture_width, int texture_height)
{
  /* Note that it's the responsibility of OpenGLDisplayDriver to ensure updating and drawing
   * the texture does not happen at the same time. This is achieved indirectly.
   *
   * When enabling the OpenGL context, it uses an internal mutex lock DST.gl_context_lock.
   * This same lock is also held when do_draw() is called, which together ensure mutual
   * exclusion.
   *
   * This locking is not performed on the Cycles side, because that would cause lock inversion. */
  if (!gl_context_enable_()) {
    return false;
  }

  if (gl_render_sync_) {
    glWaitSync((GLsync)gl_render_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  if (!gl_texture_resources_ensure()) {
    gl_context_disable_();
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
   * NOTE: Allocate the PBO for the size which will fit the final render resolution (as in,
   * at a resolution divider 1. This was we don't need to recreate graphics interoperability
   * objects which are costly and which are tied to the specific underlying buffer size.
   * The downside of this approach is that when graphics interoperability is not used we are
   * sending too much data to GPU when resolution divider is not 1. */
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

  return true;
}

void OpenGLDisplayDriver::update_end()
{
  gl_upload_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

  gl_context_disable_();
}

/* --------------------------------------------------------------------
 * Texture buffer mapping.
 */

half4 *OpenGLDisplayDriver::map_texture_buffer()
{
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture_.gl_pbo_id);

  half4 *mapped_rgba_pixels = reinterpret_cast<half4 *>(
      glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
  if (!mapped_rgba_pixels) {
    LOG(ERROR) << "Error mapping OpenGLDisplayDriver pixel buffer object.";
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

void OpenGLDisplayDriver::unmap_texture_buffer()
{
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

/* --------------------------------------------------------------------
 * Graphics interoperability.
 */

OpenGLDisplayDriver::GraphicsInterop OpenGLDisplayDriver::graphics_interop_get()
{
  GraphicsInterop interop_dst;

  interop_dst.buffer_width = texture_.buffer_width;
  interop_dst.buffer_height = texture_.buffer_height;
  interop_dst.opengl_pbo_id = texture_.gl_pbo_id;

  interop_dst.need_clear = texture_.need_clear;
  texture_.need_clear = false;

  return interop_dst;
}

void OpenGLDisplayDriver::graphics_interop_activate()
{
  gl_context_enable_();
}

void OpenGLDisplayDriver::graphics_interop_deactivate()
{
  gl_context_disable_();
}

/* --------------------------------------------------------------------
 * Drawing.
 */

void OpenGLDisplayDriver::clear()
{
  texture_.need_clear = true;
}

void OpenGLDisplayDriver::draw(const Params &params)
{
  /* See do_update_begin() for why no locking is required here. */
  if (texture_.need_clear) {
    /* Texture is requested to be cleared and was not yet cleared.
     * Do early return which should be equivalent of drawing all-zero texture. */
    return;
  }

  if (!gl_draw_resources_ensure()) {
    return;
  }

  if (gl_upload_sync_) {
    glWaitSync((GLsync)gl_upload_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  display_shader_.bind(params.full_size.x, params.full_size.y);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_.gl_id);

  if (texture_.width != params.size.x || texture_.height != params.size.y) {
    /* Resolution divider is different from 1, force nearest interpolation. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  texture_update_if_needed();
  vertex_buffer_update(params);

  GLuint vertex_array_object;
  glGenVertexArrays(1, &vertex_array_object);
  glBindVertexArray(vertex_array_object);

  const int texcoord_attribute = display_shader_.get_tex_coord_attrib_location();
  const int position_attribute = display_shader_.get_position_attrib_location();

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

  display_shader_.unbind();

  glDisable(GL_BLEND);

  gl_render_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();
}

bool OpenGLDisplayDriver::gl_draw_resources_ensure()
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

void OpenGLDisplayDriver::gl_resources_destroy()
{
  gl_context_enable_();

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

  gl_context_disable_();
}

bool OpenGLDisplayDriver::gl_texture_resources_ensure()
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

void OpenGLDisplayDriver::texture_update_if_needed()
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

void OpenGLDisplayDriver::vertex_buffer_update(const Params &params)
{
  /* Invalidate old contents - avoids stalling if the buffer is still waiting in queue to be
   * rendered. */
  glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

  float *vpointer = reinterpret_cast<float *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
  if (!vpointer) {
    return;
  }

  vpointer[0] = 0.0f;
  vpointer[1] = 0.0f;
  vpointer[2] = params.full_offset.x;
  vpointer[3] = params.full_offset.y;

  vpointer[4] = 1.0f;
  vpointer[5] = 0.0f;
  vpointer[6] = (float)params.size.x + params.full_offset.x;
  vpointer[7] = params.full_offset.y;

  vpointer[8] = 1.0f;
  vpointer[9] = 1.0f;
  vpointer[10] = (float)params.size.x + params.full_offset.x;
  vpointer[11] = (float)params.size.y + params.full_offset.y;

  vpointer[12] = 0.0f;
  vpointer[13] = 1.0f;
  vpointer[14] = params.full_offset.x;
  vpointer[15] = (float)params.size.y + params.full_offset.y;

  glUnmapBuffer(GL_ARRAY_BUFFER);
}

CCL_NAMESPACE_END
