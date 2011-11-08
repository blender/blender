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
 * Contributor(s): Lukas Toennne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/intern/node_socket.c
 *  \ingroup nodes
 */


#include "DNA_node_types.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_node.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "NOD_socket.h"

/****************** FLOAT ******************/

static bNodeSocketType node_socket_type_float = {
	/* type */				SOCK_FLOAT,
	/* ui_name */			"Float",
	/* ui_description */	"Floating Point",
	/* ui_icon */			0,
	/* ui_color */			{160,160,160,255},

	/* value_structname */	"bNodeSocketValueFloat",
	/* value_structsize */	sizeof(bNodeSocketValueFloat),

	/* buttonfunc */		NULL,
};

/****************** VECTOR ******************/

static bNodeSocketType node_socket_type_vector = {
	/* type */				SOCK_VECTOR,
	/* ui_name */			"Vector",
	/* ui_description */	"3-dimensional floating point vector",
	/* ui_icon */			0,
	/* ui_color */			{100,100,200,255},

	/* value_structname */	"bNodeSocketValueVector",
	/* value_structsize */	sizeof(bNodeSocketValueVector),

	/* buttonfunc */		NULL,
};

/****************** RGBA ******************/

static bNodeSocketType node_socket_type_rgba = {
	/* type */				SOCK_RGBA,
	/* ui_name */			"RGBA",
	/* ui_description */	"RGBA color",
	/* ui_icon */			0,
	/* ui_color */			{200,200,40,255},

	/* value_structname */	"bNodeSocketValueRGBA",
	/* value_structsize */	sizeof(bNodeSocketValueRGBA),

	/* buttonfunc */		NULL,
};

/****************** INT ******************/

static bNodeSocketType node_socket_type_int = {
	/* type */				SOCK_INT,
	/* ui_name */			"Int",
	/* ui_description */	"Integer",
	/* ui_icon */			0,
	/* ui_color */			{17,133,37,255},

	/* value_structname */	"bNodeSocketValueInt",
	/* value_structsize */	sizeof(bNodeSocketValueInt),

	/* buttonfunc */		NULL,
};

/****************** BOOLEAN ******************/

static bNodeSocketType node_socket_type_boolean = {
	/* type */				SOCK_BOOLEAN,
	/* ui_name */			"Boolean",
	/* ui_description */	"Boolean",
	/* ui_icon */			0,
	/* ui_color */			{158,139,63,255},

	/* value_structname */	"bNodeSocketValueBoolean",
	/* value_structsize */	sizeof(bNodeSocketValueBoolean),

	/* buttonfunc */		NULL,
};

/****************** SHADER ******************/

static bNodeSocketType node_socket_type_shader = {
	/* type */				SOCK_SHADER,
	/* ui_name */			"Shader",
	/* ui_description */	"Shader",
	/* ui_icon */			0,
	/* ui_color */			{100,200,100,255},

	/* value_structname */	NULL,
	/* value_structsize */	0,

	/* buttonfunc */		NULL,
};

/****************** MESH ******************/

static bNodeSocketType node_socket_type_mesh = {
	/* type */				SOCK_MESH,
	/* ui_name */			"Mesh",
	/* ui_description */	"Mesh geometry data",
	/* ui_icon */			0,
	/* ui_color */			{255,133,7,255},

	/* value_structname */	NULL,
	/* value_structsize */	0,

	/* buttonfunc */		NULL,
};


void node_socket_type_init(bNodeSocketType *types[])
{
	#define INIT_TYPE(name)		types[node_socket_type_##name.type] = &node_socket_type_##name;
	
	INIT_TYPE(float);
	INIT_TYPE(vector);
	INIT_TYPE(rgba);
	INIT_TYPE(int);
	INIT_TYPE(boolean);
	INIT_TYPE(shader);
	INIT_TYPE(mesh);
	
	#undef INIT_TYPE
}

struct bNodeSocket *nodeAddInputInt(struct bNodeTree *ntree, struct bNode *node, const char *name, PropertySubType subtype,
									int value, int min, int max)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_INT);
	bNodeSocketValueInt *dval= (bNodeSocketValueInt*)sock->default_value;
	dval->subtype = subtype;
	dval->value = value;
	dval->min = min;
	dval->max = max;
	return sock;
}
struct bNodeSocket *nodeAddOutputInt(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_INT);
	return sock;
}

struct bNodeSocket *nodeAddInputFloat(struct bNodeTree *ntree, struct bNode *node, const char *name, PropertySubType subtype,
									  float value, float min, float max)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_FLOAT);
	bNodeSocketValueFloat *dval= (bNodeSocketValueFloat*)sock->default_value;
	dval->subtype = subtype;
	dval->value = value;
	dval->min = min;
	dval->max = max;
	return sock;
}
struct bNodeSocket *nodeAddOutputFloat(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_FLOAT);
	return sock;
}

struct bNodeSocket *nodeAddInputBoolean(struct bNodeTree *ntree, struct bNode *node, const char *name, char value)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_BOOLEAN);
	bNodeSocketValueBoolean *dval= (bNodeSocketValueBoolean*)sock->default_value;
	dval->value = value;
	return sock;
}
struct bNodeSocket *nodeAddOutputBoolean(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_BOOLEAN);
	return sock;
}

struct bNodeSocket *nodeAddInputVector(struct bNodeTree *ntree, struct bNode *node, const char *name, PropertySubType subtype,
									   float x, float y, float z, float min, float max)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_VECTOR);
	bNodeSocketValueVector *dval= (bNodeSocketValueVector*)sock->default_value;
	dval->subtype = subtype;
	dval->value[0] = x;
	dval->value[1] = y;
	dval->value[2] = z;
	dval->min = min;
	dval->max = max;
	return sock;
}
struct bNodeSocket *nodeAddOutputVector(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_VECTOR);
	return sock;
}

struct bNodeSocket *nodeAddInputRGBA(struct bNodeTree *ntree, struct bNode *node, const char *name,
									 float r, float g, float b, float a)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_RGBA);
	bNodeSocketValueRGBA *dval= (bNodeSocketValueRGBA*)sock->default_value;
	dval->value[0] = r;
	dval->value[1] = g;
	dval->value[2] = b;
	dval->value[3] = a;
	return sock;
}
struct bNodeSocket *nodeAddOutputRGBA(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_RGBA);
	return sock;
}

struct bNodeSocket *nodeAddInputShader(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_SHADER);
	return sock;
}
struct bNodeSocket *nodeAddOutputShader(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_SHADER);
	return sock;
}

struct bNodeSocket *nodeAddInputMesh(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_MESH);
	return sock;
}
struct bNodeSocket *nodeAddOutputMesh(struct bNodeTree *ntree, struct bNode *node, const char *name)
{
	bNodeSocket *sock= nodeAddSocket(ntree, node, SOCK_OUT, name, SOCK_MESH);
	return sock;
}

struct bNodeSocket *node_add_input_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp)
{
	bNodeSocket *sock;
	switch (stemp->type) {
	case SOCK_INT:
		sock = nodeAddInputInt(ntree, node, stemp->name, stemp->subtype, (int)stemp->val1, (int)stemp->min, (int)stemp->max);
		break;
	case SOCK_FLOAT:
		sock = nodeAddInputFloat(ntree, node, stemp->name, stemp->subtype, stemp->val1, stemp->min, stemp->max);
		break;
	case SOCK_BOOLEAN:
		sock = nodeAddInputBoolean(ntree, node, stemp->name, (char)stemp->val1);
		break;
	case SOCK_VECTOR:
		sock = nodeAddInputVector(ntree, node, stemp->name, stemp->subtype, stemp->val1, stemp->val2, stemp->val3, stemp->min, stemp->max);
		break;
	case SOCK_RGBA:
		sock = nodeAddInputRGBA(ntree, node, stemp->name, stemp->val1, stemp->val2, stemp->val3, stemp->val4);
		break;
	case SOCK_SHADER:
		sock = nodeAddInputShader(ntree, node, stemp->name);
		break;
	case SOCK_MESH:
		sock = nodeAddInputMesh(ntree, node, stemp->name);
		break;
	default:
		sock = nodeAddSocket(ntree, node, SOCK_IN, stemp->name, stemp->type);
	}
	sock->flag |= stemp->flag;
	return sock;
}

struct bNodeSocket *node_add_output_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp)
{
	bNodeSocket *sock;
	switch (stemp->type) {
	case SOCK_INT:
		sock = nodeAddOutputInt(ntree, node, stemp->name);
		break;
	case SOCK_FLOAT:
		sock = nodeAddOutputFloat(ntree, node, stemp->name);
		break;
	case SOCK_BOOLEAN:
		sock = nodeAddOutputBoolean(ntree, node, stemp->name);
		break;
	case SOCK_VECTOR:
		sock = nodeAddOutputVector(ntree, node, stemp->name);
		break;
	case SOCK_RGBA:
		sock = nodeAddOutputRGBA(ntree, node, stemp->name);
		break;
	case SOCK_SHADER:
		sock = nodeAddOutputShader(ntree, node, stemp->name);
		break;
	case SOCK_MESH:
		sock = nodeAddOutputMesh(ntree, node, stemp->name);
		break;
	default:
		sock = nodeAddSocket(ntree, node, SOCK_OUT, stemp->name, stemp->type);
	}
	return sock;
}

static bNodeSocket *verify_socket_template(bNodeTree *ntree, bNode *node, int in_out, ListBase *socklist, bNodeSocketTemplate *stemp)
{
	bNodeSocket *sock;
	
	for(sock= socklist->first; sock; sock= sock->next) {
		if(!(sock->flag & SOCK_DYNAMIC) && strncmp(sock->name, stemp->name, NODE_MAXSTR)==0)
			break;
	}
	if(sock) {
		sock->type= stemp->type;		/* in future, read this from tydefs! */
		if(stemp->limit==0) sock->limit= 0xFFF;
		else sock->limit= stemp->limit;
		sock->flag |= stemp->flag;
		
		/* Copy the property range and subtype parameters in case the template changed.
		 * NOT copying the actual value here, only button behavior changes!
		 */
		switch (sock->type) {
		case SOCK_FLOAT:
			{
				bNodeSocketValueFloat *dval= sock->default_value;
				dval->min = stemp->min;
				dval->max = stemp->max;
				dval->subtype = stemp->subtype;
			}
			break;
		case SOCK_INT:
			{
				bNodeSocketValueInt *dval= sock->default_value;
				dval->min = stemp->min;
				dval->max = stemp->max;
				dval->subtype = stemp->subtype;
			}
			break;
		case SOCK_VECTOR:
			{
				bNodeSocketValueVector *dval= sock->default_value;
				dval->min = stemp->min;
				dval->max = stemp->max;
				dval->subtype = stemp->subtype;
			}
			break;
		}
		
		BLI_remlink(socklist, sock);
		
		return sock;
	}
	else {
		/* no socket for this template found, make a new one */
		if (in_out==SOCK_IN)
			sock = node_add_input_from_template(ntree, node, stemp);
		else
			sock = node_add_output_from_template(ntree, node, stemp);
		/* remove the new socket from the node socket list first,
		 * will be added back after verification.
		 */
		BLI_remlink(socklist, sock);
	}
	
	return sock;
}

static void verify_socket_template_list(bNodeTree *ntree, bNode *node, int in_out, ListBase *socklist, bNodeSocketTemplate *stemp_first)
{
	bNodeSocket *sock, *nextsock;
	bNodeSocketTemplate *stemp;
	
	/* no inputs anymore? */
	if(stemp_first==NULL) {
		for (sock = (bNodeSocket*)socklist->first; sock; sock=nextsock) {
			nextsock = sock->next;
			if (!(sock->flag & SOCK_DYNAMIC))
				nodeRemoveSocket(ntree, node, sock);
		}
	}
	else {
		/* step by step compare */
		stemp= stemp_first;
		while(stemp->type != -1) {
			stemp->sock= verify_socket_template(ntree, node, in_out, socklist, stemp);
			stemp++;
		}
		/* leftovers are removed */
		for (sock = (bNodeSocket*)socklist->first; sock; sock=nextsock) {
			nextsock = sock->next;
			if (!(sock->flag & SOCK_DYNAMIC))
				nodeRemoveSocket(ntree, node, sock);
		}
		
		/* and we put back the verified sockets */
		stemp= stemp_first;
		if (socklist->first) {
			/* some dynamic sockets left, store the list start
			 * so we can add static sockets infront of it.
			 */
			sock = socklist->first;
			while(stemp->type != -1) {
				/* put static sockets infront of dynamic */
				BLI_insertlinkbefore(socklist, sock, stemp->sock);
				stemp++;
			}
		}
		else {
			while(stemp->type != -1) {
				BLI_addtail(socklist, stemp->sock);
				stemp++;
			}
		}
	}
}

void node_verify_socket_templates(bNodeTree *ntree, bNode *node)
{
	bNodeType *ntype= node->typeinfo;
	/* XXX Small trick: don't try to match socket lists when there are no templates.
	 * This also prevents group node sockets from being removed, without the need to explicitly
	 * check the node type here.
	 */
	if(ntype && ((ntype->inputs && ntype->inputs[0].type>=0) || (ntype->outputs && ntype->outputs[0].type>=0))) {
		verify_socket_template_list(ntree, node, SOCK_IN, &node->inputs, ntype->inputs);
		verify_socket_template_list(ntree, node, SOCK_OUT, &node->outputs, ntype->outputs);
	}
}
