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

#ifndef NOD_TEXTURE_H
#define NOD_TEXTURE_H

#include "BKE_node.h"

extern bNodeTreeType ntreeType_Texture;


/* ****************** types array for all texture nodes ****************** */

void register_node_type_tex_group(struct bNodeTreeType *ttype);
void register_node_type_tex_forloop(struct bNodeTreeType *ttype);
void register_node_type_tex_whileloop(struct bNodeTreeType *ttype);

void register_node_type_tex_math(struct bNodeTreeType *ttype);
void register_node_type_tex_mix_rgb(struct bNodeTreeType *ttype);
void register_node_type_tex_valtorgb(struct bNodeTreeType *ttype);
void register_node_type_tex_valtonor(struct bNodeTreeType *ttype);
void register_node_type_tex_rgbtobw(struct bNodeTreeType *ttype);
void register_node_type_tex_output(struct bNodeTreeType *ttype);
void register_node_type_tex_viewer(struct bNodeTreeType *ttype);
void register_node_type_tex_checker(struct bNodeTreeType *ttype);
void register_node_type_tex_texture(struct bNodeTreeType *ttype);
void register_node_type_tex_bricks(struct bNodeTreeType *ttype);
void register_node_type_tex_image(struct bNodeTreeType *ttype);
void register_node_type_tex_curve_rgb(struct bNodeTreeType *ttype);
void register_node_type_tex_curve_time(struct bNodeTreeType *ttype);
void register_node_type_tex_invert(struct bNodeTreeType *ttype);
void register_node_type_tex_hue_sat(struct bNodeTreeType *ttype);
void register_node_type_tex_coord(struct bNodeTreeType *ttype);
void register_node_type_tex_distance(struct bNodeTreeType *ttype);

void register_node_type_tex_rotate(struct bNodeTreeType *ttype);
void register_node_type_tex_translate(struct bNodeTreeType *ttype);
void register_node_type_tex_scale(struct bNodeTreeType *ttype);
void register_node_type_tex_at(struct bNodeTreeType *ttype);

void register_node_type_tex_compose(struct bNodeTreeType *ttype);
void register_node_type_tex_decompose(struct bNodeTreeType *ttype);

void register_node_type_tex_proc_voronoi(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_blend(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_magic(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_marble(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_clouds(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_wood(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_musgrave(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_noise(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_stucci(struct bNodeTreeType *ttype);
void register_node_type_tex_proc_distnoise(struct bNodeTreeType *ttype);

#endif
