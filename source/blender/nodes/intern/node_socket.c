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

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_node.h"
#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "NOD_socket.h"

struct Main;

struct bNodeSocket *node_add_socket_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp, int in_out)
{
	bNodeSocket *sock = nodeAddStaticSocket(ntree, node, in_out, stemp->type, stemp->subtype, stemp->identifier, stemp->name);
	
	sock->flag |= stemp->flag;
	
	/* initialize default_value */
	switch (stemp->type) {
		case SOCK_FLOAT:
		{
			bNodeSocketValueFloat *dval = sock->default_value;
			dval->value = stemp->val1;
			dval->min = stemp->min;
			dval->max = stemp->max;
			break;
		}
		case SOCK_INT:
		{
			bNodeSocketValueInt *dval = sock->default_value;
			dval->value = (int)stemp->val1;
			dval->min = (int)stemp->min;
			dval->max = (int)stemp->max;
			break;
		}
		case SOCK_BOOLEAN:
		{
			bNodeSocketValueBoolean *dval = sock->default_value;
			dval->value = (int)stemp->val1;
			break;
		}
		case SOCK_VECTOR:
		{
			bNodeSocketValueVector *dval = sock->default_value;
			dval->value[0] = stemp->val1;
			dval->value[1] = stemp->val2;
			dval->value[2] = stemp->val3;
			dval->min = stemp->min;
			dval->max = stemp->max;
			break;
		}
		case SOCK_RGBA:
		{
			bNodeSocketValueRGBA *dval = sock->default_value;
			dval->value[0] = stemp->val1;
			dval->value[1] = stemp->val2;
			dval->value[2] = stemp->val3;
			dval->value[3] = stemp->val4;
			break;
		}
	}
	
	return sock;
}

static bNodeSocket *verify_socket_template(bNodeTree *ntree, bNode *node, int in_out, ListBase *socklist, bNodeSocketTemplate *stemp)
{
	bNodeSocket *sock;
	
	for (sock = socklist->first; sock; sock = sock->next) {
		if (strncmp(sock->name, stemp->name, NODE_MAXSTR) == 0)
			break;
	}
	if (sock) {
		sock->type = stemp->type;
		sock->limit = (stemp->limit == 0 ? 0xFFF : stemp->limit);
		sock->flag |= stemp->flag;
		
		BLI_remlink(socklist, sock);
		
		return sock;
	}
	else {
		/* no socket for this template found, make a new one */
		sock = node_add_socket_from_template(ntree, node, stemp, in_out);
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
	if (stemp_first == NULL) {
		for (sock = (bNodeSocket *)socklist->first; sock; sock = nextsock) {
			nextsock = sock->next;
			nodeRemoveSocket(ntree, node, sock);
		}
	}
	else {
		/* step by step compare */
		stemp = stemp_first;
		while (stemp->type != -1) {
			stemp->sock = verify_socket_template(ntree, node, in_out, socklist, stemp);
			stemp++;
		}
		/* leftovers are removed */
		for (sock = (bNodeSocket *)socklist->first; sock; sock = nextsock) {
			nextsock = sock->next;
			nodeRemoveSocket(ntree, node, sock);
		}
		
		/* and we put back the verified sockets */
		stemp = stemp_first;
		if (socklist->first) {
			/* some dynamic sockets left, store the list start
			 * so we can add static sockets infront of it.
			 */
			sock = socklist->first;
			while (stemp->type != -1) {
				/* put static sockets infront of dynamic */
				BLI_insertlinkbefore(socklist, sock, stemp->sock);
				stemp++;
			}
		}
		else {
			while (stemp->type != -1) {
				BLI_addtail(socklist, stemp->sock);
				stemp++;
			}
		}
	}
}

void node_verify_socket_templates(bNodeTree *ntree, bNode *node)
{
	bNodeType *ntype = node->typeinfo;
	/* Don't try to match socket lists when there are no templates.
	 * This prevents group node sockets from being removed, without the need to explicitly
	 * check the node type here.
	 */
	if (ntype) {
		if (ntype->inputs && ntype->inputs[0].type >= 0)
			verify_socket_template_list(ntree, node, SOCK_IN, &node->inputs, ntype->inputs);
		if (ntype->outputs && ntype->outputs[0].type >= 0)
			verify_socket_template_list(ntree, node, SOCK_OUT, &node->outputs, ntype->outputs);
	}
}


void node_socket_init_default_value(bNodeSocket *sock)
{
	int type = sock->typeinfo->type;
	int subtype = sock->typeinfo->subtype;
	
	if (sock->default_value)
		return; /* already initialized */
	
	switch (type) {
		case SOCK_FLOAT:
		{
			bNodeSocketValueFloat *dval = MEM_callocN(sizeof(bNodeSocketValueFloat), "node socket value float");
			dval->subtype = subtype;
			dval->value = 0.0f;
			dval->min = -FLT_MAX;
			dval->max = FLT_MAX;
		
			sock->default_value = dval;
			break;
		}
		case SOCK_INT:
		{
			bNodeSocketValueInt *dval = MEM_callocN(sizeof(bNodeSocketValueInt), "node socket value int");
			dval->subtype = subtype;
			dval->value = 0;
			dval->min = INT_MIN;
			dval->max = INT_MAX;
		
			sock->default_value = dval;
			break;
		}
		case SOCK_BOOLEAN:
		{
			bNodeSocketValueBoolean *dval = MEM_callocN(sizeof(bNodeSocketValueBoolean), "node socket value bool");
			dval->value = false;
		
			sock->default_value = dval;
			break;
		}
		case SOCK_VECTOR:
		{
			static float default_value[] = { 0.0f, 0.0f, 0.0f };
			bNodeSocketValueVector *dval = MEM_callocN(sizeof(bNodeSocketValueVector), "node socket value vector");
			dval->subtype = subtype;
			copy_v3_v3(dval->value, default_value);
			dval->min = -FLT_MAX;
			dval->max = FLT_MAX;
		
			sock->default_value = dval;
			break;
		}
		case SOCK_RGBA:
		{
			static float default_value[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			bNodeSocketValueRGBA *dval = MEM_callocN(sizeof(bNodeSocketValueRGBA), "node socket value color");
			copy_v4_v4(dval->value, default_value);
		
			sock->default_value = dval;
			break;
		}
		case SOCK_STRING:
		{
			bNodeSocketValueString *dval = MEM_callocN(sizeof(bNodeSocketValueString), "node socket value string");
			dval->subtype = subtype;
			dval->value[0] = '\0';
		
			sock->default_value = dval;
			break;
		}
	}
}

void node_socket_copy_default_value(bNodeSocket *to, bNodeSocket *from)
{
	/* sanity check */
	if (to->type != from->type)
		return;
	
	/* make sure both exist */
	if (!from->default_value)
		return;
	node_socket_init_default_value(to);
	
	switch (from->typeinfo->type) {
		case SOCK_FLOAT:
		{
			bNodeSocketValueFloat *toval = to->default_value;
			bNodeSocketValueFloat *fromval = from->default_value;
			*toval = *fromval;
			break;
		}
		case SOCK_INT:
		{
			bNodeSocketValueInt *toval = to->default_value;
			bNodeSocketValueInt *fromval = from->default_value;
			*toval = *fromval;
			break;
		}
		case SOCK_BOOLEAN:
		{
			bNodeSocketValueBoolean *toval = to->default_value;
			bNodeSocketValueBoolean *fromval = from->default_value;
			*toval = *fromval;
			break;
		}
		case SOCK_VECTOR:
		{
			bNodeSocketValueVector *toval = to->default_value;
			bNodeSocketValueVector *fromval = from->default_value;
			*toval = *fromval;
			break;
		}
		case SOCK_RGBA:
		{
			bNodeSocketValueRGBA *toval = to->default_value;
			bNodeSocketValueRGBA *fromval = from->default_value;
			*toval = *fromval;
			break;
		}
		case SOCK_STRING:
		{
			bNodeSocketValueString *toval = to->default_value;
			bNodeSocketValueString *fromval = from->default_value;
			*toval = *fromval;
			break;
		}
	}

	to->flag |= (from->flag & SOCK_HIDE_VALUE);
}

static void standard_node_socket_interface_init_socket(bNodeTree *UNUSED(ntree), bNodeSocket *stemp, bNode *UNUSED(node), bNodeSocket *sock, const char *UNUSED(data_path))
{
	/* initialize the type value */
	sock->type = sock->typeinfo->type;
	
	/* XXX socket interface 'type' value is not used really,
	 * but has to match or the copy function will bail out
	 */
	stemp->type = stemp->typeinfo->type;
	/* copy default_value settings */
	node_socket_copy_default_value(sock, stemp);
}

/* copies settings that are not changed for each socket instance */
static void standard_node_socket_interface_verify_socket(bNodeTree *UNUSED(ntree), bNodeSocket *stemp, bNode *UNUSED(node), bNodeSocket *sock, const char *UNUSED(data_path))
{
	/* sanity check */
	if (sock->type != stemp->typeinfo->type)
		return;
	
	/* make sure both exist */
	if (!stemp->default_value)
		return;
	node_socket_init_default_value(sock);
	
	switch (stemp->typeinfo->type) {
		case SOCK_FLOAT:
		{
			bNodeSocketValueFloat *toval = sock->default_value;
			bNodeSocketValueFloat *fromval = stemp->default_value;
			toval->min = fromval->min;
			toval->max = fromval->max;
			break;
		}
		case SOCK_INT:
		{
			bNodeSocketValueInt *toval = sock->default_value;
			bNodeSocketValueInt *fromval = stemp->default_value;
			toval->min = fromval->min;
			toval->max = fromval->max;
			break;
		}
		case SOCK_VECTOR:
		{
			bNodeSocketValueVector *toval = sock->default_value;
			bNodeSocketValueVector *fromval = stemp->default_value;
			toval->min = fromval->min;
			toval->max = fromval->max;
			break;
		}
	}
}

static void standard_node_socket_interface_from_socket(bNodeTree *UNUSED(ntree), bNodeSocket *stemp, bNode *UNUSED(node), bNodeSocket *sock)
{
	/* initialize settings */
	stemp->type = stemp->typeinfo->type;
	node_socket_copy_default_value(stemp, sock);
}

static bNodeSocketType *make_standard_socket_type(int type, int subtype)
{
	extern void ED_init_standard_node_socket_type(bNodeSocketType *);
	
	const char *socket_idname = nodeStaticSocketType(type, subtype);
	const char *interface_idname = nodeStaticSocketInterfaceType(type, subtype);
	bNodeSocketType *stype;
	StructRNA *srna;
	
	stype = MEM_callocN(sizeof(bNodeSocketType), "node socket C type");
	BLI_strncpy(stype->idname, socket_idname, sizeof(stype->idname));
	
	/* set the RNA type
	 * uses the exact same identifier as the socket type idname */
	srna = stype->ext_socket.srna = RNA_struct_find(socket_idname);
	BLI_assert(srna != NULL);
	/* associate the RNA type with the socket type */
	RNA_struct_blender_type_set(srna, stype);
	
	/* set the interface RNA type */
	srna = stype->ext_interface.srna = RNA_struct_find(interface_idname);
	BLI_assert(srna != NULL);
	/* associate the RNA type with the socket type */
	RNA_struct_blender_type_set(srna, stype);
	
	/* extra type info for standard socket types */
	stype->type = type;
	stype->subtype = subtype;
	
	/* XXX bad-level call! needed for setting draw callbacks */
	ED_init_standard_node_socket_type(stype);
	
	stype->interface_init_socket = standard_node_socket_interface_init_socket;
	stype->interface_from_socket = standard_node_socket_interface_from_socket;
	stype->interface_verify_socket = standard_node_socket_interface_verify_socket;
	
	return stype;
}

static bNodeSocketType *make_socket_type_virtual(void)
{
	extern void ED_init_node_socket_type_virtual(bNodeSocketType *);
	
	const char *socket_idname = "NodeSocketVirtual";
	bNodeSocketType *stype;
	StructRNA *srna;
	
	stype = MEM_callocN(sizeof(bNodeSocketType), "node socket C type");
	BLI_strncpy(stype->idname, socket_idname, sizeof(stype->idname));
	
	/* set the RNA type
	 * uses the exact same identifier as the socket type idname */
	srna = stype->ext_socket.srna = RNA_struct_find(socket_idname);
	BLI_assert(srna != NULL);
	/* associate the RNA type with the socket type */
	RNA_struct_blender_type_set(srna, stype);
	
	/* extra type info for standard socket types */
	stype->type = SOCK_CUSTOM;
	
	ED_init_node_socket_type_virtual(stype);
	
	return stype;
}


void register_standard_node_socket_types(void)
{
	/* draw callbacks are set in drawnode.c to avoid bad-level calls */
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_FLOAT, PROP_NONE));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_FLOAT, PROP_UNSIGNED));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_FLOAT, PROP_PERCENTAGE));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_FLOAT, PROP_FACTOR));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_FLOAT, PROP_ANGLE));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_FLOAT, PROP_TIME));
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_INT, PROP_NONE));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_INT, PROP_UNSIGNED));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_INT, PROP_PERCENTAGE));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_INT, PROP_FACTOR));
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_BOOLEAN, PROP_NONE));
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_NONE));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_TRANSLATION));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_DIRECTION));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_VELOCITY));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_ACCELERATION));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_EULER));
	nodeRegisterSocketType(make_standard_socket_type(SOCK_VECTOR, PROP_XYZ));
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_RGBA, PROP_NONE));
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_STRING, PROP_NONE));
	
	nodeRegisterSocketType(make_standard_socket_type(SOCK_SHADER, PROP_NONE));
	
	nodeRegisterSocketType(make_socket_type_virtual());
}
