/**
 * $Id: CMP_node.h 12429 2007-10-29 14:37:19Z bebraw $
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

#ifndef TEX_NODE_H
#define TEX_NODE_H

#include "BKE_node.h"


/* ****************** types array for all texture nodes ****************** */

bNodeType tex_node_math;
bNodeType tex_node_mix_rgb;
bNodeType tex_node_valtorgb;
bNodeType tex_node_valtonor;
bNodeType tex_node_rgbtobw;
bNodeType tex_node_output;
bNodeType tex_node_viewer;
bNodeType tex_node_checker;
bNodeType tex_node_texture;
bNodeType tex_node_bricks;
bNodeType tex_node_image;
bNodeType tex_node_curve_rgb;
bNodeType tex_node_curve_time;
bNodeType tex_node_invert;
bNodeType tex_node_hue_sat;
bNodeType tex_node_coord;
bNodeType tex_node_distance;

bNodeType tex_node_rotate;
bNodeType tex_node_translate;
bNodeType tex_node_scale;

bNodeType tex_node_compose;
bNodeType tex_node_decompose;

bNodeType tex_node_proc_voronoi;
bNodeType tex_node_proc_blend;
bNodeType tex_node_proc_magic;
bNodeType tex_node_proc_marble;
bNodeType tex_node_proc_clouds;
bNodeType tex_node_proc_wood;
bNodeType tex_node_proc_musgrave;
bNodeType tex_node_proc_noise;
bNodeType tex_node_proc_stucci;
bNodeType tex_node_proc_distnoise;

#endif
