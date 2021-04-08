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
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#include "MEM_guardedalloc.h"

#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "gpu_shader_interface.hh"

namespace blender::gpu {

/* TODO(fclem): add unique ID for debugging. */
ShaderInterface::ShaderInterface() = default;

ShaderInterface::~ShaderInterface()
{
  /* Free memory used by name_buffer. */
  MEM_freeN(name_buffer_);
  MEM_freeN(inputs_);
}

static void sort_input_list(MutableSpan<ShaderInput> dst)
{
  if (dst.size() == 0) {
    return;
  }

  Vector<ShaderInput> inputs_vec = Vector<ShaderInput>(dst.size());
  MutableSpan<ShaderInput> src = inputs_vec.as_mutable_span();
  src.copy_from(dst);

  /* Simple sorting by going through the array and selecting the biggest element each time. */
  for (uint i = 0; i < dst.size(); i++) {
    ShaderInput *input_src = &src[0];
    for (uint j = 1; j < src.size(); j++) {
      if (src[j].name_hash > input_src->name_hash) {
        input_src = &src[j];
      }
    }
    dst[i] = *input_src;
    input_src->name_hash = 0;
  }
}

/* Sorts all inputs inside their respective array.
 * This is to allow fast hash collision detection.
 * See ShaderInterface::input_lookup for more details. */
void ShaderInterface::sort_inputs()
{
  sort_input_list(MutableSpan<ShaderInput>(inputs_, attr_len_));
  sort_input_list(MutableSpan<ShaderInput>(inputs_ + attr_len_, ubo_len_));
  sort_input_list(MutableSpan<ShaderInput>(inputs_ + attr_len_ + ubo_len_, uniform_len_));
}

void ShaderInterface::debug_print()
{
  Span<ShaderInput> attrs = Span<ShaderInput>(inputs_, attr_len_);
  Span<ShaderInput> ubos = Span<ShaderInput>(inputs_ + attr_len_, ubo_len_);
  Span<ShaderInput> uniforms = Span<ShaderInput>(inputs_ + attr_len_ + ubo_len_, uniform_len_);
  char *name_buf = name_buffer_;
  const char format[] = "      | %.8x : %4d : %s\n";

  if (attrs.size() > 0) {
    printf("\n    Attributes :\n");
  }
  for (const ShaderInput &attr : attrs) {
    printf(format, attr.name_hash, attr.location, name_buf + attr.name_offset);
  }

  if (uniforms.size() > 0) {
    printf("\n    Uniforms :\n");
  }
  for (const ShaderInput &uni : uniforms) {
    /* Bypass samplers. */
    if (uni.binding == -1) {
      printf(format, uni.name_hash, uni.location, name_buf + uni.name_offset);
    }
  }

  if (ubos.size() > 0) {
    printf("\n    Uniform Buffer Objects :\n");
  }
  for (const ShaderInput &ubo : ubos) {
    printf(format, ubo.name_hash, ubo.binding, name_buf + ubo.name_offset);
  }

  if (enabled_tex_mask_ > 0) {
    printf("\n    Samplers :\n");
  }
  for (const ShaderInput &samp : uniforms) {
    /* Bypass uniforms. */
    if (samp.binding != -1) {
      printf(format, samp.name_hash, samp.binding, name_buf + samp.name_offset);
    }
  }

  printf("\n");
}

}  // namespace blender::gpu
