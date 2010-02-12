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

#ifndef TEX_NODE_H
#define TEX_NODE_H

#include "BKE_node.h"


/* ****************** types array for all texture nodes ****************** */

extern bNodeType tex_node_math;
extern bNodeType tex_node_mix_rgb;
extern bNodeType tex_node_valtorgb;
extern bNodeType tex_node_valtonor;
extern bNodeType tex_node_rgbtobw;
extern bNodeType tex_node_output;
extern bNodeType tex_node_viewer;
extern bNodeType tex_node_checker;
extern bNodeType tex_node_texture;
extern bNodeType tex_node_bricks;
extern bNodeType tex_node_image;
extern bNodeType tex_node_curve_rgb;
extern bNodeType tex_node_curve_time;
extern bNodeType tex_node_invert;
extern bNodeType tex_node_hue_sat;
extern bNodeType tex_node_coord;
extern bNodeType tex_node_distance;

extern bNodeType tex_node_rotate;
extern bNodeType tex_node_translate;
extern bNodeType tex_node_scale;
extern bNodeType tex_node_at;

extern bNodeType tex_node_compose;
extern bNodeType tex_node_decompose;

extern bNodeType tex_node_proc_voronoi;
extern bNodeType tex_node_proc_blend;
extern bNodeType tex_node_proc_magic;
extern bNodeType tex_node_proc_marble;
extern bNodeType tex_node_proc_clouds;
extern bNodeType tex_node_proc_wood;
extern bNodeType tex_node_proc_musgrave;
extern bNodeType tex_node_proc_noise;
extern bNodeType tex_node_proc_stucci;
extern bNodeType tex_node_proc_distnoise;

#endif
