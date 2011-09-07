/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software(ListBase *lb); you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation(ListBase *lb); either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY(ListBase *lb); without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program(ListBase *lb); if not, write to the Free Software Foundation,
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

void register_node_type_cmp_group(ListBase *lb);
void register_node_type_cmp_forloop(ListBase *lb);
void register_node_type_cmp_whileloop(ListBase *lb);

void register_node_type_cmp_rlayers(ListBase *lb);
void register_node_type_cmp_image(ListBase *lb);
void register_node_type_cmp_texture(ListBase *lb);
void register_node_type_cmp_value(ListBase *lb);
void register_node_type_cmp_rgb(ListBase *lb);
void register_node_type_cmp_curve_time(ListBase *lb);
void register_node_type_cmp_movieclip(ListBase *lb);

void register_node_type_cmp_composite(ListBase *lb);
void register_node_type_cmp_viewer(ListBase *lb);
void register_node_type_cmp_splitviewer(ListBase *lb);
void register_node_type_cmp_output_file(ListBase *lb);
void register_node_type_cmp_view_levels(ListBase *lb);

void register_node_type_cmp_curve_rgb(ListBase *lb);
void register_node_type_cmp_mix_rgb(ListBase *lb);
void register_node_type_cmp_hue_sat(ListBase *lb);
void register_node_type_cmp_brightcontrast(ListBase *lb);
void register_node_type_cmp_gamma(ListBase *lb);
void register_node_type_cmp_invert(ListBase *lb);
void register_node_type_cmp_alphaover(ListBase *lb);
void register_node_type_cmp_zcombine(ListBase *lb);
void register_node_type_cmp_colorbalance(ListBase *lb);
void register_node_type_cmp_huecorrect(ListBase *lb);

void register_node_type_cmp_normal(ListBase *lb);
void register_node_type_cmp_curve_vec(ListBase *lb);
void register_node_type_cmp_map_value(ListBase *lb);
void register_node_type_cmp_normalize(ListBase *lb);

void register_node_type_cmp_filter(ListBase *lb);
void register_node_type_cmp_blur(ListBase *lb);
void register_node_type_cmp_dblur(ListBase *lb);
void register_node_type_cmp_bilateralblur(ListBase *lb);
void register_node_type_cmp_vecblur(ListBase *lb);
void register_node_type_cmp_dilateerode(ListBase *lb);
void register_node_type_cmp_defocus(ListBase *lb);

void register_node_type_cmp_valtorgb(ListBase *lb);
void register_node_type_cmp_rgbtobw(ListBase *lb);	
void register_node_type_cmp_setalpha(ListBase *lb);
void register_node_type_cmp_idmask(ListBase *lb);
void register_node_type_cmp_math(ListBase *lb);
void register_node_type_cmp_seprgba(ListBase *lb);
void register_node_type_cmp_combrgba(ListBase *lb);
void register_node_type_cmp_sephsva(ListBase *lb);
void register_node_type_cmp_combhsva(ListBase *lb);
void register_node_type_cmp_sepyuva(ListBase *lb);
void register_node_type_cmp_combyuva(ListBase *lb);
void register_node_type_cmp_sepycca(ListBase *lb);
void register_node_type_cmp_combycca(ListBase *lb); 
void register_node_type_cmp_premulkey(ListBase *lb);

void register_node_type_cmp_diff_matte(ListBase *lb);
void register_node_type_cmp_distance_matte(ListBase *lb);
void register_node_type_cmp_chroma_matte(ListBase *lb);
void register_node_type_cmp_color_matte(ListBase *lb);
void register_node_type_cmp_channel_matte(ListBase *lb);
void register_node_type_cmp_color_spill(ListBase *lb);
void register_node_type_cmp_luma_matte(ListBase *lb); 

void register_node_type_cmp_translate(ListBase *lb);
void register_node_type_cmp_rotate(ListBase *lb);
void register_node_type_cmp_scale(ListBase *lb);
void register_node_type_cmp_flip(ListBase *lb);
void register_node_type_cmp_crop(ListBase *lb);
void register_node_type_cmp_displace(ListBase *lb);
void register_node_type_cmp_mapuv(ListBase *lb);
void register_node_type_cmp_transform(ListBase *lb);
void register_node_type_cmp_stabilize2d(ListBase *lb);

void register_node_type_cmp_glare(ListBase *lb);
void register_node_type_cmp_tonemap(ListBase *lb);
void register_node_type_cmp_lensdist(ListBase *lb);

#endif
