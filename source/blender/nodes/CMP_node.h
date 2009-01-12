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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CMP_NODE_H
#define CMP_NODE_H

#include "BKE_node.h"


/* ****************** types array for all composite nodes ****************** */

bNodeType cmp_node_rlayers;
bNodeType cmp_node_image;
bNodeType cmp_node_texture;
bNodeType cmp_node_value;
bNodeType cmp_node_rgb;
bNodeType cmp_node_curve_time;

bNodeType cmp_node_composite;
bNodeType cmp_node_viewer;
bNodeType cmp_node_splitviewer;
bNodeType cmp_node_output_file;

bNodeType cmp_node_curve_rgb;
bNodeType cmp_node_mix_rgb;
bNodeType cmp_node_hue_sat;
bNodeType cmp_node_brightcontrast;
bNodeType cmp_node_gamma;
bNodeType cmp_node_invert;
bNodeType cmp_node_alphaover;
bNodeType cmp_node_zcombine;

bNodeType cmp_node_normal;
bNodeType cmp_node_curve_vec;
bNodeType cmp_node_map_value;
bNodeType cmp_node_normalize;

bNodeType cmp_node_filter;
bNodeType cmp_node_blur;
bNodeType cmp_node_dblur;
bNodeType cmp_node_bilateralblur;
bNodeType cmp_node_vecblur;
bNodeType cmp_node_dilateerode;
bNodeType cmp_node_defocus;

bNodeType cmp_node_valtorgb;
bNodeType cmp_node_rgbtobw;	
bNodeType cmp_node_setalpha;
bNodeType cmp_node_idmask;
bNodeType cmp_node_math;
bNodeType cmp_node_seprgba;
bNodeType cmp_node_combrgba;
bNodeType cmp_node_sephsva;
bNodeType cmp_node_combhsva;
bNodeType cmp_node_sepyuva;
bNodeType cmp_node_combyuva;
bNodeType cmp_node_sepycca;
bNodeType cmp_node_combycca; 
bNodeType cmp_node_premulkey;

bNodeType cmp_node_diff_matte;
bNodeType cmp_node_chroma;
bNodeType cmp_node_channel_matte;
bNodeType cmp_node_color_spill;
bNodeType cmp_node_luma_matte; 

bNodeType cmp_node_translate;
bNodeType cmp_node_rotate;
bNodeType cmp_node_scale;
bNodeType cmp_node_flip;
bNodeType cmp_node_crop;
bNodeType cmp_node_displace;
bNodeType cmp_node_mapuv;

bNodeType cmp_node_glare;
bNodeType cmp_node_tonemap;
bNodeType cmp_node_lensdist;

#endif
