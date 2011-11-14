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

struct bNodeSocket *nodeAddInputInt(struct bNodeTree *ntree, struct bNode *node, const char *name, PropertySubType subtype, int value, int min, int max);
struct bNodeSocket *nodeAddOutputInt(struct bNodeTree *ntree, struct bNode *node, const char *name);

struct bNodeSocket *nodeAddInputFloat(struct bNodeTree *ntree, struct bNode *node, const char *name, PropertySubType subtype, float value, float min, float max);
struct bNodeSocket *nodeAddOutputFloat(struct bNodeTree *ntree, struct bNode *node, const char *name);

struct bNodeSocket *nodeAddInputBoolean(struct bNodeTree *ntree, struct bNode *node, const char *name, char value);
struct bNodeSocket *nodeAddOutputBoolean(struct bNodeTree *ntree, struct bNode *node, const char *name);

struct bNodeSocket *nodeAddInputVector(struct bNodeTree *ntree, struct bNode *node, const char *name, PropertySubType subtype, float x, float y, float z, float min, float max);
struct bNodeSocket *nodeAddOutputVector(struct bNodeTree *ntree, struct bNode *node, const char *name);

struct bNodeSocket *nodeAddInputRGBA(struct bNodeTree *ntree, struct bNode *node, const char *name, float r, float g, float b, float a);
struct bNodeSocket *nodeAddOutputRGBA(struct bNodeTree *ntree, struct bNode *node, const char *name);

struct bNodeSocket *nodeAddInputShader(struct bNodeTree *ntree, struct bNode *node, const char *name);
struct bNodeSocket *nodeAddOutputShader(struct bNodeTree *ntree, struct bNode *node, const char *name);

struct bNodeSocket *nodeAddInputMesh(struct bNodeTree *ntree, struct bNode *node, const char *name);
struct bNodeSocket *nodeAddOutputMesh(struct bNodeTree *ntree, struct bNode *node, const char *name);

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
