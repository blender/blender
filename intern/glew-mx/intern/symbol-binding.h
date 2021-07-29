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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file intern/symbol-binding.h
 *  \ingroup glew-mx
 *
 * This file is for any simple stuff that is missing from GLEW when
 * compiled with either the GLEW_ES_ONLY or the GLEW_NO_ES flag.
 *
 * Should be limited to symbolic constants.
 *
 * This file is NOT for checking DEPRECATED OpenGL symbolic constants.
 */

#ifndef __SYMBOL_BINDING_H__
#define __SYMBOL_BINDING_H__

#ifndef __GLEW_MX_H__
#error This file is meant to be included from glew-mx.h
#endif


#ifdef GLEW_ES_ONLY

/* ES does not support the GLdouble type. */
#ifndef GLdouble
#define GLdouble double
#endif

/*
 * Need stubs for these version checks if compiling with only ES support.
 * Rely on compiler to eliminate unreachable code when version checks become constants.
 */

#ifndef GLEW_VERSION_1_1
#define GLEW_VERSION_1_1 0
#endif

#ifndef GLEW_VERSION_1_2
#define GLEW_VERSION_1_2 0
#endif

#ifndef GLEW_VERSION_1_3
#define GLEW_VERSION_1_3 0
#endif

#ifndef GLEW_VERSION_1_4
#define GLEW_VERSION_1_4 0
#endif

#ifndef GLEW_VERSION_1_5
#define GLEW_VERSION_1_5 0
#endif

#ifndef GLEW_VERSION_2_0
#define GLEW_VERSION_2_0 0
#endif

#ifndef GLEW_VERSION_3_0
#define GLEW_VERSION_3_0 0
#endif

#ifndef GLEW_ARB_shader_objects
#define GLEW_ARB_shader_objects 0
#endif

#ifndef GLEW_ARB_vertex_shader
#define GLEW_ARB_vertex_shader 0
#endif

#ifndef GLEW_ARB_vertex_program
#define GLEW_ARB_vertex_program 0
#endif

#ifndef GLEW_ARB_fragment_program
#define GLEW_ARB_fragment_program 0
#endif

#ifndef GLEW_ARB_vertex_buffer_object
#define GLEW_ARB_vertex_buffer_object 0
#endif

#ifndef GLEW_ARB_framebuffer_object
#define GLEW_ARB_framebuffer_object 0
#endif

#ifndef GLEW_ARB_multitexture
#define GLEW_ARB_multitexture 0
#endif

#ifndef GLEW_EXT_framebuffer_object
#define GLEW_EXT_framebuffer_object 0
#endif

#ifndef GLEW_ARB_depth_texture
#define GLEW_ARB_depth_texture 0
#endif

#ifndef GLEW_ARB_shadow
#define GLEW_ARB_shadow 0
#endif

#ifndef GLEW_ARB_texture_float
#define GLEW_ARB_texture_float 0
#endif

#ifndef GLEW_ARB_texture_non_power_of_two
#define GLEW_ARB_texture_non_power_of_two 0
#endif

#ifndef GLEW_ARB_texture3D
#define GLEW_ARB_texture3D 0
#endif

#ifndef GLEW_EXT_texture3D
#define GLEW_EXT_texture3D 0
#endif

#ifndef GLEW_ARB_texture_rg
#define GLEW_ARB_texture_rg 0
#endif

#ifndef GLEW_ARB_texture_query_lod
#define GLEW_ARB_texture_query_lod 0
#endif


/*
 * The following symbolic constants are missing from an ES only header,
 * so alias them to their (same valued) extension versions which are available in the header.
 *
 * Be careful that this does not lead to unguarded use of what are extensions in ES!
 *
 * Some of these may be here simply to patch inconsistencies in the header files.
 */

#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D GL_TEXTURE_3D_OES
#endif

#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R GL_TEXTURE_WRAP_R_OES
#endif

#ifndef GL_TEXTURE_COMPARE_MODE
#define GL_TEXTURE_COMPARE_MODE GL_TEXTURE_COMPARE_MODE_EXT
#endif

#ifndef GL_COMPARE_REF_TO_TEXTURE
#define GL_COMPARE_REF_TO_TEXTURE GL_COMPARE_REF_TO_TEXTURE_EXT
#endif

#ifndef GL_TEXTURE_COMPARE_FUNC
#define GL_TEXTURE_COMPARE_FUNC GL_TEXTURE_COMPARE_FUNC_EXT
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 GL_RGBA8_OES
#endif

#ifndef GL_RGBA16F
#define GL_RGBA16F GL_RGBA16F_EXT
#endif

#ifndef GL_RG32F
#define GL_RG32F GL_RG32F_EXT
#endif

#ifndef GL_RGB8
#define GL_RGB8 GL_RGB8_OES
#endif

#ifndef GL_RG
#define GL_RG GL_RG_EXT
#endif

#ifndef GL_RED
#define GL_RED GL_RED_EXT
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_OES
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_OES
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY GL_WRITE_ONLY_OES
#endif

#ifndef GLEW_ARB_vertex_array_object
#define GLEW_ARB_vertex_array_object 0
#endif


/* end of ifdef GLEW_ES_ONLY */
#elif defined(GLEW_NO_ES)


/*
 * Need stubs for these version checks if compiling without any support.
 * Rely on compiler to eliminate unreachable code when version checks become constants
 */

#ifndef GLEW_ES_VERSION_2_0
#define GLEW_ES_VERSION_2_0 0
#endif

#ifndef GLEW_EXT_texture_storage
#define GLEW_EXT_texture_storage 0
#endif

#ifndef GLEW_OES_framebuffer_object
#define GLEW_OES_framebuffer_object 0
#endif

#ifndef GLEW_OES_mapbuffer
#define GLEW_OES_mapbuffer 0
#endif

#ifndef GLEW_OES_required_internalformat
#define GLEW_OES_required_internalformat 0
#endif

#ifndef GLEW_EXT_color_buffer_half_float
#define GLEW_EXT_color_buffer_half_float 0
#endif

#ifndef GLEW_OES_depth_texture
#define GLEW_OES_depth_texture 0
#endif

#ifndef GLEW_EXT_shadow_samplers
#define GLEW_EXT_shadow_samplers 0
#endif

#ifndef GLEW_ARB_texture3D
#define GLEW_ARB_texture3D 0
#endif

#ifndef GLEW_OES_texture_3D
#define GLEW_OES_texture_3D 0
#endif

#ifndef GLEW_EXT_texture_rg
#define GLEW_EXT_texture_rg 0
#endif

#ifndef GLEW_OES_vertex_array_object
#define GLEW_OES_vertex_array_object 0
#endif


/*
 * The following symbolic constants are missing when there is no ES support,
 * so alias them to their (same valued) extension versions which are available in the header.
 *
 * Desktop GL typically does not have any extensions that originated from ES,
 * unlike ES which has many extensions to replace what was taken out.
 *
 * For that reason these aliases are more likely just patching inconsistencies in the header files.
 */

#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT
#endif

#endif /* ifdef GLEW_NO_ES */


#endif /* __SYMBOL_BINDING_H__*/
