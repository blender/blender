/*
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

/** \file NOD_shader.h
 *  \ingroup nodes
 */

#ifndef NOD_SHADER_H
#define NOD_SHADER_H

#include "BKE_node.h"

extern struct bNodeTreeType ntreeType_Shader;


/* the type definitions array */
/* ****************** types array for all shaders ****************** */

void register_node_type_sh_group(ListBase *lb);
void register_node_type_sh_forloop(ListBase *lb);
void register_node_type_sh_whileloop(ListBase *lb);

void register_node_type_sh_output(ListBase *lb);
void register_node_type_sh_material(ListBase *lb);
void register_node_type_sh_camera(ListBase *lb);
void register_node_type_sh_value(ListBase *lb);
void register_node_type_sh_rgb(ListBase *lb);
void register_node_type_sh_mix_rgb(ListBase *lb);
void register_node_type_sh_valtorgb(ListBase *lb);
void register_node_type_sh_rgbtobw(ListBase *lb);
void register_node_type_sh_texture(ListBase *lb);
void register_node_type_sh_normal(ListBase *lb);
void register_node_type_sh_geom(ListBase *lb);
void register_node_type_sh_mapping(ListBase *lb);
void register_node_type_sh_curve_vec(ListBase *lb);
void register_node_type_sh_curve_rgb(ListBase *lb);
void register_node_type_sh_math(ListBase *lb);
void register_node_type_sh_vect_math(ListBase *lb);
void register_node_type_sh_squeeze(ListBase *lb);
void register_node_type_sh_dynamic(ListBase *lb);
void register_node_type_sh_material_ext(ListBase *lb);
void register_node_type_sh_invert(ListBase *lb);
void register_node_type_sh_seprgb(ListBase *lb);
void register_node_type_sh_combrgb(ListBase *lb);
void register_node_type_sh_hue_sat(ListBase *lb);

#endif


