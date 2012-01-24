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

void register_node_type_sh_group(struct bNodeTreeType *ttype);
void register_node_type_sh_forloop(struct bNodeTreeType *ttype);
void register_node_type_sh_whileloop(struct bNodeTreeType *ttype);

void register_node_type_sh_output(struct bNodeTreeType *ttype);
void register_node_type_sh_material(struct bNodeTreeType *ttype);
void register_node_type_sh_camera(struct bNodeTreeType *ttype);
void register_node_type_sh_value(struct bNodeTreeType *ttype);
void register_node_type_sh_rgb(struct bNodeTreeType *ttype);
void register_node_type_sh_mix_rgb(struct bNodeTreeType *ttype);
void register_node_type_sh_valtorgb(struct bNodeTreeType *ttype);
void register_node_type_sh_rgbtobw(struct bNodeTreeType *ttype);
void register_node_type_sh_texture(struct bNodeTreeType *ttype);
void register_node_type_sh_normal(struct bNodeTreeType *ttype);
void register_node_type_sh_gamma(struct bNodeTreeType *ttype);
void register_node_type_sh_brightcontrast(struct bNodeTreeType *ttype);
void register_node_type_sh_geom(struct bNodeTreeType *ttype);
void register_node_type_sh_mapping(struct bNodeTreeType *ttype);
void register_node_type_sh_curve_vec(struct bNodeTreeType *ttype);
void register_node_type_sh_curve_rgb(struct bNodeTreeType *ttype);
void register_node_type_sh_math(struct bNodeTreeType *ttype);
void register_node_type_sh_vect_math(struct bNodeTreeType *ttype);
void register_node_type_sh_squeeze(struct bNodeTreeType *ttype);
void register_node_type_sh_dynamic(struct bNodeTreeType *ttype);
void register_node_type_sh_material_ext(struct bNodeTreeType *ttype);
void register_node_type_sh_invert(struct bNodeTreeType *ttype);
void register_node_type_sh_seprgb(struct bNodeTreeType *ttype);
void register_node_type_sh_combrgb(struct bNodeTreeType *ttype);
void register_node_type_sh_hue_sat(struct bNodeTreeType *ttype);

void register_node_type_sh_attribute(struct bNodeTreeType *ttype);
void register_node_type_sh_geometry(struct bNodeTreeType *ttype);
void register_node_type_sh_light_path(struct bNodeTreeType *ttype);
void register_node_type_sh_fresnel(struct bNodeTreeType *ttype);
void register_node_type_sh_layer_weight(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_coord(struct bNodeTreeType *ttype);

void register_node_type_sh_background(struct bNodeTreeType *ttype);
void register_node_type_sh_bsdf_diffuse(struct bNodeTreeType *ttype);
void register_node_type_sh_bsdf_glossy(struct bNodeTreeType *ttype);
void register_node_type_sh_bsdf_glass(struct bNodeTreeType *ttype);
void register_node_type_sh_bsdf_translucent(struct bNodeTreeType *ttype);
void register_node_type_sh_bsdf_transparent(struct bNodeTreeType *ttype);
void register_node_type_sh_bsdf_velvet(struct bNodeTreeType *ttype);
void register_node_type_sh_emission(struct bNodeTreeType *ttype);
void register_node_type_sh_holdout(struct bNodeTreeType *ttype);
void register_node_type_sh_volume_transparent(struct bNodeTreeType *ttype);
void register_node_type_sh_volume_isotropic(struct bNodeTreeType *ttype);
void register_node_type_sh_mix_shader(struct bNodeTreeType *ttype);
void register_node_type_sh_add_shader(struct bNodeTreeType *ttype);

void register_node_type_sh_output_lamp(struct bNodeTreeType *ttype);
void register_node_type_sh_output_material(struct bNodeTreeType *ttype);
void register_node_type_sh_output_world(struct bNodeTreeType *ttype);

void register_node_type_sh_tex_image(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_environment(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_sky(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_voronoi(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_gradient(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_magic(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_wave(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_musgrave(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_noise(struct bNodeTreeType *ttype);
void register_node_type_sh_tex_checker(struct bNodeTreeType *ttype);

#endif


