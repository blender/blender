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

#ifndef __NOD_SHADER_H__
#define __NOD_SHADER_H__

#include "BKE_node.h"

extern struct bNodeTreeType *ntreeType_Shader;


/* the type definitions array */
/* ****************** types array for all shaders ****************** */

void register_node_tree_type_sh(void);

void register_node_type_sh_group(void);

void register_node_type_sh_output(void);
void register_node_type_sh_material(void);
void register_node_type_sh_camera(void);
void register_node_type_sh_lamp(void);
void register_node_type_sh_value(void);
void register_node_type_sh_rgb(void);
void register_node_type_sh_mix_rgb(void);
void register_node_type_sh_valtorgb(void);
void register_node_type_sh_rgbtobw(void);
void register_node_type_sh_texture(void);
void register_node_type_sh_normal(void);
void register_node_type_sh_gamma(void);
void register_node_type_sh_brightcontrast(void);
void register_node_type_sh_geom(void);
void register_node_type_sh_mapping(void);
void register_node_type_sh_curve_vec(void);
void register_node_type_sh_curve_rgb(void);
void register_node_type_sh_math(void);
void register_node_type_sh_vect_math(void);
void register_node_type_sh_squeeze(void);
void register_node_type_sh_dynamic(void);
void register_node_type_sh_material_ext(void);
void register_node_type_sh_invert(void);
void register_node_type_sh_seprgb(void);
void register_node_type_sh_combrgb(void);
void register_node_type_sh_sephsv(void);
void register_node_type_sh_combhsv(void);
void register_node_type_sh_hue_sat(void);
void register_node_type_sh_tex_brick(void);

void register_node_type_sh_attribute(void);
void register_node_type_sh_geometry(void);
void register_node_type_sh_light_path(void);
void register_node_type_sh_light_falloff(void);
void register_node_type_sh_object_info(void);
void register_node_type_sh_fresnel(void);
void register_node_type_sh_wireframe(void);
void register_node_type_sh_wavelength(void);
void register_node_type_sh_blackbody(void);
void register_node_type_sh_layer_weight(void);
void register_node_type_sh_tex_coord(void);
void register_node_type_sh_particle_info(void);
void register_node_type_sh_hair_info(void);
void register_node_type_sh_script(void);
void register_node_type_sh_normal_map(void);
void register_node_type_sh_tangent(void);
void register_node_type_sh_vect_transform(void);

void register_node_type_sh_ambient_occlusion(void);
void register_node_type_sh_background(void);
void register_node_type_sh_bsdf_diffuse(void);
void register_node_type_sh_bsdf_glossy(void);
void register_node_type_sh_bsdf_glass(void);
void register_node_type_sh_bsdf_refraction(void);
void register_node_type_sh_bsdf_translucent(void);
void register_node_type_sh_bsdf_transparent(void);
void register_node_type_sh_bsdf_velvet(void);
void register_node_type_sh_bsdf_toon(void);
void register_node_type_sh_bsdf_anisotropic(void);
void register_node_type_sh_emission(void);
void register_node_type_sh_holdout(void);
void register_node_type_sh_volume_absorption(void);
void register_node_type_sh_volume_scatter(void);
void register_node_type_sh_bsdf_hair(void);
void register_node_type_sh_subsurface_scattering(void);
void register_node_type_sh_mix_shader(void);
void register_node_type_sh_add_shader(void);
void register_node_type_sh_uvmap(void);

void register_node_type_sh_output_lamp(void);
void register_node_type_sh_output_material(void);
void register_node_type_sh_output_world(void);

void register_node_type_sh_tex_image(void);
void register_node_type_sh_tex_environment(void);
void register_node_type_sh_tex_sky(void);
void register_node_type_sh_tex_voronoi(void);
void register_node_type_sh_tex_gradient(void);
void register_node_type_sh_tex_magic(void);
void register_node_type_sh_tex_wave(void);
void register_node_type_sh_tex_musgrave(void);
void register_node_type_sh_tex_noise(void);
void register_node_type_sh_tex_checker(void);
void register_node_type_sh_bump(void);

#endif


