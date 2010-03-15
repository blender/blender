/**
 * $Id:
 *
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_NODE_H
#define ED_NODE_H

struct Material;
struct Scene;
struct Tex;
struct bContext;
struct bNode;
struct ID;

/* drawnode.c */
void ED_init_node_butfuncs(void);

/* node_draw.c */
void ED_node_changed_update(struct ID *id, struct bNode *node);
void ED_node_generic_update(struct Main *bmain, struct Scene *scene, struct bNodeTree *ntree, struct bNode *node);

/* node_edit.c */
void ED_node_shader_default(struct Material *ma);
void ED_node_composit_default(struct Scene *sce);
void ED_node_texture_default(struct Tex *tex);

/* node ops.c */
void ED_operatormacros_node(void);

#endif /* ED_NODE_H */

