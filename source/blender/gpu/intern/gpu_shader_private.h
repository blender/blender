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

/** \file \ingroup gpu
 */

#ifndef __GPU_SHADER_PRIVATE_H__
#define __GPU_SHADER_PRIVATE_H__

#include "GPU_glew.h"
#include "GPU_shader_interface.h"

struct GPUShader {
	/** Handle for full program (links shader stages below). */
	GLuint program;

	/** Handle for vertex shader. */
	GLuint vertex;
	/** Handle for geometry shader. */
	GLuint geometry;
	/** Handle for fragment shader. */
	GLuint fragment;

	/** Cached uniform & attribute interface for shader. */
	GPUShaderInterface *interface;

	int feedback_transform_type;
#ifndef NDEBUG
	char name[64];
#endif
};

#endif  /* __GPU_SHADER_PRIVATE_H__ */
