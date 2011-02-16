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

void register_node_type_tex_math(ListBase *lb);
void register_node_type_tex_mix_rgb(ListBase *lb);
void register_node_type_tex_valtorgb(ListBase *lb);
void register_node_type_tex_valtonor(ListBase *lb);
void register_node_type_tex_rgbtobw(ListBase *lb);
void register_node_type_tex_output(ListBase *lb);
void register_node_type_tex_viewer(ListBase *lb);
void register_node_type_tex_checker(ListBase *lb);
void register_node_type_tex_texture(ListBase *lb);
void register_node_type_tex_bricks(ListBase *lb);
void register_node_type_tex_image(ListBase *lb);
void register_node_type_tex_curve_rgb(ListBase *lb);
void register_node_type_tex_curve_time(ListBase *lb);
void register_node_type_tex_invert(ListBase *lb);
void register_node_type_tex_hue_sat(ListBase *lb);
void register_node_type_tex_coord(ListBase *lb);
void register_node_type_tex_distance(ListBase *lb);

void register_node_type_tex_rotate(ListBase *lb);
void register_node_type_tex_translate(ListBase *lb);
void register_node_type_tex_scale(ListBase *lb);
void register_node_type_tex_at(ListBase *lb);

void register_node_type_tex_compose(ListBase *lb);
void register_node_type_tex_decompose(ListBase *lb);

void register_node_type_tex_proc_voronoi(ListBase *lb);
void register_node_type_tex_proc_blend(ListBase *lb);
void register_node_type_tex_proc_magic(ListBase *lb);
void register_node_type_tex_proc_marble(ListBase *lb);
void register_node_type_tex_proc_clouds(ListBase *lb);
void register_node_type_tex_proc_wood(ListBase *lb);
void register_node_type_tex_proc_musgrave(ListBase *lb);
void register_node_type_tex_proc_noise(ListBase *lb);
void register_node_type_tex_proc_stucci(ListBase *lb);
void register_node_type_tex_proc_distnoise(ListBase *lb);

#endif
