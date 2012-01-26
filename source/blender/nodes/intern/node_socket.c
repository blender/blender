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

#include <limits.h>

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

void *node_socket_make_default_value(int type)
{
	/* XXX currently just allocates from stype->structsize.
	 * it might become necessary to do more complex allocations for later types.
	 */
	bNodeSocketType *stype = ntreeGetSocketType(type);
	if (stype->value_structsize > 0) {
		void *default_value = MEM_callocN(stype->value_structsize, "default socket value");
		return default_value;
	}
	else
		return NULL;
}

void node_socket_free_default_value(int UNUSED(type), void *default_value)
{
	/* XXX can just free the pointee for all current socket types. */
	if (default_value)
		MEM_freeN(default_value);
}

void node_socket_init_default_value(int type, void *default_value)
{
	switch (type) {
	case SOCK_FLOAT:
		node_socket_set_default_value_float(default_value, PROP_NONE, 0.0f, -FLT_MAX, FLT_MAX);
		break;
	case SOCK_INT:
		node_socket_set_default_value_int(default_value, PROP_NONE, 0, INT_MIN, INT_MAX);
		break;
	case SOCK_BOOLEAN:
		node_socket_set_default_value_boolean(default_value, FALSE);
		break;
	case SOCK_VECTOR:
		node_socket_set_default_value_vector(default_value, PROP_NONE, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX);
		break;
	case SOCK_RGBA:
		node_socket_set_default_value_rgba(default_value, 0.0f, 0.0f, 0.0f, 1.0f);
		break;
	case SOCK_SHADER:
		node_socket_set_default_value_shader(default_value);
		break;
	case SOCK_MESH:
		node_socket_set_default_value_mesh(default_value);
		break;
	}
}

void node_socket_set_default_value_int(void *default_value, PropertySubType subtype, int value, int min, int max)
{
	bNodeSocketValueInt *val = default_value;
	val->subtype = subtype;
	val->value = value;
	val->min = min;
	val->max = max;
}

void node_socket_set_default_value_float(void *default_value, PropertySubType subtype, float value, float min, float max)
{
	bNodeSocketValueFloat *val = default_value;
	val->subtype = subtype;
	val->value = value;
	val->min = min;
	val->max = max;
}

void node_socket_set_default_value_boolean(void *default_value, char value)
{
	bNodeSocketValueBoolean *val = default_value;
	val->value = value;
}

void node_socket_set_default_value_vector(void *default_value, PropertySubType subtype, float x, float y, float z, float min, float max)
{
	bNodeSocketValueVector *val = default_value;
	val->subtype = subtype;
	val->value[0] = x;
	val->value[1] = y;
	val->value[2] = z;
	val->min = min;
	val->max = max;
}

void node_socket_set_default_value_rgba(void *default_value, float r, float g, float b, float a)
{
	bNodeSocketValueRGBA *val = default_value;
	val->value[0] = r;
	val->value[1] = g;
	val->value[2] = b;
	val->value[3] = a;
}

void node_socket_set_default_value_shader(void *UNUSED(default_value))
{
}

void node_socket_set_default_value_mesh(void *UNUSED(default_value))
{
}


void node_socket_copy_default_value(int type, void *to_default_value, void *from_default_value)
{
	/* XXX only one of these pointers is valid! just putting them here for convenience */
	bNodeSocketValueFloat *fromfloat= (bNodeSocketValueFloat*)from_default_value;
	bNodeSocketValueInt *fromint= (bNodeSocketValueInt*)from_default_value;
	bNodeSocketValueBoolean *frombool= (bNodeSocketValueBoolean*)from_default_value;
	bNodeSocketValueVector *fromvector= (bNodeSocketValueVector*)from_default_value;
	bNodeSocketValueRGBA *fromrgba= (bNodeSocketValueRGBA*)from_default_value;

	bNodeSocketValueFloat *tofloat= (bNodeSocketValueFloat*)to_default_value;
	bNodeSocketValueInt *toint= (bNodeSocketValueInt*)to_default_value;
	bNodeSocketValueBoolean *tobool= (bNodeSocketValueBoolean*)to_default_value;
	bNodeSocketValueVector *tovector= (bNodeSocketValueVector*)to_default_value;
	bNodeSocketValueRGBA *torgba= (bNodeSocketValueRGBA*)to_default_value;

	switch (type) {
	case SOCK_FLOAT:
		*tofloat = *fromfloat;
		break;
	case SOCK_INT:
		*toint = *fromint;
		break;
	case SOCK_BOOLEAN:
		*tobool = *frombool;
		break;
	case SOCK_VECTOR:
		*tovector = *fromvector;
		break;
	case SOCK_RGBA:
		*torgba = *fromrgba;
		break;
	}
}

/* XXX This is a makeshift function to have useful initial group socket values.
 * In the end this should be implemented by a flexible socket data conversion system,
 * which is yet to be implemented. The idea is that beside default standard conversions,
 * such as int-to-float, it should be possible to quickly select a conversion method or
 * a chain of conversions for each input, whenever there is more than one option.
 * E.g. a vector-to-float conversion could use either of the x/y/z components or
 * the vector length.
 *
 * In the interface this could be implemented by a pseudo-script textbox on linked inputs,
 * with quick selection from a predefined list of conversion options. Some Examples:
 * - vector component 'z' (vector->float):						"z"
 * - greyscale color (float->color):							"grey"
 * - color luminance (color->float):							"lum"
 * - matrix column 2 length (matrix->vector->float):			"col[1].len"
 * - mesh vertex coordinate 'y' (mesh->vertex->vector->float):	"vertex.co.y"
 *
 * The actual conversion is then done by a series of conversion functions,
 * which are defined in the socket type structs.
 */
void node_socket_convert_default_value(int to_type, void *to_default_value, int from_type, void *from_default_value)
{
	/* XXX only one of these pointers is valid! just putting them here for convenience */
	bNodeSocketValueFloat *fromfloat= (bNodeSocketValueFloat*)from_default_value;
	bNodeSocketValueInt *fromint= (bNodeSocketValueInt*)from_default_value;
	bNodeSocketValueBoolean *frombool= (bNodeSocketValueBoolean*)from_default_value;
	bNodeSocketValueVector *fromvector= (bNodeSocketValueVector*)from_default_value;
	bNodeSocketValueRGBA *fromrgba= (bNodeSocketValueRGBA*)from_default_value;

	bNodeSocketValueFloat *tofloat= (bNodeSocketValueFloat*)to_default_value;
	bNodeSocketValueInt *toint= (bNodeSocketValueInt*)to_default_value;
	bNodeSocketValueBoolean *tobool= (bNodeSocketValueBoolean*)to_default_value;
	bNodeSocketValueVector *tovector= (bNodeSocketValueVector*)to_default_value;
	bNodeSocketValueRGBA *torgba= (bNodeSocketValueRGBA*)to_default_value;

	switch (from_type) {
	case SOCK_FLOAT:
		switch (to_type) {
		case SOCK_FLOAT:
			tofloat->value = fromfloat->value;
			break;
		case SOCK_INT:
			toint->value = (int)fromfloat->value;
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromfloat->value > 0.0f);
			break;
		case SOCK_VECTOR:
			tovector->value[0] = tovector->value[1] = tovector->value[2] = fromfloat->value;
			break;
		case SOCK_RGBA:
			torgba->value[0] = torgba->value[1] = torgba->value[2] = torgba->value[3] = fromfloat->value;
			break;
		}
		break;
	case SOCK_INT:
		switch (to_type) {
		case SOCK_FLOAT:
			tofloat->value = (float)fromint->value;
			break;
		case SOCK_INT:
			toint->value = fromint->value;
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromint->value > 0);
			break;
		case SOCK_VECTOR:
			tovector->value[0] = tovector->value[1] = tovector->value[2] = (float)fromint->value;
			break;
		case SOCK_RGBA:
			torgba->value[0] = torgba->value[1] = torgba->value[2] = torgba->value[3] = (float)fromint->value;
			break;
		}
		break;
	case SOCK_BOOLEAN:
		switch (to_type) {
		case SOCK_FLOAT:
			tofloat->value = (float)frombool->value;
			break;
		case SOCK_INT:
			toint->value = (int)frombool->value;
			break;
		case SOCK_BOOLEAN:
			tobool->value = frombool->value;
			break;
		case SOCK_VECTOR:
			tovector->value[0] = tovector->value[1] = tovector->value[2] = (float)frombool->value;
			break;
		case SOCK_RGBA:
			torgba->value[0] = torgba->value[1] = torgba->value[2] = torgba->value[3] = (float)frombool->value;
			break;
		}
		break;
	case SOCK_VECTOR:
		switch (to_type) {
		case SOCK_FLOAT:
			tofloat->value = fromvector->value[0];
			break;
		case SOCK_INT:
			toint->value = (int)fromvector->value[0];
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromvector->value[0] > 0.0f);
			break;
		case SOCK_VECTOR:
			copy_v3_v3(tovector->value, fromvector->value);
			break;
		case SOCK_RGBA:
			copy_v3_v3(torgba->value, fromvector->value);
			torgba->value[3] = 1.0f;
			break;
		}
		break;
	case SOCK_RGBA:
		switch (to_type) {
		case SOCK_FLOAT:
			tofloat->value = fromrgba->value[0];
			break;
		case SOCK_INT:
			toint->value = (int)fromrgba->value[0];
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromrgba->value[0] > 0.0f);
			break;
		case SOCK_VECTOR:
			copy_v3_v3(tovector->value, fromrgba->value);
			break;
		case SOCK_RGBA:
			copy_v4_v4(torgba->value, fromrgba->value);
			break;
		}
		break;
	}
}


struct bNodeSocket *node_add_input_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp)
{
	bNodeSocket *sock = nodeAddSocket(ntree, node, SOCK_IN, stemp->name, stemp->type);
	sock->flag |= stemp->flag;
	
	switch (stemp->type) {
	case SOCK_INT:
		node_socket_set_default_value_int(sock->default_value, stemp->subtype, (int)stemp->val1, (int)stemp->min, (int)stemp->max);
		break;
	case SOCK_FLOAT:
		node_socket_set_default_value_float(sock->default_value, stemp->subtype, stemp->val1, stemp->min, stemp->max);
		break;
	case SOCK_BOOLEAN:
		node_socket_set_default_value_boolean(sock->default_value, (char)stemp->val1);
		break;
	case SOCK_VECTOR:
		node_socket_set_default_value_vector(sock->default_value, stemp->subtype, stemp->val1, stemp->val2, stemp->val3, stemp->min, stemp->max);
		break;
	case SOCK_RGBA:
		node_socket_set_default_value_rgba(sock->default_value, stemp->val1, stemp->val2, stemp->val3, stemp->val4);
		break;
	case SOCK_SHADER:
		node_socket_set_default_value_shader(sock->default_value);
		break;
	case SOCK_MESH:
		node_socket_set_default_value_mesh(sock->default_value);
		break;
	}
	
	return sock;
}

struct bNodeSocket *node_add_output_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp)
{
	bNodeSocket *sock = nodeAddSocket(ntree, node, SOCK_OUT, stemp->name, stemp->type);
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
