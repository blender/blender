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
 * GL implementation of GPUBatch.
 * The only specificity of GL here is that it caches a list of
 * Vertex Array Objects based on the bound shader interface.
 */

#include "BLI_assert.h"

#include "glew-mx.h"

#include "gpu_batch_private.hh"
#include "gpu_primitive_private.h"

#include "gl_batch.hh"

using namespace blender::gpu;

GLBatch::GLBatch(void)
{
}

GLBatch::~GLBatch()
{
}

void GLBatch::draw(int v_first, int v_count, int i_first, int i_count)
{
  UNUSED_VARS(v_first, v_count, i_first, i_count);
}