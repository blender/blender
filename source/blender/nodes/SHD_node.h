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

extern bNodeType sh_node_output;
extern bNodeType sh_node_material;
extern bNodeType sh_node_camera;
extern bNodeType sh_node_value;
extern bNodeType sh_node_rgb;
extern bNodeType sh_node_mix_rgb;
extern bNodeType sh_node_valtorgb;
extern bNodeType sh_node_rgbtobw;
extern bNodeType sh_node_texture;
extern bNodeType sh_node_normal;
extern bNodeType sh_node_geom;
extern bNodeType sh_node_mapping;
extern bNodeType sh_node_curve_vec;
extern bNodeType sh_node_curve_rgb;
extern bNodeType sh_node_math;
extern bNodeType sh_node_vect_math;
extern bNodeType sh_node_squeeze;
extern bNodeType node_dynamic_typeinfo;
extern bNodeType sh_node_material_ext;
extern bNodeType sh_node_invert;
extern bNodeType sh_node_seprgb;
extern bNodeType sh_node_combrgb;
extern bNodeType sh_node_hue_sat;

#endif


