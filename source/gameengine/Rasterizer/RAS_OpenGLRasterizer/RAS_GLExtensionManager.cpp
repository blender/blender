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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Rasterizer/RAS_OpenGLRasterizer/RAS_GLExtensionManager.cpp
 *  \ingroup bgerastogl
 */


#include <iostream>

#include "RAS_GLExtensionManager.h"

namespace bgl
{
	void InitExtensions(bool debug)
	{
		static bool firsttime = true;
		
		if (firsttime) {
			firsttime = false;

			if (debug) {
				if (GLEW_ATI_pn_triangles)
					std::cout << "Enabled GL_ATI_pn_triangles" << std::endl;
				if (GLEW_ARB_texture_env_combine)
					std::cout << "Detected GL_ARB_texture_env_combine" << std::endl;
				if (GLEW_ARB_texture_cube_map)
					std::cout << "Detected GL_ARB_texture_cube_map" << std::endl;
				if (GLEW_ARB_multitexture)
					std::cout << "Detected GL_ARB_multitexture" << std::endl;
				if (GLEW_ARB_shader_objects)
					std::cout << "Detected GL_ARB_shader_objects" << std::endl;
				if (GLEW_ARB_vertex_shader)
					std::cout << "Detected GL_ARB_vertex_shader" << std::endl;
				if (GLEW_ARB_fragment_shader)
					std::cout << "Detected GL_ARB_fragment_shader" << std::endl;
				if (GLEW_ARB_vertex_program)
					std::cout << "Detected GL_ARB_vertex_program" << std::endl;
				if (GLEW_ARB_depth_texture)
					std::cout << "Detected GL_ARB_depth_texture" << std::endl;
				if (GLEW_EXT_separate_specular_color)
					std::cout << "Detected GL_EXT_separate_specular_color" << std::endl;
			}
		}
	}
} // namespace bgl

