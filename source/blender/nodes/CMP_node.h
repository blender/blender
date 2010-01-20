/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef CMP_NODE_H
#define CMP_NODE_H

#include "BKE_node.h"


/* ****************** types array for all composite nodes ****************** */

extern bNodeType cmp_node_rlayers;
extern bNodeType cmp_node_image;
extern bNodeType cmp_node_texture;
extern bNodeType cmp_node_value;
extern bNodeType cmp_node_rgb;
extern bNodeType cmp_node_curve_time;

extern bNodeType cmp_node_composite;
extern bNodeType cmp_node_viewer;
extern bNodeType cmp_node_splitviewer;
extern bNodeType cmp_node_output_file;
extern bNodeType cmp_node_view_levels;

extern bNodeType cmp_node_curve_rgb;
extern bNodeType cmp_node_mix_rgb;
extern bNodeType cmp_node_hue_sat;
extern bNodeType cmp_node_brightcontrast;
extern bNodeType cmp_node_gamma;
extern bNodeType cmp_node_invert;
extern bNodeType cmp_node_alphaover;
extern bNodeType cmp_node_zcombine;
extern bNodeType cmp_node_colorbalance;

extern bNodeType cmp_node_normal;
extern bNodeType cmp_node_curve_vec;
extern bNodeType cmp_node_map_value;
extern bNodeType cmp_node_normalize;

extern bNodeType cmp_node_filter;
extern bNodeType cmp_node_blur;
extern bNodeType cmp_node_dblur;
extern bNodeType cmp_node_bilateralblur;
extern bNodeType cmp_node_vecblur;
extern bNodeType cmp_node_dilateerode;
extern bNodeType cmp_node_defocus;

extern bNodeType cmp_node_valtorgb;
extern bNodeType cmp_node_rgbtobw;	
extern bNodeType cmp_node_setalpha;
extern bNodeType cmp_node_idmask;
extern bNodeType cmp_node_math;
extern bNodeType cmp_node_seprgba;
extern bNodeType cmp_node_combrgba;
extern bNodeType cmp_node_sephsva;
extern bNodeType cmp_node_combhsva;
extern bNodeType cmp_node_sepyuva;
extern bNodeType cmp_node_combyuva;
extern bNodeType cmp_node_sepycca;
extern bNodeType cmp_node_combycca; 
extern bNodeType cmp_node_premulkey;

extern bNodeType cmp_node_diff_matte;
extern bNodeType cmp_node_distance_matte;
extern bNodeType cmp_node_chroma_matte;
extern bNodeType cmp_node_color_matte;
extern bNodeType cmp_node_channel_matte;
extern bNodeType cmp_node_color_spill;
extern bNodeType cmp_node_luma_matte; 

extern bNodeType cmp_node_translate;
extern bNodeType cmp_node_rotate;
extern bNodeType cmp_node_scale;
extern bNodeType cmp_node_flip;
extern bNodeType cmp_node_crop;
extern bNodeType cmp_node_displace;
extern bNodeType cmp_node_mapuv;

extern bNodeType cmp_node_glare;
extern bNodeType cmp_node_tonemap;
extern bNodeType cmp_node_lensdist;

#endif
