/*
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file NOD_socket.h
 *  \ingroup nodes
 */


#ifndef NOD_SOCKET_H_
#define NOD_SOCKET_H_

#include "DNA_listBase.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "RNA_types.h"

struct bNodeTree;
struct bNode;
struct bNodeStack;

void node_socket_type_init(struct bNodeSocketType *types[]);

void *node_socket_make_default_value(int type);
void node_socket_free_default_value(int type, void *default_value);
void node_socket_init_default_value(int type, void *default_value);
void node_socket_copy_default_value(int type, void *to_default_value, void *from_default_value);
void node_socket_convert_default_value(int to_type, void *to_default_value, int from_type, void *from_default_value);

void node_socket_set_default_value_int(void *default_value, PropertySubType subtype, int value, int min, int max);
void node_socket_set_default_value_float(void *default_value, PropertySubType subtype, float value, float min, float max);
void node_socket_set_default_value_boolean(void *default_value, char value);
void node_socket_set_default_value_vector(void *default_value, PropertySubType subtype, float x, float y, float z, float min, float max);
void node_socket_set_default_value_rgba(void *default_value, float r, float g, float b, float a);
void node_socket_set_default_value_shader(void *default_value);
void node_socket_set_default_value_mesh(void *default_value);

struct bNodeSocket *node_add_input_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp);
struct bNodeSocket *node_add_output_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp);

void node_verify_socket_templates(struct bNodeTree *ntree, struct bNode *node);


/* Socket Converters */

#define SOCK_VECTOR_X			1
#define SOCK_VECTOR_Y			2
#define SOCK_VECTOR_Z			3

#define SOCK_RGBA_R			1
#define SOCK_RGBA_G			2
#define SOCK_RGBA_B			3
#define SOCK_RGBA_A			4

#define SOCK_MESH_VERT_CO	1
#define SOCK_MESH_VERT_NO	2

#endif
