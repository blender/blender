/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file NOD_shader.h
 *  \ingroup nodes
 */

#ifndef NOD_SHADER_H
#define NOD_SHADER_H

#include "BKE_node.h"

extern struct bNodeTreeType ntreeType_Shader;


/* the type definitions array */
/* ****************** types array for all shaders ****************** */

void register_node_type_sh_group(ListBase *lb);
void register_node_type_sh_forloop(ListBase *lb);
void register_node_type_sh_whileloop(ListBase *lb);

void register_node_type_sh_value(ListBase *lb);
void register_node_type_sh_rgb(ListBase *lb);
void register_node_type_sh_math(ListBase *lb);
void register_node_type_sh_vect_math(ListBase *lb);
void register_node_type_sh_mix_rgb(ListBase *lb);
void register_node_type_sh_rgbtobw(ListBase *lb);
void register_node_type_sh_mapping(ListBase *lb);
void register_node_type_sh_texture(ListBase *lb);

void register_node_type_sh_attribute(ListBase *lb);
void register_node_type_sh_geometry(ListBase *lb);
void register_node_type_sh_light_path(ListBase *lb);
void register_node_type_sh_fresnel(ListBase *lb);
void register_node_type_sh_blend_weight(ListBase *lb);
void register_node_type_sh_tex_coord(ListBase *lb);

void register_node_type_sh_background(ListBase *lb);
void register_node_type_sh_bsdf_diffuse(ListBase *lb);
void register_node_type_sh_bsdf_glossy(ListBase *lb);
void register_node_type_sh_bsdf_glass(ListBase *lb);
void register_node_type_sh_bsdf_anisotropic(ListBase *lb);
void register_node_type_sh_bsdf_translucent(ListBase *lb);
void register_node_type_sh_bsdf_transparent(ListBase *lb);
void register_node_type_sh_bsdf_velvet(ListBase *lb);
void register_node_type_sh_emission(ListBase *lb);
void register_node_type_sh_holdout(ListBase *lb);
void register_node_type_sh_mix_shader(ListBase *lb);
void register_node_type_sh_add_shader(ListBase *lb);

void register_node_type_sh_output_lamp(ListBase *lb);
void register_node_type_sh_output_material(ListBase *lb);
void register_node_type_sh_output_texture(ListBase *lb);
void register_node_type_sh_output_world(ListBase *lb);

void register_node_type_sh_tex_image(ListBase *lb);
void register_node_type_sh_tex_environment(ListBase *lb);
void register_node_type_sh_tex_sky(ListBase *lb);
void register_node_type_sh_tex_voronoi(ListBase *lb);
void register_node_type_sh_tex_blend(ListBase *lb);
void register_node_type_sh_tex_magic(ListBase *lb);
void register_node_type_sh_tex_marble(ListBase *lb);
void register_node_type_sh_tex_clouds(ListBase *lb);
void register_node_type_sh_tex_wood(ListBase *lb);
void register_node_type_sh_tex_musgrave(ListBase *lb);
void register_node_type_sh_tex_noise(ListBase *lb);
void register_node_type_sh_tex_stucci(ListBase *lb);
void register_node_type_sh_tex_distnoise(ListBase *lb);

#endif


