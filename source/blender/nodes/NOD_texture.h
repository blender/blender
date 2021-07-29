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

/** \file NOD_texture.h
 *  \ingroup nodes
 */

#ifndef __NOD_TEXTURE_H__
#define __NOD_TEXTURE_H__

#include "BKE_node.h"

extern struct bNodeTreeType *ntreeType_Texture;


/* ****************** types array for all texture nodes ****************** */

void register_node_tree_type_tex(void);

void register_node_type_tex_group(void);

void register_node_type_tex_math(void);
void register_node_type_tex_mix_rgb(void);
void register_node_type_tex_valtorgb(void);
void register_node_type_tex_valtonor(void);
void register_node_type_tex_rgbtobw(void);
void register_node_type_tex_output(void);
void register_node_type_tex_viewer(void);
void register_node_type_tex_checker(void);
void register_node_type_tex_texture(void);
void register_node_type_tex_bricks(void);
void register_node_type_tex_image(void);
void register_node_type_tex_curve_rgb(void);
void register_node_type_tex_curve_time(void);
void register_node_type_tex_invert(void);
void register_node_type_tex_hue_sat(void);
void register_node_type_tex_coord(void);
void register_node_type_tex_distance(void);

void register_node_type_tex_rotate(void);
void register_node_type_tex_translate(void);
void register_node_type_tex_scale(void);
void register_node_type_tex_at(void);

void register_node_type_tex_compose(void);
void register_node_type_tex_decompose(void);

void register_node_type_tex_proc_voronoi(void);
void register_node_type_tex_proc_blend(void);
void register_node_type_tex_proc_magic(void);
void register_node_type_tex_proc_marble(void);
void register_node_type_tex_proc_clouds(void);
void register_node_type_tex_proc_wood(void);
void register_node_type_tex_proc_musgrave(void);
void register_node_type_tex_proc_noise(void);
void register_node_type_tex_proc_stucci(void);
void register_node_type_tex_proc_distnoise(void);

#endif
