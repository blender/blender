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
 * Contributor(s): Bob Holcomb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file NOD_composite.h
 *  \ingroup nodes
 */

#ifndef NOD_composite_H
#define NOD_composite_H

#include "BKE_node.h"

extern bNodeTreeType ntreeType_Composite;


/* ****************** types array for all composite nodes ****************** */

void register_node_type_cmp_group(struct bNodeTreeType *ttype);
void register_node_type_cmp_forloop(struct bNodeTreeType *ttype);
void register_node_type_cmp_whileloop(struct bNodeTreeType *ttype);

void register_node_type_cmp_rlayers(struct bNodeTreeType *ttype);
void register_node_type_cmp_image(struct bNodeTreeType *ttype);
void register_node_type_cmp_texture(struct bNodeTreeType *ttype);
void register_node_type_cmp_value(struct bNodeTreeType *ttype);
void register_node_type_cmp_rgb(struct bNodeTreeType *ttype);
void register_node_type_cmp_curve_time(struct bNodeTreeType *ttype);
void register_node_type_cmp_movieclip(struct bNodeTreeType *ttype);

void register_node_type_cmp_composite(struct bNodeTreeType *ttype);
void register_node_type_cmp_viewer(struct bNodeTreeType *ttype);
void register_node_type_cmp_splitviewer(struct bNodeTreeType *ttype);
void register_node_type_cmp_output_file(struct bNodeTreeType *ttype);
void register_node_type_cmp_view_levels(struct bNodeTreeType *ttype);

void register_node_type_cmp_curve_rgb(struct bNodeTreeType *ttype);
void register_node_type_cmp_mix_rgb(struct bNodeTreeType *ttype);
void register_node_type_cmp_hue_sat(struct bNodeTreeType *ttype);
void register_node_type_cmp_brightcontrast(struct bNodeTreeType *ttype);
void register_node_type_cmp_gamma(struct bNodeTreeType *ttype);
void register_node_type_cmp_invert(struct bNodeTreeType *ttype);
void register_node_type_cmp_alphaover(struct bNodeTreeType *ttype);
void register_node_type_cmp_zcombine(struct bNodeTreeType *ttype);
void register_node_type_cmp_colorbalance(struct bNodeTreeType *ttype);
void register_node_type_cmp_huecorrect(struct bNodeTreeType *ttype);

void register_node_type_cmp_normal(struct bNodeTreeType *ttype);
void register_node_type_cmp_curve_vec(struct bNodeTreeType *ttype);
void register_node_type_cmp_map_value(struct bNodeTreeType *ttype);
void register_node_type_cmp_normalize(struct bNodeTreeType *ttype);

void register_node_type_cmp_filter(struct bNodeTreeType *ttype);
void register_node_type_cmp_blur(struct bNodeTreeType *ttype);
void register_node_type_cmp_dblur(struct bNodeTreeType *ttype);
void register_node_type_cmp_bilateralblur(struct bNodeTreeType *ttype);
void register_node_type_cmp_vecblur(struct bNodeTreeType *ttype);
void register_node_type_cmp_dilateerode(struct bNodeTreeType *ttype);
void register_node_type_cmp_defocus(struct bNodeTreeType *ttype);
void register_node_type_cmp_doubleedgemask(struct bNodeTreeType *ttype);

void register_node_type_cmp_valtorgb(struct bNodeTreeType *ttype);
void register_node_type_cmp_rgbtobw(struct bNodeTreeType *ttype);
void register_node_type_cmp_setalpha(struct bNodeTreeType *ttype);
void register_node_type_cmp_idmask(struct bNodeTreeType *ttype);
void register_node_type_cmp_math(struct bNodeTreeType *ttype);
void register_node_type_cmp_seprgba(struct bNodeTreeType *ttype);
void register_node_type_cmp_combrgba(struct bNodeTreeType *ttype);
void register_node_type_cmp_sephsva(struct bNodeTreeType *ttype);
void register_node_type_cmp_combhsva(struct bNodeTreeType *ttype);
void register_node_type_cmp_sepyuva(struct bNodeTreeType *ttype);
void register_node_type_cmp_combyuva(struct bNodeTreeType *ttype);
void register_node_type_cmp_sepycca(struct bNodeTreeType *ttype);
void register_node_type_cmp_combycca(struct bNodeTreeType *ttype); 
void register_node_type_cmp_premulkey(struct bNodeTreeType *ttype);

void register_node_type_cmp_diff_matte(struct bNodeTreeType *ttype);
void register_node_type_cmp_distance_matte(struct bNodeTreeType *ttype);
void register_node_type_cmp_chroma_matte(struct bNodeTreeType *ttype);
void register_node_type_cmp_color_matte(struct bNodeTreeType *ttype);
void register_node_type_cmp_channel_matte(struct bNodeTreeType *ttype);
void register_node_type_cmp_color_spill(struct bNodeTreeType *ttype);
void register_node_type_cmp_luma_matte(struct bNodeTreeType *ttype); 

void register_node_type_cmp_translate(struct bNodeTreeType *ttype);
void register_node_type_cmp_rotate(struct bNodeTreeType *ttype);
void register_node_type_cmp_scale(struct bNodeTreeType *ttype);
void register_node_type_cmp_flip(struct bNodeTreeType *ttype);
void register_node_type_cmp_crop(struct bNodeTreeType *ttype);
void register_node_type_cmp_displace(struct bNodeTreeType *ttype);
void register_node_type_cmp_mapuv(struct bNodeTreeType *ttype);
void register_node_type_cmp_transform(struct bNodeTreeType *ttype);
void register_node_type_cmp_stabilize2d(struct bNodeTreeType *ttype);
void register_node_type_cmp_moviedistortion(struct bNodeTreeType *ttype);

void register_node_type_cmp_glare(struct bNodeTreeType *ttype);
void register_node_type_cmp_tonemap(struct bNodeTreeType *ttype);
void register_node_type_cmp_lensdist(struct bNodeTreeType *ttype);

#endif
