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

#ifndef SHD_NODE_H
#define SHD_NODE_H

#include "BKE_node.h"


/* the type definitions array */
/* ****************** types array for all shaders ****************** */

bNodeType sh_node_output;
bNodeType sh_node_material;
bNodeType sh_node_camera;
bNodeType sh_node_value;
bNodeType sh_node_rgb;
bNodeType sh_node_mix_rgb;
bNodeType sh_node_valtorgb;
bNodeType sh_node_rgbtobw;
bNodeType sh_node_texture;
bNodeType sh_node_normal;
bNodeType sh_node_geom;
bNodeType sh_node_mapping;
bNodeType sh_node_curve_vec;
bNodeType sh_node_curve_rgb;
bNodeType sh_node_math;
bNodeType sh_node_vect_math;
bNodeType sh_node_squeeze;
bNodeType node_dynamic_typeinfo;
bNodeType sh_node_material_ext;
bNodeType sh_node_invert;
bNodeType sh_node_seprgb;
bNodeType sh_node_combrgb;
bNodeType sh_node_hue_sat;

#endif


