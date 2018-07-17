/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_shader_private.h
 *  \ingroup gpu
 */

#ifndef __GPU_SHADER_PRIVATE_H__
#define __GPU_SHADER_PRIVATE_H__

#include "GPU_glew.h"
#include "GPU_shader_interface.h"

struct GPUShader {
	GLuint program;  /* handle for full program (links shader stages below) */

	GLuint vertex;   /* handle for vertex shader */
	GLuint geometry; /* handle for geometry shader */
	GLuint fragment; /* handle for fragment shader */

	Gwn_ShaderInterface *interface; /* cached uniform & attrib interface for shader */

	int feedback_transform_type;
};

#endif  /* __GPU_SHADER_PRIVATE_H__ */
