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

#ifndef __NOD_COMPOSITE_H__
#define __NOD_COMPOSITE_H__

#include "BKE_node.h"

extern struct bNodeTreeType *ntreeType_Composite;


/* ****************** types array for all composite nodes ****************** */

void register_node_tree_type_cmp(void);

void register_node_type_cmp_group(void);

void register_node_type_cmp_rlayers(void);
void register_node_type_cmp_image(void);
void register_node_type_cmp_texture(void);
void register_node_type_cmp_value(void);
void register_node_type_cmp_rgb(void);
void register_node_type_cmp_curve_time(void);
void register_node_type_cmp_movieclip(void);

void register_node_type_cmp_composite(void);
void register_node_type_cmp_viewer(void);
void register_node_type_cmp_splitviewer(void);
void register_node_type_cmp_output_file(void);
void register_node_type_cmp_view_levels(void);

void register_node_type_cmp_curve_rgb(void);
void register_node_type_cmp_mix_rgb(void);
void register_node_type_cmp_hue_sat(void);
void register_node_type_cmp_brightcontrast(void);
void register_node_type_cmp_gamma(void);
void register_node_type_cmp_invert(void);
void register_node_type_cmp_alphaover(void);
void register_node_type_cmp_zcombine(void);
void register_node_type_cmp_colorbalance(void);
void register_node_type_cmp_huecorrect(void);

void register_node_type_cmp_normal(void);
void register_node_type_cmp_curve_vec(void);
void register_node_type_cmp_map_value(void);
void register_node_type_cmp_map_range(void);
void register_node_type_cmp_normalize(void);

void register_node_type_cmp_filter(void);
void register_node_type_cmp_blur(void);
void register_node_type_cmp_dblur(void);
void register_node_type_cmp_bilateralblur(void);
void register_node_type_cmp_vecblur(void);
void register_node_type_cmp_dilateerode(void);
void register_node_type_cmp_inpaint(void);
void register_node_type_cmp_despeckle(void);
void register_node_type_cmp_defocus(void);

void register_node_type_cmp_valtorgb(void);
void register_node_type_cmp_rgbtobw(void);
void register_node_type_cmp_setalpha(void);
void register_node_type_cmp_idmask(void);
void register_node_type_cmp_math(void);
void register_node_type_cmp_seprgba(void);
void register_node_type_cmp_combrgba(void);
void register_node_type_cmp_sephsva(void);
void register_node_type_cmp_combhsva(void);
void register_node_type_cmp_sepyuva(void);
void register_node_type_cmp_combyuva(void);
void register_node_type_cmp_sepycca(void);
void register_node_type_cmp_combycca(void);
void register_node_type_cmp_premulkey(void);

void register_node_type_cmp_diff_matte(void);
void register_node_type_cmp_distance_matte(void);
void register_node_type_cmp_chroma_matte(void);
void register_node_type_cmp_color_matte(void);
void register_node_type_cmp_channel_matte(void);
void register_node_type_cmp_color_spill(void);
void register_node_type_cmp_luma_matte(void);
void register_node_type_cmp_doubleedgemask(void);
void register_node_type_cmp_keyingscreen(void);
void register_node_type_cmp_keying(void);

void register_node_type_cmp_translate(void);
void register_node_type_cmp_rotate(void);
void register_node_type_cmp_scale(void);
void register_node_type_cmp_flip(void);
void register_node_type_cmp_crop(void);
void register_node_type_cmp_displace(void);
void register_node_type_cmp_mapuv(void);
void register_node_type_cmp_transform(void);
void register_node_type_cmp_stabilize2d(void);
void register_node_type_cmp_moviedistortion(void);
void register_node_type_cmp_mask(void);

void register_node_type_cmp_glare(void);
void register_node_type_cmp_tonemap(void);
void register_node_type_cmp_lensdist(void);


void register_node_type_cmp_colorcorrection(void);
void register_node_type_cmp_boxmask(void);
void register_node_type_cmp_ellipsemask(void);
void register_node_type_cmp_bokehimage(void);
void register_node_type_cmp_bokehblur(void);
void register_node_type_cmp_switch(void);
void register_node_type_cmp_pixelate(void);
void register_node_type_cmp_trackpos(void);
void register_node_type_cmp_planetrackdeform(void);
void register_node_type_cmp_cornerpin(void);

void node_cmp_rlayers_force_hidden_passes(struct bNode *node);

#endif
