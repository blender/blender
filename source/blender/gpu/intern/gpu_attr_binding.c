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
 * GPU vertex attribute binding
 */

#include "GPU_attr_binding.h"
#include "gpu_attr_binding_private.h"
#include <stddef.h>
#include <stdlib.h>

#if GPU_VERT_ATTR_MAX_LEN != 16
#  error "attribute binding code assumes GPU_VERT_ATTR_MAX_LEN = 16"
#endif

void AttrBinding_clear(GPUAttrBinding *binding)
{
  binding->loc_bits = 0;
  binding->enabled_bits = 0;
}

uint read_attr_location(const GPUAttrBinding *binding, uint a_idx)
{
#if TRUST_NO_ONE
  assert(a_idx < GPU_VERT_ATTR_MAX_LEN);
  assert(binding->enabled_bits & (1 << a_idx));
#endif
  return (binding->loc_bits >> (4 * a_idx)) & 0xF;
}

static void write_attr_location(GPUAttrBinding *binding, uint a_idx, uint location)
{
#if TRUST_NO_ONE
  assert(a_idx < GPU_VERT_ATTR_MAX_LEN);
  assert(location < GPU_VERT_ATTR_MAX_LEN);
#endif
  const uint shift = 4 * a_idx;
  const uint64_t mask = ((uint64_t)0xF) << shift;
  /* overwrite this attr's previous location */
  binding->loc_bits = (binding->loc_bits & ~mask) | (location << shift);
  /* mark this attr as enabled */
  binding->enabled_bits |= 1 << a_idx;
}

void get_attr_locations(const GPUVertFormat *format,
                        GPUAttrBinding *binding,
                        const GPUShaderInterface *shaderface)
{
  AttrBinding_clear(binding);

  for (uint a_idx = 0; a_idx < format->attr_len; ++a_idx) {
    const GPUVertAttr *a = &format->attrs[a_idx];
    for (uint n_idx = 0; n_idx < a->name_len; ++n_idx) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const GPUShaderInput *input = GPU_shaderinterface_attr(shaderface, name);
#if TRUST_NO_ONE
      assert(input != NULL);
      /* TODO: make this a recoverable runtime error?
       * indicates mismatch between vertex format and program. */
#endif
      write_attr_location(binding, a_idx, input->location);
    }
  }
}
