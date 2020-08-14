/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_span.hh"

#include "GPU_shader.h"
#include "GPU_shader_interface.h"
#include "GPU_vertex_buffer.h"

namespace blender {
namespace gpu {

class Shader : public GPUShader {
 public:
  Shader(const char *name);
  virtual ~Shader();

  virtual void vertex_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual void geometry_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual void fragment_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual bool finalize(void) = 0;

  virtual void transform_feedback_names_set(Span<const char *> name_list,
                                            const eGPUShaderTFBType geom_type) = 0;
  virtual bool transform_feedback_enable(GPUVertBuf *) = 0;
  virtual void transform_feedback_disable(void) = 0;

  virtual void bind(void) = 0;
  virtual void unbind(void) = 0;

  virtual void uniform_float(int location, int comp_len, int array_size, const float *data) = 0;
  virtual void uniform_int(int location, int comp_len, int array_size, const int *data) = 0;

  virtual void vertformat_from_shader(GPUVertFormat *) const = 0;

 protected:
  void print_errors(Span<const char *> sources, char *log);
};

}  // namespace gpu
}  // namespace blender

/* XXX do not use it. Special hack to use OCIO with batch API. */
GPUShader *immGetShader(void);
