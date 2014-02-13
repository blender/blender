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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Bob Holcomb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/node.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_string.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLI_ghash.h"
#include "RNA_access.h"
#include "RNA_define.h"

#include "NOD_socket.h"
#include "NOD_common.h"
#include "NOD_composite.h"
#include "NOD_shader.h"
#include "NOD_texture.h"

/* Fallback types for undefined tree, nodes, sockets */
bNodeTreeType NodeTreeTypeUndefined;
bNodeType NodeTypeUndefined;
bNodeSocketType NodeSocketTypeUndefined;


static void node_add_sockets_from_type(bNodeTree *ntree, bNode *node, bNodeType *ntype)
{
	bNodeSocketTemplate *sockdef;
	/* bNodeSocket *sock; */ /* UNUSED */

	if (ntype->inputs) {
		sockdef = ntype->inputs;
		while (sockdef->type != -1) {
			/* sock = */ node_add_socket_from_template(ntree, node, sockdef, SOCK_IN);
			
			sockdef++;
		}
	}
	if (ntype->outputs) {
		sockdef = ntype->outputs;
		while (sockdef->type != -1) {
			/* sock = */ node_add_socket_from_template(ntree, node, sockdef, SOCK_OUT);
			
			sockdef++;
		}
	}
}

/* Note: This function is called to initialize node data based on the type.
 * The bNodeType may not be registered at creation time of the node,
 * so this can be delayed until the node type gets registered.
 */
static void node_init(const struct bContext *C, bNodeTree *ntree, bNode *node)
{
	bNodeType *ntype = node->typeinfo;
	if (ntype == &NodeTypeUndefined)
		return;
	
	/* only do this once */
	if (node->flag & NODE_INIT)
		return;
	
	node->flag = NODE_SELECT | NODE_OPTIONS | ntype->flag;
	node->width = ntype->width;
	node->miniwidth = 42.0f;
	node->height = ntype->height;
	node->color[0] = node->color[1] = node->color[2] = 0.608;   /* default theme color */
	/* initialize the node name with the node label.
	 * note: do this after the initfunc so nodes get their data set which may be used in naming
	 * (node groups for example) */
	/* XXX Do not use nodeLabel() here, it returns translated content for UI, which should *only* be used
	 *     in UI, *never* in data... Data have their own translation option!
	 *     This solution may be a bit rougher than nodeLabel()'s returned string, but it's simpler
	 *     than adding "do_translate" flags to this func (and labelfunc() as well). */
	BLI_strncpy(node->name, DATA_(ntype->ui_name), NODE_MAXSTR);
	nodeUniqueName(ntree, node);
	
	node_add_sockets_from_type(ntree, node, ntype);

	if (ntype->initfunc != NULL)
		ntype->initfunc(ntree, node);

	if (ntree->typeinfo->node_add_init != NULL)
		ntree->typeinfo->node_add_init(ntree, node);

	/* extra init callback */
	if (ntype->initfunc_api) {
		PointerRNA ptr;
		RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);
		
		/* XXX Warning: context can be NULL in case nodes are added in do_versions.
		 * Delayed init is not supported for nodes with context-based initfunc_api atm.
		 */
		BLI_assert(C != NULL);
		ntype->initfunc_api(C, &ptr);
	}
	
	if (node->id)
		id_us_plus(node->id);
	
	node->flag |= NODE_INIT;
}

static void ntree_set_typeinfo(bNodeTree *ntree, bNodeTreeType *typeinfo)
{
	if (typeinfo) {
		ntree->typeinfo = typeinfo;
		
		/* deprecated integer type */
		ntree->type = typeinfo->type;
	}
	else {
		ntree->typeinfo = &NodeTreeTypeUndefined;
		
		ntree->init &= ~NTREE_TYPE_INIT;
	}
}

static void node_set_typeinfo(const struct bContext *C, bNodeTree *ntree, bNode *node, bNodeType *typeinfo)
{
	if (typeinfo) {
		node->typeinfo = typeinfo;
		
		/* deprecated integer type */
		node->type = typeinfo->type;
		
		/* initialize the node if necessary */
		node_init(C, ntree, node);
	}
	else {
		node->typeinfo = &NodeTypeUndefined;
		
		ntree->init &= ~NTREE_TYPE_INIT;
	}
}

static void node_socket_set_typeinfo(bNodeTree *ntree, bNodeSocket *sock, bNodeSocketType *typeinfo)
{
	if (typeinfo) {
		sock->typeinfo = typeinfo;
		
		/* deprecated integer type */
		sock->type = typeinfo->type;
		
		if (sock->default_value == NULL) {
			/* initialize the default_value pointer used by standard socket types */
			node_socket_init_default_value(sock);
		}
	}
	else {
		sock->typeinfo = &NodeSocketTypeUndefined;
		
		ntree->init &= ~NTREE_TYPE_INIT;
	}
}

/* Set specific typeinfo pointers in all node trees on register/unregister */
static void update_typeinfo(Main *bmain, const struct bContext *C, bNodeTreeType *treetype, bNodeType *nodetype, bNodeSocketType *socktype, bool unregister)
{
	if (!bmain)
		return;
	
	FOREACH_NODETREE(bmain, ntree, id) {
		bNode *node;
		bNodeSocket *sock;
		
		ntree->init |= NTREE_TYPE_INIT;
		
		if (treetype && STREQ(ntree->idname, treetype->idname))
			ntree_set_typeinfo(ntree, unregister ? NULL : treetype);
		
		/* initialize nodes */
		for (node = ntree->nodes.first; node; node = node->next) {
			if (nodetype && STREQ(node->idname, nodetype->idname))
				node_set_typeinfo(C, ntree, node, unregister ? NULL : nodetype);
			
			/* initialize node sockets */
			for (sock = node->inputs.first; sock; sock = sock->next)
				if (socktype && STREQ(sock->idname, socktype->idname))
					node_socket_set_typeinfo(ntree, sock, unregister ? NULL : socktype);
			for (sock = node->outputs.first; sock; sock = sock->next)
				if (socktype && STREQ(sock->idname, socktype->idname))
					node_socket_set_typeinfo(ntree, sock, unregister ? NULL : socktype);
		}
		
		/* initialize tree sockets */
		for (sock = ntree->inputs.first; sock; sock = sock->next)
			if (socktype && STREQ(sock->idname, socktype->idname))
				node_socket_set_typeinfo(ntree, sock, unregister ? NULL : socktype);
		for (sock = ntree->outputs.first; sock; sock = sock->next)
			if (socktype && STREQ(sock->idname, socktype->idname))
				node_socket_set_typeinfo(ntree, sock, unregister ? NULL : socktype);
	}
	FOREACH_NODETREE_END
}

/* Try to initialize all typeinfo in a node tree.
 * NB: In general undefined typeinfo is a perfectly valid case, the type may just be registered later.
 * In that case the update_typeinfo function will set typeinfo on registration
 * and do necessary updates.
 */
void ntreeSetTypes(const struct bContext *C, bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	ntree->init |= NTREE_TYPE_INIT;
	
	ntree_set_typeinfo(ntree, ntreeTypeFind(ntree->idname));
	
	for (node = ntree->nodes.first; node; node = node->next) {
		node_set_typeinfo(C, ntree, node, nodeTypeFind(node->idname));
		
		for (sock = node->inputs.first; sock; sock = sock->next)
			node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
		for (sock = node->outputs.first; sock; sock = sock->next)
			node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
	}
	
	for (sock = ntree->inputs.first; sock; sock = sock->next)
		node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
	for (sock = ntree->outputs.first; sock; sock = sock->next)
		node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
}


static GHash *nodetreetypes_hash = NULL;
static GHash *nodetypes_hash = NULL;
static GHash *nodesockettypes_hash = NULL;

bNodeTreeType *ntreeTypeFind(const char *idname)
{
	bNodeTreeType *nt;

	if (idname[0]) {
		nt = BLI_ghash_lookup(nodetreetypes_hash, idname);
		if (nt)
			return nt;
	}

	return NULL;
}

void ntreeTypeAdd(bNodeTreeType *nt)
{
	BLI_ghash_insert(nodetreetypes_hash, (void *)nt->idname, nt);
	/* XXX pass Main to register function? */
	update_typeinfo(G.main, NULL, nt, NULL, NULL, false);
}

/* callback for hash value free function */
static void ntree_free_type(void *treetype_v)
{
	bNodeTreeType *treetype = treetype_v;
	/* XXX pass Main to unregister function? */
	update_typeinfo(G.main, NULL, treetype, NULL, NULL, true);
	MEM_freeN(treetype);
}

void ntreeTypeFreeLink(bNodeTreeType *nt)
{
	BLI_ghash_remove(nodetreetypes_hash, nt->idname, NULL, ntree_free_type);
}

bool ntreeIsRegistered(bNodeTree *ntree)
{
	return (ntree->typeinfo != &NodeTreeTypeUndefined);
}

GHashIterator *ntreeTypeGetIterator(void)
{
	return BLI_ghashIterator_new(nodetreetypes_hash);
}

bNodeType *nodeTypeFind(const char *idname)
{
	bNodeType *nt;

	if (idname[0]) {
		nt = BLI_ghash_lookup(nodetypes_hash, idname);
		if (nt)
			return nt;
	}

	return NULL;
}

static void free_dynamic_typeinfo(bNodeType *ntype)
{
	if (ntype->type == NODE_DYNAMIC) {
		if (ntype->inputs) {
			MEM_freeN(ntype->inputs);
		}
		if (ntype->outputs) {
			MEM_freeN(ntype->outputs);
		}
	}
}

/* callback for hash value free function */
static void node_free_type(void *nodetype_v)
{
	bNodeType *nodetype = nodetype_v;
	/* XXX pass Main to unregister function? */
	update_typeinfo(G.main, NULL, NULL, nodetype, NULL, true);
	
	/* XXX deprecated */
	if (nodetype->type == NODE_DYNAMIC)
		free_dynamic_typeinfo(nodetype);
	
	if (nodetype->needs_free)
		MEM_freeN(nodetype);
}

void nodeRegisterType(bNodeType *nt)
{
	/* debug only: basic verification of registered types */
	BLI_assert(nt->idname[0] != '\0');
	BLI_assert(nt->poll != NULL);
	
	BLI_ghash_insert(nodetypes_hash, (void *)nt->idname, nt);
	/* XXX pass Main to register function? */
	update_typeinfo(G.main, NULL, NULL, nt, NULL, false);
}

void nodeUnregisterType(bNodeType *nt)
{
	BLI_ghash_remove(nodetypes_hash, nt->idname, NULL, node_free_type);
}

bool nodeIsRegistered(bNode *node)
{
	return (node->typeinfo != &NodeTypeUndefined);
}

GHashIterator *nodeTypeGetIterator(void)
{
	return BLI_ghashIterator_new(nodetypes_hash);
}

bNodeSocketType *nodeSocketTypeFind(const char *idname)
{
	bNodeSocketType *st;

	if (idname[0]) {
		st = BLI_ghash_lookup(nodesockettypes_hash, idname);
		if (st)
			return st;
	}

	return NULL;
}

/* callback for hash value free function */
static void node_free_socket_type(void *socktype_v)
{
	bNodeSocketType *socktype = socktype_v;
	/* XXX pass Main to unregister function? */
	update_typeinfo(G.main, NULL, NULL, NULL, socktype, true);
	
	MEM_freeN(socktype);
}

void nodeRegisterSocketType(bNodeSocketType *st)
{
	BLI_ghash_insert(nodesockettypes_hash, (void *)st->idname, st);
	/* XXX pass Main to register function? */
	update_typeinfo(G.main, NULL, NULL, NULL, st, false);
}

void nodeUnregisterSocketType(bNodeSocketType *st)
{
	BLI_ghash_remove(nodesockettypes_hash, st->idname, NULL, node_free_socket_type);
}

bool nodeSocketIsRegistered(bNodeSocket *sock)
{
	return (sock->typeinfo != &NodeSocketTypeUndefined);
}

GHashIterator *nodeSocketTypeGetIterator(void)
{
	return BLI_ghashIterator_new(nodesockettypes_hash);
}

struct bNodeSocket *nodeFindSocket(bNode *node, int in_out, const char *identifier)
{
	bNodeSocket *sock = (in_out == SOCK_IN ? node->inputs.first : node->outputs.first);
	for (; sock; sock = sock->next) {
		if (STREQ(sock->identifier, identifier))
			return sock;
	}
	return NULL;
}

/* find unique socket identifier */
static bool unique_identifier_check(void *arg, const char *identifier)
{
	struct ListBase *lb = arg;
	bNodeSocket *sock;
	for (sock = lb->first; sock; sock = sock->next) {
		if (STREQ(sock->identifier, identifier))
			return true;
	}
	return false;
}

static bNodeSocket *make_socket(bNodeTree *ntree, bNode *UNUSED(node), int in_out, ListBase *lb,
                                const char *idname, const char *identifier, const char *name)
{
	bNodeSocket *sock;
	char auto_identifier[MAX_NAME];
	
	if (identifier && identifier[0] != '\0') {
		/* use explicit identifier */
		BLI_strncpy(auto_identifier, identifier, sizeof(auto_identifier));
	}
	else {
		/* if no explicit identifier is given, assign a unique identifier based on the name */
		BLI_strncpy(auto_identifier, name, sizeof(auto_identifier));
	}
	/* make the identifier unique */
	BLI_uniquename_cb(unique_identifier_check, lb, "socket", '.', auto_identifier, sizeof(auto_identifier));
	
	sock = MEM_callocN(sizeof(bNodeSocket), "sock");
	sock->in_out = in_out;
	
	BLI_strncpy(sock->identifier, auto_identifier, NODE_MAXSTR);
	sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);
	
	BLI_strncpy(sock->name, name, NODE_MAXSTR);
	sock->storage = NULL;
	sock->flag |= SOCK_COLLAPSED;
	sock->type = SOCK_CUSTOM;	/* int type undefined by default */
	
	BLI_strncpy(sock->idname, idname, sizeof(sock->idname));
	node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(idname));
	
	return sock;
}

bNodeSocket *nodeAddSocket(bNodeTree *ntree, bNode *node, int in_out, const char *idname,
                           const char *identifier, const char *name)
{
	ListBase *lb = (in_out == SOCK_IN ? &node->inputs : &node->outputs);
	bNodeSocket *sock = make_socket(ntree, node, in_out, lb, idname, identifier, name);
	
	BLI_remlink(lb, sock);	/* does nothing for new socket */
	BLI_addtail(lb, sock);
	
	node->update |= NODE_UPDATE;
	
	return sock;
}

bNodeSocket *nodeInsertSocket(bNodeTree *ntree, bNode *node, int in_out, const char *idname,
                              bNodeSocket *next_sock, const char *identifier, const char *name)
{
	ListBase *lb = (in_out == SOCK_IN ? &node->inputs : &node->outputs);
	bNodeSocket *sock = make_socket(ntree, node, in_out, lb, idname, identifier, name);
	
	BLI_remlink(lb, sock);	/* does nothing for new socket */
	BLI_insertlinkbefore(lb, next_sock, sock);
	
	node->update |= NODE_UPDATE;
	
	return sock;
}

const char *nodeStaticSocketType(int type, int subtype)
{
	switch (type) {
		case SOCK_FLOAT:
			switch (subtype) {
				case PROP_UNSIGNED:
					return "NodeSocketFloatUnsigned";
				case PROP_PERCENTAGE:
					return "NodeSocketFloatPercentage";
				case PROP_FACTOR:
					return "NodeSocketFloatFactor";
				case PROP_ANGLE:
					return "NodeSocketFloatAngle";
				case PROP_TIME:
					return "NodeSocketFloatTime";
				case PROP_NONE:
				default:
					return "NodeSocketFloat";
			}
		case SOCK_INT:
			switch (subtype) {
				case PROP_UNSIGNED:
					return "NodeSocketIntUnsigned";
				case PROP_PERCENTAGE:
					return "NodeSocketIntPercentage";
				case PROP_FACTOR:
					return "NodeSocketIntFactor";
				case PROP_NONE:
				default:
					return "NodeSocketInt";
			}
		case SOCK_BOOLEAN:
			return "NodeSocketBool";
		case SOCK_VECTOR:
			switch (subtype) {
				case PROP_TRANSLATION:
					return "NodeSocketVectorTranslation";
				case PROP_DIRECTION:
					return "NodeSocketVectorDirection";
				case PROP_VELOCITY:
					return "NodeSocketVectorVelocity";
				case PROP_ACCELERATION:
					return "NodeSocketVectorAcceleration";
				case PROP_EULER:
					return "NodeSocketVectorEuler";
				case PROP_XYZ:
					return "NodeSocketVectorXYZ";
				case PROP_NONE:
				default:
					return "NodeSocketVector";
			}
		case SOCK_RGBA:
			return "NodeSocketColor";
		case SOCK_STRING:
			return "NodeSocketString";
		case SOCK_SHADER:
			return "NodeSocketShader";
	}
	return NULL;
}

const char *nodeStaticSocketInterfaceType(int type, int subtype)
{
	switch (type) {
		case SOCK_FLOAT:
			switch (subtype) {
				case PROP_UNSIGNED:
					return "NodeSocketInterfaceFloatUnsigned";
				case PROP_PERCENTAGE:
					return "NodeSocketInterfaceFloatPercentage";
				case PROP_FACTOR:
					return "NodeSocketInterfaceFloatFactor";
				case PROP_ANGLE:
					return "NodeSocketInterfaceFloatAngle";
				case PROP_TIME:
					return "NodeSocketInterfaceFloatTime";
				case PROP_NONE:
				default:
					return "NodeSocketInterfaceFloat";
			}
		case SOCK_INT:
			switch (subtype) {
				case PROP_UNSIGNED:
					return "NodeSocketInterfaceIntUnsigned";
				case PROP_PERCENTAGE:
					return "NodeSocketInterfaceIntPercentage";
				case PROP_FACTOR:
					return "NodeSocketInterfaceIntFactor";
				case PROP_NONE:
				default:
					return "NodeSocketInterfaceInt";
			}
		case SOCK_BOOLEAN:
			return "NodeSocketInterfaceBool";
		case SOCK_VECTOR:
			switch (subtype) {
				case PROP_TRANSLATION:
					return "NodeSocketInterfaceVectorTranslation";
				case PROP_DIRECTION:
					return "NodeSocketInterfaceVectorDirection";
				case PROP_VELOCITY:
					return "NodeSocketInterfaceVectorVelocity";
				case PROP_ACCELERATION:
					return "NodeSocketInterfaceVectorAcceleration";
				case PROP_EULER:
					return "NodeSocketInterfaceVectorEuler";
				case PROP_XYZ:
					return "NodeSocketInterfaceVectorXYZ";
				case PROP_NONE:
				default:
					return "NodeSocketInterfaceVector";
			}
		case SOCK_RGBA:
			return "NodeSocketInterfaceColor";
		case SOCK_STRING:
			return "NodeSocketInterfaceString";
		case SOCK_SHADER:
			return "NodeSocketInterfaceShader";
	}
	return NULL;
}

bNodeSocket *nodeAddStaticSocket(bNodeTree *ntree, bNode *node, int in_out, int type, int subtype,
                                 const char *identifier, const char *name)
{
	const char *idname = nodeStaticSocketType(type, subtype);
	bNodeSocket *sock;
	
	if (!idname) {
		printf("Error: static node socket type %d undefined\n", type);
		return NULL;
	}
	
	sock = nodeAddSocket(ntree, node, in_out, idname, identifier, name);
	sock->type = type;
	return sock;
}

bNodeSocket *nodeInsertStaticSocket(bNodeTree *ntree, bNode *node, int in_out, int type, int subtype,
                                    bNodeSocket *next_sock, const char *identifier, const char *name)
{
	const char *idname = nodeStaticSocketType(type, subtype);
	bNodeSocket *sock;
	
	if (!idname) {
		printf("Error: static node socket type %d undefined\n", type);
		return NULL;
	}
	
	sock = nodeInsertSocket(ntree, node, in_out, idname, next_sock, identifier, name);
	sock->type = type;
	return sock;
}

static void node_socket_free(bNodeTree *UNUSED(ntree), bNodeSocket *sock, bNode *UNUSED(node))
{
	if (sock->prop) {
		IDP_FreeProperty(sock->prop);
		MEM_freeN(sock->prop);
	}
	
	if (sock->default_value)
		MEM_freeN(sock->default_value);
}

void nodeRemoveSocket(bNodeTree *ntree, bNode *node, bNodeSocket *sock)
{
	bNodeLink *link, *next;
	
	for (link = ntree->links.first; link; link = next) {
		next = link->next;
		if (link->fromsock == sock || link->tosock == sock) {
			nodeRemLink(ntree, link);
		}
	}
	
	/* this is fast, this way we don't need an in_out argument */
	BLI_remlink(&node->inputs, sock);
	BLI_remlink(&node->outputs, sock);
	
	node_socket_free(ntree, sock, node);
	MEM_freeN(sock);
	
	node->update |= NODE_UPDATE;
}

void nodeRemoveAllSockets(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock, *sock_next;
	bNodeLink *link, *next;
	
	for (link = ntree->links.first; link; link = next) {
		next = link->next;
		if (link->fromnode == node || link->tonode == node) {
			nodeRemLink(ntree, link);
		}
	}
	
	for (sock = node->inputs.first; sock; sock = sock_next) {
		sock_next = sock->next;
		node_socket_free(ntree, sock, node);
		MEM_freeN(sock);
	}
	for (sock = node->outputs.first; sock; sock = sock_next) {
		sock_next = sock->next;
		node_socket_free(ntree, sock, node);
		MEM_freeN(sock);
	}
	
	node->update |= NODE_UPDATE;
}

/* finds a node based on its name */
bNode *nodeFindNodebyName(bNodeTree *ntree, const char *name)
{
	return BLI_findstring(&ntree->nodes, name, offsetof(bNode, name));
}

/* finds a node based on given socket */
int nodeFindNode(bNodeTree *ntree, bNodeSocket *sock, bNode **nodep, int *sockindex)
{
	int in_out = sock->in_out;
	bNode *node;
	bNodeSocket *tsock;
	int index = 0;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		tsock = (in_out == SOCK_IN ? node->inputs.first : node->outputs.first);
		for (index = 0; tsock; tsock = tsock->next, index++) {
			if (tsock == sock)
				break;
		}
		if (tsock)
			break;
	}

	if (node) {
		*nodep = node;
		if (sockindex) *sockindex = index;
		return 1;
	}
	
	*nodep = NULL;
	return 0;
}

/* ************** Add stuff ********** */

/* Find the first available, non-duplicate name for a given node */
void nodeUniqueName(bNodeTree *ntree, bNode *node)
{
	BLI_uniquename(&ntree->nodes, node, DATA_("Node"), '.', offsetof(bNode, name), sizeof(node->name));
}

bNode *nodeAddNode(const struct bContext *C, bNodeTree *ntree, const char *idname)
{
	bNode *node;
	
	node = MEM_callocN(sizeof(bNode), "new node");
	BLI_addtail(&ntree->nodes, node);
	
	BLI_strncpy(node->idname, idname, sizeof(node->idname));
	node_set_typeinfo(C, ntree, node, nodeTypeFind(idname));
	
	ntree->update |= NTREE_UPDATE_NODES;
	
	return node;
}

bNode *nodeAddStaticNode(const struct bContext *C, bNodeTree *ntree, int type)
{
	const char *idname = NULL;
	
	NODE_TYPES_BEGIN(ntype)
		/* do an extra poll here, because some int types are used
		 * for multiple node types, this helps find the desired type
		 */
		if (ntype->type == type && (!ntype->poll || ntype->poll(ntype, ntree))) {
			idname = ntype->idname;
			break;
		}
	NODE_TYPES_END
	if (!idname) {
		printf("Error: static node type %d undefined\n", type);
		return NULL;
	}
	return nodeAddNode(C, ntree, idname);
}

static void node_socket_copy(bNodeSocket *dst, bNodeSocket *src)
{
	src->new_sock = dst;
	
	if (src->prop)
		dst->prop = IDP_CopyProperty(src->prop);
	
	if (src->default_value)
		dst->default_value = MEM_dupallocN(src->default_value);
	
	dst->stack_index = 0;
	/* XXX some compositor node (e.g. image, render layers) still store
	 * some persistent buffer data here, need to clear this to avoid dangling pointers.
	 */
	dst->cache = NULL;
}

/* keep socket listorder identical, for copying links */
/* ntree is the target tree */
bNode *nodeCopyNode(struct bNodeTree *ntree, struct bNode *node)
{
	bNode *nnode = MEM_callocN(sizeof(bNode), "dupli node");
	bNodeSocket *sock, *oldsock;
	bNodeLink *link, *oldlink;

	*nnode = *node;
	/* can be called for nodes outside a node tree (e.g. clipboard) */
	if (ntree) {
		nodeUniqueName(ntree, nnode);

		BLI_addtail(&ntree->nodes, nnode);
	}

	BLI_duplicatelist(&nnode->inputs, &node->inputs);
	oldsock = node->inputs.first;
	for (sock = nnode->inputs.first; sock; sock = sock->next, oldsock = oldsock->next)
		node_socket_copy(sock, oldsock);
	
	BLI_duplicatelist(&nnode->outputs, &node->outputs);
	oldsock = node->outputs.first;
	for (sock = nnode->outputs.first; sock; sock = sock->next, oldsock = oldsock->next)
		node_socket_copy(sock, oldsock);
	
	if (node->prop)
		nnode->prop = IDP_CopyProperty(node->prop);
	
	BLI_duplicatelist(&nnode->internal_links, &node->internal_links);
	oldlink = node->internal_links.first;
	for (link = nnode->internal_links.first; link; link = link->next, oldlink = oldlink->next) {
		link->fromnode = nnode;
		link->tonode = nnode;
		link->fromsock = link->fromsock->new_sock;
		link->tosock = link->tosock->new_sock;
	}
	
	/* don't increase node->id users, freenode doesn't decrement either */
	
	if (node->typeinfo->copyfunc)
		node->typeinfo->copyfunc(ntree, nnode, node);
	
	node->new_node = nnode;
	nnode->new_node = NULL;
	
	if (nnode->typeinfo->copyfunc_api) {
		PointerRNA ptr;
		RNA_pointer_create((ID *)ntree, &RNA_Node, nnode, &ptr);
		
		nnode->typeinfo->copyfunc_api(&ptr, node);
	}
	
	if (ntree)
		ntree->update |= NTREE_UPDATE_NODES;
	
	return nnode;
}

/* also used via rna api, so we check for proper input output direction */
bNodeLink *nodeAddLink(bNodeTree *ntree, bNode *fromnode, bNodeSocket *fromsock, bNode *tonode, bNodeSocket *tosock)
{
	bNodeLink *link = NULL;
	
	/* test valid input */
	BLI_assert(fromnode);
	BLI_assert(tonode);
	
	if (fromsock->in_out == SOCK_OUT && tosock->in_out == SOCK_IN) {
		link = MEM_callocN(sizeof(bNodeLink), "link");
		if (ntree)
			BLI_addtail(&ntree->links, link);
		link->fromnode = fromnode;
		link->fromsock = fromsock;
		link->tonode = tonode;
		link->tosock = tosock;
	}
	else if (fromsock->in_out == SOCK_IN && tosock->in_out == SOCK_OUT) {
		/* OK but flip */
		link = MEM_callocN(sizeof(bNodeLink), "link");
		if (ntree)
			BLI_addtail(&ntree->links, link);
		link->fromnode = tonode;
		link->fromsock = tosock;
		link->tonode = fromnode;
		link->tosock = fromsock;
	}
	
	if (ntree)
		ntree->update |= NTREE_UPDATE_LINKS;
	
	return link;
}

void nodeRemLink(bNodeTree *ntree, bNodeLink *link)
{
	/* can be called for links outside a node tree (e.g. clipboard) */
	if (ntree)
		BLI_remlink(&ntree->links, link);

	if (link->tosock)
		link->tosock->link = NULL;
	MEM_freeN(link);
	
	if (ntree)
		ntree->update |= NTREE_UPDATE_LINKS;
}

void nodeRemSocketLinks(bNodeTree *ntree, bNodeSocket *sock)
{
	bNodeLink *link, *next;
	
	for (link = ntree->links.first; link; link = next) {
		next = link->next;
		if (link->fromsock == sock || link->tosock == sock) {
			nodeRemLink(ntree, link);
		}
	}
	
	ntree->update |= NTREE_UPDATE_LINKS;
}

int nodeLinkIsHidden(bNodeLink *link)
{
	return nodeSocketIsHidden(link->fromsock) || nodeSocketIsHidden(link->tosock);
}

void nodeInternalRelink(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link, *link_next;
	
	/* store link pointers in output sockets, for efficient lookup */
	for (link = node->internal_links.first; link; link = link->next)
		link->tosock->link = link;
	
	/* redirect downstream links */
	for (link = ntree->links.first; link; link = link_next) {
		link_next = link->next;
		
		/* do we have internal link? */
		if (link->fromnode == node) {
			if (link->fromsock->link) {
				/* get the upstream input link */
				bNodeLink *fromlink = link->fromsock->link->fromsock->link;
				/* skip the node */
				if (fromlink) {
					link->fromnode = fromlink->fromnode;
					link->fromsock = fromlink->fromsock;
					
					/* if the up- or downstream link is invalid,
					 * the replacement link will be invalid too.
					 */
					if (!(fromlink->flag & NODE_LINK_VALID))
						link->flag &= ~NODE_LINK_VALID;
					
					ntree->update |= NTREE_UPDATE_LINKS;
				}
				else
					nodeRemLink(ntree, link);
			}
			else
				nodeRemLink(ntree, link);
		}
	}
	
	/* remove remaining upstream links */
	for (link = ntree->links.first; link; link = link_next) {
		link_next = link->next;
		
		if (link->tonode == node)
			nodeRemLink(ntree, link);
	}
}

void nodeToView(bNode *node, float x, float y, float *rx, float *ry)
{
	if (node->parent) {
		nodeToView(node->parent, x + node->locx, y + node->locy, rx, ry);
	}
	else {
		*rx = x + node->locx;
		*ry = y + node->locy;
	}
}

void nodeFromView(bNode *node, float x, float y, float *rx, float *ry)
{
	if (node->parent) {
		nodeFromView(node->parent, x, y, rx, ry);
		*rx -= node->locx;
		*ry -= node->locy;
	}
	else {
		*rx = x - node->locx;
		*ry = y - node->locy;
	}
}

int nodeAttachNodeCheck(bNode *node, bNode *parent)
{
	bNode *parent_recurse;
	for (parent_recurse = node; parent_recurse; parent_recurse = parent_recurse->parent) {
		if (parent_recurse == parent) {
			return TRUE;
		}
	}

	return FALSE;
}

void nodeAttachNode(bNode *node, bNode *parent)
{
	float locx, locy;

	BLI_assert(parent->type == NODE_FRAME);
	BLI_assert(nodeAttachNodeCheck(parent, node) == FALSE);

	nodeToView(node, 0.0f, 0.0f, &locx, &locy);
	
	node->parent = parent;
	/* transform to parent space */
	nodeFromView(parent, locx, locy, &node->locx, &node->locy);
}

void nodeDetachNode(struct bNode *node)
{
	float locx, locy;
	
	if (node->parent) {

		BLI_assert(node->parent->type == NODE_FRAME);

		/* transform to view space */
		nodeToView(node, 0.0f, 0.0f, &locx, &locy);
		node->locx = locx;
		node->locy = locy;
		node->parent = NULL;
	}
}

bNodeTree *ntreeAddTree(Main *bmain, const char *name, const char *idname)
{
	bNodeTree *ntree;
	
	/* trees are created as local trees for compositor, material or texture nodes,
	 * node groups and other tree types are created as library data.
	 */
	if (bmain) {
		ntree = BKE_libblock_alloc(bmain, ID_NT, name);
	}
	else {
		ntree = MEM_callocN(sizeof(bNodeTree), "new node tree");
		*( (short *)ntree->id.name ) = ID_NT;
		BLI_strncpy(ntree->id.name + 2, name, sizeof(ntree->id.name));
	}
	
	/* Types are fully initialized at this point,
	 * if an undefined node is added later this will be reset.
	 */
	ntree->init |= NTREE_TYPE_INIT;
	
	BLI_strncpy(ntree->idname, idname, sizeof(ntree->idname));
	ntree_set_typeinfo(ntree, ntreeTypeFind(idname));
	
	return ntree;
}

/* Warning: this function gets called during some rather unexpected times
 *	- this gets called when executing compositing updates (for threaded previews)
 *	- when the nodetree datablock needs to be copied (i.e. when users get copied)
 *	- for scene duplication use ntreeSwapID() after so we don't have stale pointers.
 *
 * do_make_extern: keep enabled for general use, only reason _not_ to enable is when
 * copying for internal use (threads for eg), where you wont want it to modify the
 * scene data.
 */
static bNodeTree *ntreeCopyTree_internal(bNodeTree *ntree, Main *bmain, bool do_id_user, bool do_make_extern, bool copy_previews)
{
	bNodeTree *newtree;
	bNode *node /*, *nnode */ /* UNUSED */, *last;
	bNodeSocket *sock, *oldsock;
	bNodeLink *link;
	
	if (ntree == NULL) return NULL;
	
	if (bmain) {
		/* is ntree part of library? */
		if (BLI_findindex(&bmain->nodetree, ntree) != -1)
			newtree = BKE_libblock_copy(&ntree->id);
		else
			newtree = NULL;
	}
	else
		newtree = NULL;
	
	if (newtree == NULL) {
		newtree = MEM_dupallocN(ntree);
		newtree->id.lib = NULL;	/* same as owning datablock id.lib */
		BKE_libblock_copy_data(&newtree->id, &ntree->id, true); /* copy animdata and ID props */
	}

	id_us_plus((ID *)newtree->gpd);

	/* in case a running nodetree is copied */
	newtree->execdata = NULL;
	
	BLI_listbase_clear(&newtree->nodes);
	BLI_listbase_clear(&newtree->links);
	
	last = ntree->nodes.last;
	for (node = ntree->nodes.first; node; node = node->next) {

		/* ntreeUserDecrefID inline */
		if (do_id_user) {
			id_us_plus(node->id);
		}

		if (do_make_extern) {
			id_lib_extern(node->id);
		}

		node->new_node = NULL;
		/* nnode = */ nodeCopyNode(newtree, node);   /* sets node->new */
		
		/* make sure we don't copy new nodes again! */
		if (node == last)
			break;
	}
	
	/* copy links */
	BLI_duplicatelist(&newtree->links, &ntree->links);
	for (link = newtree->links.first; link; link = link->next) {
		link->fromnode = (link->fromnode ? link->fromnode->new_node : NULL);
		link->fromsock = (link->fromsock ? link->fromsock->new_sock : NULL);
		link->tonode = (link->tonode ? link->tonode->new_node : NULL);
		link->tosock = (link->tosock ? link->tosock->new_sock : NULL);
		/* update the link socket's pointer */
		if (link->tosock)
			link->tosock->link = link;
	}
	
	/* copy interface sockets */
	BLI_duplicatelist(&newtree->inputs, &ntree->inputs);
	oldsock = ntree->inputs.first;
	for (sock = newtree->inputs.first; sock; sock = sock->next, oldsock = oldsock->next)
		node_socket_copy(sock, oldsock);
	
	BLI_duplicatelist(&newtree->outputs, &ntree->outputs);
	oldsock = ntree->outputs.first;
	for (sock = newtree->outputs.first; sock; sock = sock->next, oldsock = oldsock->next)
		node_socket_copy(sock, oldsock);
	
	/* copy preview hash */
	if (ntree->previews && copy_previews) {
		bNodeInstanceHashIterator iter;
		
		newtree->previews = BKE_node_instance_hash_new("node previews");
		
		NODE_INSTANCE_HASH_ITER(iter, ntree->previews) {
			bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
			bNodePreview *preview = BKE_node_instance_hash_iterator_get_value(&iter);
			BKE_node_instance_hash_insert(newtree->previews, key, BKE_node_preview_copy(preview));
		}
	}
	else
		newtree->previews = NULL;
	
	/* update node->parent pointers */
	for (node = newtree->nodes.first; node; node = node->next) {
		if (node->parent)
			node->parent = node->parent->new_node;
	}
	
	/* node tree will generate its own interface type */
	newtree->interface_type = NULL;
	
	return newtree;
}

bNodeTree *ntreeCopyTree_ex(bNodeTree *ntree, const bool do_id_user)
{
	return ntreeCopyTree_internal(ntree, G.main, do_id_user, TRUE, TRUE);
}
bNodeTree *ntreeCopyTree(bNodeTree *ntree)
{
	return ntreeCopyTree_ex(ntree, TRUE);
}

/* use when duplicating scenes */
void ntreeSwitchID_ex(bNodeTree *ntree, ID *id_from, ID *id_to, const bool do_id_user)
{
	bNode *node;

	if (id_from == id_to) {
		/* should never happen but may as well skip if it does */
		return;
	}

	/* for scene duplication only */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id == id_from) {
			if (do_id_user) {
				id_us_min(id_from);
				id_us_plus(id_to);
			}

			node->id = id_to;
		}
	}
}
void ntreeSwitchID(bNodeTree *ntree, ID *id_from, ID *id_to)
{
	ntreeSwitchID_ex(ntree, id_from, id_to, TRUE);
}

void ntreeUserIncrefID(bNodeTree *ntree)
{
	bNode *node;
	for (node = ntree->nodes.first; node; node = node->next) {
		id_us_plus(node->id);
	}
}
void ntreeUserDecrefID(bNodeTree *ntree)
{
	bNode *node;
	for (node = ntree->nodes.first; node; node = node->next) {
		id_us_min(node->id);
	}
}

/* *************** Node Preview *********** */

/* XXX this should be removed eventually ...
 * Currently BKE functions are modelled closely on previous code,
 * using BKE_node_preview_init_tree to set up previews for a whole node tree in advance.
 * This should be left more to the individual node tree implementations.
 */
int BKE_node_preview_used(bNode *node)
{
	/* XXX check for closed nodes? */
	return (node->typeinfo->flag & NODE_PREVIEW) != 0;
}

bNodePreview *BKE_node_preview_verify(bNodeInstanceHash *previews, bNodeInstanceKey key, int xsize, int ysize, int create)
{
	bNodePreview *preview;
	
	preview = BKE_node_instance_hash_lookup(previews, key);
	if (!preview) {
		if (create) {
			preview = MEM_callocN(sizeof(bNodePreview), "node preview");
			BKE_node_instance_hash_insert(previews, key, preview);
		}
		else
			return NULL;
	}
	
	/* node previews can get added with variable size this way */
	if (xsize == 0 || ysize == 0)
		return preview;
	
	/* sanity checks & initialize */
	if (preview->rect) {
		if (preview->xsize != xsize || preview->ysize != ysize) {
			MEM_freeN(preview->rect);
			preview->rect = NULL;
		}
	}
	
	if (preview->rect == NULL) {
		preview->rect = MEM_callocN(4 * xsize + xsize * ysize * sizeof(char) * 4, "node preview rect");
		preview->xsize = xsize;
		preview->ysize = ysize;
	}
	/* no clear, makes nicer previews */
	
	return preview;
}

bNodePreview *BKE_node_preview_copy(bNodePreview *preview)
{
	bNodePreview *new_preview = MEM_dupallocN(preview);
	if (preview->rect)
		new_preview->rect = MEM_dupallocN(preview->rect);
	return new_preview;
}

void BKE_node_preview_free(bNodePreview *preview)
{
	if (preview->rect)
		MEM_freeN(preview->rect);
	MEM_freeN(preview);
}

static void node_preview_init_tree_recursive(bNodeInstanceHash *previews, bNodeTree *ntree, bNodeInstanceKey parent_key, int xsize, int ysize, int create)
{
	bNode *node;
	for (node = ntree->nodes.first; node; node = node->next) {
		bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);
		
		if (BKE_node_preview_used(node)) {
			node->preview_xsize = xsize;
			node->preview_ysize = ysize;
			
			BKE_node_preview_verify(previews, key, xsize, ysize, create);
		}
		
		if (node->type == NODE_GROUP && node->id)
			node_preview_init_tree_recursive(previews, (bNodeTree *)node->id, key, xsize, ysize, create);
	}
}

void BKE_node_preview_init_tree(bNodeTree *ntree, int xsize, int ysize, int create_previews)
{
	if (!ntree)
		return;
	
	if (!ntree->previews)
		ntree->previews = BKE_node_instance_hash_new("node previews");
	
	node_preview_init_tree_recursive(ntree->previews, ntree, NODE_INSTANCE_KEY_BASE, xsize, ysize, create_previews);
}

static void node_preview_tag_used_recursive(bNodeInstanceHash *previews, bNodeTree *ntree, bNodeInstanceKey parent_key)
{
	bNode *node;
	for (node = ntree->nodes.first; node; node = node->next) {
		bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);
		
		if (BKE_node_preview_used(node))
			BKE_node_instance_hash_tag_key(previews, key);
		
		if (node->type == NODE_GROUP && node->id)
			node_preview_tag_used_recursive(previews, (bNodeTree *)node->id, key);
	}
}

void BKE_node_preview_remove_unused(bNodeTree *ntree)
{
	if (!ntree || !ntree->previews)
		return;
	
	/* use the instance hash functions for tagging and removing unused previews */
	BKE_node_instance_hash_clear_tags(ntree->previews);
	node_preview_tag_used_recursive(ntree->previews, ntree, NODE_INSTANCE_KEY_BASE);
	
	BKE_node_instance_hash_remove_untagged(ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
}

void BKE_node_preview_free_tree(bNodeTree *ntree)
{
	if (!ntree)
		return;
	
	if (ntree->previews) {
		BKE_node_instance_hash_free(ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
		ntree->previews = NULL;
	}
}

void BKE_node_preview_clear(bNodePreview *preview)
{
	if (preview && preview->rect)
		memset(preview->rect, 0, MEM_allocN_len(preview->rect));
}

void BKE_node_preview_clear_tree(bNodeTree *ntree)
{
	bNodeInstanceHashIterator iter;
	
	if (!ntree || !ntree->previews)
		return;
	
	NODE_INSTANCE_HASH_ITER(iter, ntree->previews) {
		bNodePreview *preview = BKE_node_instance_hash_iterator_get_value(&iter);
		BKE_node_preview_clear(preview);
	}
}

static void node_preview_sync(bNodePreview *to, bNodePreview *from)
{
	/* sizes should have been initialized by BKE_node_preview_init_tree */
	BLI_assert(to->xsize == from->xsize && to->ysize == from->ysize);
	
	/* copy over contents of previews */
	if (to->rect && from->rect) {
		int xsize = to->xsize;
		int ysize = to->ysize;
		memcpy(to->rect, from->rect, 4 * xsize + xsize * ysize * sizeof(char) * 4);
	}
}

void BKE_node_preview_sync_tree(bNodeTree *to_ntree, bNodeTree *from_ntree)
{
	bNodeInstanceHash *from_previews = from_ntree->previews;
	bNodeInstanceHash *to_previews = to_ntree->previews;
	bNodeInstanceHashIterator iter;
	
	if (!from_previews || !to_previews)
		return;
	
	NODE_INSTANCE_HASH_ITER(iter, from_previews) {
		bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
		bNodePreview *from = BKE_node_instance_hash_iterator_get_value(&iter);
		bNodePreview *to = BKE_node_instance_hash_lookup(to_previews, key);
		
		if (from && to)
			node_preview_sync(to, from);
	}
}

void BKE_node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old)
{
	if (remove_old || !to_ntree->previews) {
		/* free old previews */
		if (to_ntree->previews)
			BKE_node_instance_hash_free(to_ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
		
		/* transfer previews */
		to_ntree->previews = from_ntree->previews;
		from_ntree->previews = NULL;
		
		/* clean up, in case any to_ntree nodes have been removed */
		BKE_node_preview_remove_unused(to_ntree);
	}
	else {
		bNodeInstanceHashIterator iter;
		
		if (from_ntree->previews) {
			NODE_INSTANCE_HASH_ITER(iter, from_ntree->previews) {
				bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
				bNodePreview *preview = BKE_node_instance_hash_iterator_get_value(&iter);
				
				/* replace existing previews */
				BKE_node_instance_hash_remove(to_ntree->previews, key, (bNodeInstanceValueFP)BKE_node_preview_free);
				BKE_node_instance_hash_insert(to_ntree->previews, key, preview);
			}
			
			/* Note: NULL free function here, because pointers have already been moved over to to_ntree->previews! */
			BKE_node_instance_hash_free(from_ntree->previews, NULL);
			from_ntree->previews = NULL;
		}
	}
}

/* hack warning! this function is only used for shader previews, and 
 * since it gets called multiple times per pixel for Ztransp we only
 * add the color once. Preview gets cleared before it starts render though */
void BKE_node_preview_set_pixel(bNodePreview *preview, const float col[4], int x, int y, bool do_manage)
{
	if (preview) {
		if (x >= 0 && y >= 0) {
			if (x < preview->xsize && y < preview->ysize) {
				unsigned char *tar = preview->rect + 4 * ((preview->xsize * y) + x);
				
				if (do_manage) {
					linearrgb_to_srgb_uchar4(tar, col);
				}
				else {
					rgba_float_to_uchar(tar, col);
				}
			}
			//else printf("prv out bound x y %d %d\n", x, y);
		}
		//else printf("prv out bound x y %d %d\n", x, y);
	}
}

#if 0
static void nodeClearPreview(bNode *node)
{
	if (node->preview && node->preview->rect)
		memset(node->preview->rect, 0, MEM_allocN_len(node->preview->rect));
}

/* use it to enforce clear */
void ntreeClearPreview(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree == NULL)
		return;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->typeinfo->flag & NODE_PREVIEW)
			nodeClearPreview(node);
		if (node->type == NODE_GROUP)
			ntreeClearPreview((bNodeTree *)node->id);
	}
}

/* hack warning! this function is only used for shader previews, and 
 * since it gets called multiple times per pixel for Ztransp we only
 * add the color once. Preview gets cleared before it starts render though */
void nodeAddToPreview(bNode *node, const float col[4], int x, int y, int do_manage)
{
	bNodePreview *preview = node->preview;
	if (preview) {
		if (x >= 0 && y >= 0) {
			if (x < preview->xsize && y < preview->ysize) {
				unsigned char *tar = preview->rect + 4 * ((preview->xsize * y) + x);
				
				if (do_manage) {
					linearrgb_to_srgb_uchar4(tar, col);
				}
				else {
					rgba_float_to_uchar(tar, col);
				}
			}
			//else printf("prv out bound x y %d %d\n", x, y);
		}
		//else printf("prv out bound x y %d %d\n", x, y);
	}
}
#endif

/* ************** Free stuff ********** */

/* goes over entire tree */
void nodeUnlinkNode(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link, *next;
	bNodeSocket *sock;
	ListBase *lb;
	
	for (link = ntree->links.first; link; link = next) {
		next = link->next;
		
		if (link->fromnode == node) {
			lb = &node->outputs;
			if (link->tonode)
				link->tonode->update |= NODE_UPDATE;
		}
		else if (link->tonode == node)
			lb = &node->inputs;
		else
			lb = NULL;

		if (lb) {
			for (sock = lb->first; sock; sock = sock->next) {
				if (link->fromsock == sock || link->tosock == sock)
					break;
			}
			if (sock) {
				nodeRemLink(ntree, link);
			}
		}
	}
}

static void node_unlink_attached(bNodeTree *ntree, bNode *parent)
{
	bNode *node;
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->parent == parent)
			nodeDetachNode(node);
	}
}

/** \note caller needs to manage node->id user */
static void node_free_node_ex(bNodeTree *ntree, bNode *node, bool remove_animdata, bool use_api_free_cb)
{
	bNodeSocket *sock, *nextsock;
	
	/* don't remove node animdata if the tree is localized,
	 * Action is shared with the original tree (T38221)
	 */
	remove_animdata &= ntree && !(ntree->flag & NTREE_IS_LOCALIZED);
	
	/* extra free callback */
	if (use_api_free_cb && node->typeinfo->freefunc_api) {
		PointerRNA ptr;
		RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);
		
		node->typeinfo->freefunc_api(&ptr);
	}
	
	/* since it is called while free database, node->id is undefined */
	
	/* can be called for nodes outside a node tree (e.g. clipboard) */
	if (ntree) {
		/* remove all references to this node */
		nodeUnlinkNode(ntree, node);
		node_unlink_attached(ntree, node);
		
		BLI_remlink(&ntree->nodes, node);
		
		if (remove_animdata) {
			char propname_esc[MAX_IDPROP_NAME * 2];
			char prefix[MAX_IDPROP_NAME * 2];

			BLI_strescape(propname_esc, node->name, sizeof(propname_esc));
			BLI_snprintf(prefix, sizeof(prefix), "nodes[\"%s\"]", propname_esc);

			BKE_animdata_fix_paths_remove((ID *)ntree, prefix);
		}

		if (ntree->typeinfo->free_node_cache)
			ntree->typeinfo->free_node_cache(ntree, node);
		
		/* texture node has bad habit of keeping exec data around */
		if (ntree->type == NTREE_TEXTURE && ntree->execdata) {
			ntreeTexEndExecTree(ntree->execdata);
			ntree->execdata = NULL;
		}
		
		if (node->typeinfo->freefunc)
			node->typeinfo->freefunc(node);
	}
	
	for (sock = node->inputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		node_socket_free(ntree, sock, node);
		MEM_freeN(sock);
	}
	for (sock = node->outputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		node_socket_free(ntree, sock, node);
		MEM_freeN(sock);
	}

	BLI_freelistN(&node->internal_links);

	if (node->prop) {
		IDP_FreeProperty(node->prop);
		MEM_freeN(node->prop);
	}

	MEM_freeN(node);
	
	if (ntree)
		ntree->update |= NTREE_UPDATE_NODES;
}

void nodeFreeNode(bNodeTree *ntree, bNode *node)
{
	node_free_node_ex(ntree, node, true, true);
}

static void node_socket_interface_free(bNodeTree *UNUSED(ntree), bNodeSocket *sock)
{
	if (sock->prop) {
		IDP_FreeProperty(sock->prop);
		MEM_freeN(sock->prop);
	}
	
	if (sock->default_value)
		MEM_freeN(sock->default_value);
}

static void free_localized_node_groups(bNodeTree *ntree)
{
	bNode *node;
	
	/* Only localized node trees store a copy for each node group tree.
	 * Each node group tree in a localized node tree can be freed,
	 * since it is a localized copy itself (no risk of accessing free'd
	 * data in main, see [#37939]).
	 */
	if (!(ntree->flag & NTREE_IS_LOCALIZED))
		return;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP && node->id) {
			bNodeTree *ngroup = (bNodeTree *)node->id;
			ntreeFreeTree_ex(ngroup, false);
			MEM_freeN(ngroup);
		}
	}
}

/* do not free ntree itself here, BKE_libblock_free calls this function too */
void ntreeFreeTree_ex(bNodeTree *ntree, const bool do_id_user)
{
	bNodeTree *tntree;
	bNode *node, *next;
	bNodeSocket *sock, *nextsock;
	
	if (ntree == NULL) return;
	
	/* XXX hack! node trees should not store execution graphs at all.
	 * This should be removed when old tree types no longer require it.
	 * Currently the execution data for texture nodes remains in the tree
	 * after execution, until the node tree is updated or freed.
	 */
	if (ntree->execdata) {
		switch (ntree->type) {
			case NTREE_SHADER:
				ntreeShaderEndExecTree(ntree->execdata);
				break;
			case NTREE_TEXTURE:
				ntreeTexEndExecTree(ntree->execdata);
				ntree->execdata = NULL;
				break;
		}
	}
	
	/* XXX not nice, but needed to free localized node groups properly */
	free_localized_node_groups(ntree);
	
	/* unregister associated RNA types */
	ntreeInterfaceTypeFree(ntree);
	
	BKE_free_animdata((ID *)ntree);
	
	id_us_min((ID *)ntree->gpd);

	BLI_freelistN(&ntree->links);   /* do first, then unlink_node goes fast */
	
	for (node = ntree->nodes.first; node; node = next) {
		next = node->next;

		/* ntreeUserIncrefID inline */

		/* XXX, this is correct, however when freeing the entire database
		 * this ends up accessing freed data which isn't properly unlinking
		 * its self from scene nodes, SO - for now prefer invalid usercounts
		 * on free rather then bad memory access - Campbell */
#if 0
		if (do_id_user) {
			id_us_min(node->id);
		}
#else
		(void)do_id_user;
#endif

		node_free_node_ex(ntree, node, false, false);
	}

	/* free interface sockets */
	for (sock = ntree->inputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		node_socket_interface_free(ntree, sock);
		MEM_freeN(sock);
	}
	for (sock = ntree->outputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		node_socket_interface_free(ntree, sock);
		MEM_freeN(sock);
	}
	
	/* free preview hash */
	if (ntree->previews) {
		BKE_node_instance_hash_free(ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
	}
	
	/* if ntree is not part of library, free the libblock data explicitly */
	for (tntree = G.main->nodetree.first; tntree; tntree = tntree->id.next)
		if (tntree == ntree)
			break;
	if (tntree == NULL) {
		BKE_libblock_free_data(&ntree->id);
	}
}
/* same as ntreeFreeTree_ex but always manage users */
void ntreeFreeTree(bNodeTree *ntree)
{
	ntreeFreeTree_ex(ntree, TRUE);
}

void ntreeFreeCache(bNodeTree *ntree)
{
	if (ntree == NULL) return;
	
	if (ntree->typeinfo->free_cache)
		ntree->typeinfo->free_cache(ntree);
}

void ntreeSetOutput(bNodeTree *ntree)
{
	bNode *node;

	/* find the active outputs, might become tree type dependent handler */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->typeinfo->nclass == NODE_CLASS_OUTPUT) {
			bNode *tnode;
			int output = 0;
			
			/* we need a check for which output node should be tagged like this, below an exception */
			if (node->type == CMP_NODE_OUTPUT_FILE)
				continue;

			/* there is more types having output class, each one is checked */
			for (tnode = ntree->nodes.first; tnode; tnode = tnode->next) {
				if (tnode->typeinfo->nclass == NODE_CLASS_OUTPUT) {
					
					if (ntree->type == NTREE_COMPOSIT) {
							
						/* same type, exception for viewer */
						if (tnode->type == node->type ||
						    (ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER) &&
						     ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)))
						{
							if (tnode->flag & NODE_DO_OUTPUT) {
								output++;
								if (output > 1)
									tnode->flag &= ~NODE_DO_OUTPUT;
							}
						}
					}
					else {
						/* same type */
						if (tnode->type == node->type) {
							if (tnode->flag & NODE_DO_OUTPUT) {
								output++;
								if (output > 1)
									tnode->flag &= ~NODE_DO_OUTPUT;
							}
						}
					}
				}
			}
			if (output == 0)
				node->flag |= NODE_DO_OUTPUT;
		}
		
		/* group node outputs use this flag too */
		if (node->type == NODE_GROUP_OUTPUT) {
			bNode *tnode;
			int output = 0;
			
			for (tnode = ntree->nodes.first; tnode; tnode = tnode->next) {
				if (tnode->type == NODE_GROUP_OUTPUT) {
					if (tnode->flag & NODE_DO_OUTPUT) {
						output++;
						if (output > 1)
							tnode->flag &= ~NODE_DO_OUTPUT;
					}
				}
			}
			if (output == 0)
				node->flag |= NODE_DO_OUTPUT;
		}
	}
	
	/* here we could recursively set which nodes have to be done,
	 * might be different for editor or for "real" use... */
}

bNodeTree *ntreeFromID(ID *id)
{
	switch (GS(id->name)) {
		case ID_MA:  return ((Material *)id)->nodetree;
		case ID_LA:  return ((Lamp *)id)->nodetree;
		case ID_WO:  return ((World *)id)->nodetree;
		case ID_TE:  return ((Tex *)id)->nodetree;
		case ID_SCE: return ((Scene *)id)->nodetree;
		default: return NULL;
	}
}

void ntreeMakeLocal(bNodeTree *ntree)
{
	Main *bmain = G.main;
	int lib = FALSE, local = FALSE;
	
	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (ntree->id.lib == NULL) return;
	if (ntree->id.us == 1) {
		id_clear_lib_data(bmain, (ID *)ntree);
		return;
	}
	
	/* now check users of groups... again typedepending, callback... */
	FOREACH_NODETREE(G.main, tntree, owner_id) {
		bNode *node;
		/* find if group is in tree */
		for (node = tntree->nodes.first; node; node = node->next) {
			if (node->id == (ID *)ntree) {
				if (owner_id->lib)
					lib = TRUE;
				else
					local = TRUE;
			}
		}
	} FOREACH_NODETREE_END
	
	/* if all users are local, we simply make tree local */
	if (local && !lib) {
		id_clear_lib_data(bmain, (ID *)ntree);
	}
	else if (local && lib) {
		/* this is the mixed case, we copy the tree and assign it to local users */
		bNodeTree *newtree = ntreeCopyTree(ntree);
		
		newtree->id.us = 0;
		
		FOREACH_NODETREE(G.main, tntree, owner_id) {
			bNode *node;
			/* find if group is in tree */
			for (node = tntree->nodes.first; node; node = node->next) {
				if (node->id == (ID *)ntree) {
					if (owner_id->lib == NULL) {
						node->id = (ID *)newtree;
						newtree->id.us++;
						ntree->id.us--;
					}
				}
			}
		} FOREACH_NODETREE_END
	}
}

int ntreeNodeExists(bNodeTree *ntree, bNode *testnode)
{
	bNode *node = ntree->nodes.first;
	for (; node; node = node->next)
		if (node == testnode)
			return 1;
	return 0;
}

int ntreeOutputExists(bNode *node, bNodeSocket *testsock)
{
	bNodeSocket *sock = node->outputs.first;
	for (; sock; sock = sock->next)
		if (sock == testsock)
			return 1;
	return 0;
}

/* returns localized tree for execution in threads */
bNodeTree *ntreeLocalize(bNodeTree *ntree)
{
	if (ntree) {
		bNodeTree *ltree;
		bNode *node;
		
		bAction *action_backup = NULL, *tmpact_backup = NULL;
		
		/* Workaround for copying an action on each render!
		 * set action to NULL so animdata actions don't get copied */
		AnimData *adt = BKE_animdata_from_id(&ntree->id);
	
		if (adt) {
			action_backup = adt->action;
			tmpact_backup = adt->tmpact;
	
			adt->action = NULL;
			adt->tmpact = NULL;
		}
	
		/* Make full copy.
		 * Note: previews are not copied here.
		 */
		ltree = ntreeCopyTree_internal(ntree, NULL, FALSE, FALSE, FALSE);
		ltree->flag |= NTREE_IS_LOCALIZED;
		
		for (node = ltree->nodes.first; node; node = node->next) {
			if (node->type == NODE_GROUP && node->id) {
				node->id = (ID *)ntreeLocalize((bNodeTree *)node->id);
			}
		}
		
		if (adt) {
			AnimData *ladt = BKE_animdata_from_id(&ltree->id);
	
			adt->action = ladt->action = action_backup;
			adt->tmpact = ladt->tmpact = tmpact_backup;
	
			if (action_backup) action_backup->id.us++;
			if (tmpact_backup) tmpact_backup->id.us++;
			
		}
		/* end animdata uglyness */
	
		/* ensures only a single output node is enabled */
		ntreeSetOutput(ntree);
	
		for (node = ntree->nodes.first; node; node = node->next) {
			/* store new_node pointer to original */
			node->new_node->new_node = node;
		}
	
		if (ntree->typeinfo->localize)
			ntree->typeinfo->localize(ltree, ntree);
	
		return ltree;
	}
	else
		return NULL;
}

/* sync local composite with real tree */
/* local tree is supposed to be running, be careful moving previews! */
/* is called by jobs manager, outside threads, so it doesnt happen during draw */
void ntreeLocalSync(bNodeTree *localtree, bNodeTree *ntree)
{
	if (localtree && ntree) {
		if (ntree->typeinfo->local_sync)
			ntree->typeinfo->local_sync(localtree, ntree);
	}
}

/* merge local tree results back, and free local tree */
/* we have to assume the editor already changed completely */
void ntreeLocalMerge(bNodeTree *localtree, bNodeTree *ntree)
{
	if (ntree && localtree) {
		if (ntree->typeinfo->local_merge)
			ntree->typeinfo->local_merge(localtree, ntree);
		
		ntreeFreeTree_ex(localtree, FALSE);
		MEM_freeN(localtree);
	}
}


/* ************ NODE TREE INTERFACE *************** */

static bNodeSocket *make_socket_interface(bNodeTree *ntree, int in_out,
                                         const char *idname, const char *name)
{
	bNodeSocketType *stype = nodeSocketTypeFind(idname);
	bNodeSocket *sock;
	int own_index = ntree->cur_index++;

	if (stype == NULL) {
		return NULL;
	}

	sock = MEM_callocN(sizeof(bNodeSocket), "socket template");
	BLI_strncpy(sock->idname, stype->idname, sizeof(sock->idname));
	node_socket_set_typeinfo(ntree, sock, stype);
	sock->in_out = in_out;
	sock->type = SOCK_CUSTOM;	/* int type undefined by default */
	
	/* assign new unique index */
	own_index = ntree->cur_index++;
	/* use the own_index as socket identifier */
	if (in_out == SOCK_IN)
		BLI_snprintf(sock->identifier, MAX_NAME, "Input_%d", own_index);
	else
		BLI_snprintf(sock->identifier, MAX_NAME, "Output_%d", own_index);
#ifdef USE_NODE_COMPAT_CUSTOMNODES
	/* XXX forward compatibility:
	 * own_index is deprecated, but needs to be set here.
	 * Node sockets generally use the identifier string instead now,
	 * but reconstructing own_index in writefile.c would require parsing the identifier string.
	 */

#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 406)) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

	sock->own_index = own_index;

#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 406)) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

#endif  /* USE_NODE_COMPAT_CUSTOMNODES */
	
	sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);
	
	BLI_strncpy(sock->name, name, NODE_MAXSTR);
	sock->storage = NULL;
	sock->flag |= SOCK_COLLAPSED;
	
	return sock;
}

bNodeSocket *ntreeFindSocketInterface(bNodeTree *ntree, int in_out, const char *identifier)
{
	bNodeSocket *iosock = (in_out == SOCK_IN ? ntree->inputs.first : ntree->outputs.first);
	for (; iosock; iosock = iosock->next)
		if (STREQ(iosock->identifier, identifier))
			return iosock;
	return NULL;
}

bNodeSocket *ntreeAddSocketInterface(bNodeTree *ntree, int in_out, const char *idname, const char *name)
{
	bNodeSocket *iosock;
	
	iosock = make_socket_interface(ntree, in_out, idname, name);
	if (in_out == SOCK_IN) {
		BLI_addtail(&ntree->inputs, iosock);
		ntree->update |= NTREE_UPDATE_GROUP_IN;
	}
	else if (in_out == SOCK_OUT) {
		BLI_addtail(&ntree->outputs, iosock);
		ntree->update |= NTREE_UPDATE_GROUP_OUT;
	}
	
	return iosock;
}

bNodeSocket *ntreeInsertSocketInterface(bNodeTree *ntree, int in_out, const char *idname,
                               bNodeSocket *next_sock, const char *name)
{
	bNodeSocket *iosock;
	
	iosock = make_socket_interface(ntree, in_out, idname, name);
	if (in_out == SOCK_IN) {
		BLI_insertlinkbefore(&ntree->inputs, next_sock, iosock);
		ntree->update |= NTREE_UPDATE_GROUP_IN;
	}
	else if (in_out == SOCK_OUT) {
		BLI_insertlinkbefore(&ntree->outputs, next_sock, iosock);
		ntree->update |= NTREE_UPDATE_GROUP_OUT;
	}
	
	return iosock;
}

struct bNodeSocket *ntreeAddSocketInterfaceFromSocket(bNodeTree *ntree, bNode *from_node, bNodeSocket *from_sock)
{
	bNodeSocket *iosock = ntreeAddSocketInterface(ntree, from_sock->in_out, from_sock->idname, from_sock->name);
	if (iosock) {
		if (iosock->typeinfo->interface_from_socket)
			iosock->typeinfo->interface_from_socket(ntree, iosock, from_node, from_sock);
	}
	return iosock;
}

struct bNodeSocket *ntreeInsertSocketInterfaceFromSocket(bNodeTree *ntree, bNodeSocket *next_sock, bNode *from_node, bNodeSocket *from_sock)
{
	bNodeSocket *iosock = ntreeInsertSocketInterface(ntree, from_sock->in_out, from_sock->idname, next_sock, from_sock->name);
	if (iosock) {
		if (iosock->typeinfo->interface_from_socket)
			iosock->typeinfo->interface_from_socket(ntree, iosock, from_node, from_sock);
	}
	return iosock;
}

void ntreeRemoveSocketInterface(bNodeTree *ntree, bNodeSocket *sock)
{
	/* this is fast, this way we don't need an in_out argument */
	BLI_remlink(&ntree->inputs, sock);
	BLI_remlink(&ntree->outputs, sock);
	
	node_socket_interface_free(ntree, sock);
	MEM_freeN(sock);
	
	ntree->update |= NTREE_UPDATE_GROUP;
}

/* generates a valid RNA identifier from the node tree name */
static void ntree_interface_identifier_base(bNodeTree *ntree, char *base)
{
	/* generate a valid RNA identifier */
	sprintf(base, "NodeTreeInterface_%s", ntree->id.name + 2);
	RNA_identifier_sanitize(base, FALSE);
}

/* check if the identifier is already in use */
static bool ntree_interface_unique_identifier_check(void *UNUSED(data), const char *identifier)
{
	return (RNA_struct_find(identifier) != NULL);
}

/* generates the actual unique identifier and ui name and description */
static void ntree_interface_identifier(bNodeTree *ntree, const char *base, char *identifier, int maxlen, char *name, char *description)
{
	/* There is a possibility that different node tree names get mapped to the same identifier
	 * after sanitization (e.g. "SomeGroup_A", "SomeGroup.A" both get sanitized to "SomeGroup_A").
	 * On top of the sanitized id string add a number suffix if necessary to avoid duplicates.
	 */
	identifier[0] = '\0';
	BLI_uniquename_cb(ntree_interface_unique_identifier_check, NULL, base, '_', identifier, maxlen);
	
	sprintf(name, "Node Tree %s Interface", ntree->id.name + 2);
	sprintf(description, "Interface properties of node group %s", ntree->id.name + 2);
}

static void ntree_interface_type_create(bNodeTree *ntree)
{
	StructRNA *srna;
	bNodeSocket *sock;
	/* strings are generated from base string + ID name, sizes are sufficient */
	char base[MAX_ID_NAME + 64], identifier[MAX_ID_NAME + 64], name[MAX_ID_NAME + 64], description[MAX_ID_NAME + 64];
	
	/* generate a valid RNA identifier */
	ntree_interface_identifier_base(ntree, base);
	ntree_interface_identifier(ntree, base, identifier, sizeof(identifier), name, description);
	
	/* register a subtype of PropertyGroup */
	srna = RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_PropertyGroup);
	RNA_def_struct_ui_text(srna, name, description);
	RNA_def_struct_duplicate_pointers(srna);
	
	/* associate the RNA type with the node tree */
	ntree->interface_type = srna;
	RNA_struct_blender_type_set(srna, ntree);
	
	/* add socket properties */
	for (sock = ntree->inputs.first; sock; sock = sock->next) {
		bNodeSocketType *stype = sock->typeinfo;
		if (stype && stype->interface_register_properties)
			stype->interface_register_properties(ntree, sock, srna);
	}
	for (sock = ntree->outputs.first; sock; sock = sock->next) {
		bNodeSocketType *stype = sock->typeinfo;
		if (stype && stype->interface_register_properties)
			stype->interface_register_properties(ntree, sock, srna);
	}
}

StructRNA *ntreeInterfaceTypeGet(bNodeTree *ntree, int create)
{
	if (ntree->interface_type) {
		/* strings are generated from base string + ID name, sizes are sufficient */
		char base[MAX_ID_NAME + 64], identifier[MAX_ID_NAME + 64], name[MAX_ID_NAME + 64], description[MAX_ID_NAME + 64];
		
		/* A bit of a hack: when changing the ID name, update the RNA type identifier too,
		 * so that the names match. This is not strictly necessary to keep it working,
		 * but better for identifying associated NodeTree blocks and RNA types.
		 */
		StructRNA *srna = ntree->interface_type;
		
		ntree_interface_identifier_base(ntree, base);
		
		/* RNA identifier may have a number suffix, but should start with the idbase string */
		if (strncmp(RNA_struct_identifier(srna), base, sizeof(base)) != 0) {
			/* generate new unique RNA identifier from the ID name */
			ntree_interface_identifier(ntree, base, identifier, sizeof(identifier), name, description);
			
			/* rename the RNA type */
			RNA_def_struct_free_pointers(srna);
			RNA_def_struct_identifier(srna, identifier);
			RNA_def_struct_ui_text(srna, name, description);
			RNA_def_struct_duplicate_pointers(srna);
		}
	}
	else if (create) {
		ntree_interface_type_create(ntree);
	}
	
	return ntree->interface_type;
}

void ntreeInterfaceTypeFree(bNodeTree *ntree)
{
	if (ntree->interface_type) {
		RNA_struct_free(&BLENDER_RNA, ntree->interface_type);
		ntree->interface_type = NULL;
	}
}

void ntreeInterfaceTypeUpdate(bNodeTree *ntree)
{
	/* XXX it would be sufficient to just recreate all properties
	 * instead of re-registering the whole struct type,
	 * but there is currently no good way to do this in the RNA functions.
	 * Overhead should be negligible.
	 */
	ntreeInterfaceTypeFree(ntree);
	ntree_interface_type_create(ntree);
}


/* ************ find stuff *************** */

bool ntreeHasType(const bNodeTree *ntree, int type)
{
	bNode *node;
	
	if (ntree)
		for (node = ntree->nodes.first; node; node = node->next)
			if (node->type == type)
				return 1;
	return 0;
}

bool ntreeHasTree(const bNodeTree *ntree, const bNodeTree *lookup)
{
	bNode *node;

	if (ntree == lookup)
		return true;

	for (node = ntree->nodes.first; node; node = node->next)
		if (node->type == NODE_GROUP && node->id)
			if (ntreeHasTree((bNodeTree *)node->id, lookup))
				return true;

	return false;
}

bNodeLink *nodeFindLink(bNodeTree *ntree, bNodeSocket *from, bNodeSocket *to)
{
	bNodeLink *link;
	
	for (link = ntree->links.first; link; link = link->next) {
		if (link->fromsock == from && link->tosock == to)
			return link;
		if (link->fromsock == to && link->tosock == from) /* hrms? */
			return link;
	}
	return NULL;
}

int nodeCountSocketLinks(bNodeTree *ntree, bNodeSocket *sock)
{
	bNodeLink *link;
	int tot = 0;
	
	for (link = ntree->links.first; link; link = link->next) {
		if (link->fromsock == sock || link->tosock == sock)
			tot++;
	}
	return tot;
}

bNode *nodeGetActive(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree == NULL) return NULL;
	
	for (node = ntree->nodes.first; node; node = node->next)
		if (node->flag & NODE_ACTIVE)
			break;
	return node;
}

static bNode *node_get_active_id_recursive(bNodeInstanceKey active_key, bNodeInstanceKey parent_key, bNodeTree *ntree, short idtype)
{
	if (parent_key.value == active_key.value || active_key.value == 0) {
		bNode *node;
		for (node = ntree->nodes.first; node; node = node->next)
			if (node->id && GS(node->id->name) == idtype)
				if (node->flag & NODE_ACTIVE_ID)
					return node;
	}
	else {
		bNode *node, *tnode;
		/* no node with active ID in this tree, look inside groups */
		for (node = ntree->nodes.first; node; node = node->next) {
			if (node->type == NODE_GROUP) {
				bNodeTree *group = (bNodeTree *)node->id;
				if (group) {
					bNodeInstanceKey group_key = BKE_node_instance_key(parent_key, ntree, node);
					tnode = node_get_active_id_recursive(active_key, group_key, group, idtype);
					if (tnode)
						return tnode;
				}
			}
		}
	}
	
	return NULL;
}

/* two active flags, ID nodes have special flag for buttons display */
bNode *nodeGetActiveID(bNodeTree *ntree, short idtype)
{
	if (ntree)
		return node_get_active_id_recursive(ntree->active_viewer_key, NODE_INSTANCE_KEY_BASE, ntree, idtype);
	else
		return NULL;
}

bool nodeSetActiveID(bNodeTree *ntree, short idtype, ID *id)
{
	bNode *node;
	bool ok = false;

	if (ntree == NULL) return ok;

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id && GS(node->id->name) == idtype) {
			if (id && ok == FALSE && node->id == id) {
				node->flag |= NODE_ACTIVE_ID;
				ok = TRUE;
			}
			else {
				node->flag &= ~NODE_ACTIVE_ID;
			}
		}
	}

	/* update all groups linked from here
	 * if active ID node has been found already,
	 * just pass NULL so other matching nodes are deactivated.
	 */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP)
			ok |= nodeSetActiveID((bNodeTree *)node->id, idtype, (ok == false ? id : NULL));
	}

	return ok;
}


/* two active flags, ID nodes have special flag for buttons display */
void nodeClearActiveID(bNodeTree *ntree, short idtype)
{
	bNode *node;
	
	if (ntree == NULL) return;
	
	for (node = ntree->nodes.first; node; node = node->next)
		if (node->id && GS(node->id->name) == idtype)
			node->flag &= ~NODE_ACTIVE_ID;
}

void nodeSetSelected(bNode *node, int select)
{
	if (select) {
		node->flag |= NODE_SELECT;
	}
	else {
		bNodeSocket *sock;
		
		node->flag &= ~NODE_SELECT;
		
		/* deselect sockets too */
		for (sock = node->inputs.first; sock; sock = sock->next)
			sock->flag &= ~NODE_SELECT;
		for (sock = node->outputs.first; sock; sock = sock->next)
			sock->flag &= ~NODE_SELECT;
	}
}

void nodeClearActive(bNodeTree *ntree)
{
	bNode *node;

	if (ntree == NULL) return;

	for (node = ntree->nodes.first; node; node = node->next)
		node->flag &= ~(NODE_ACTIVE | NODE_ACTIVE_ID);
}

/* two active flags, ID nodes have special flag for buttons display */
void nodeSetActive(bNodeTree *ntree, bNode *node)
{
	bNode *tnode;
	
	/* make sure only one node is active, and only one per ID type */
	for (tnode = ntree->nodes.first; tnode; tnode = tnode->next) {
		tnode->flag &= ~NODE_ACTIVE;
		
		if (node->id && tnode->id) {
			if (GS(node->id->name) == GS(tnode->id->name))
				tnode->flag &= ~NODE_ACTIVE_ID;
		}
		if (node->typeinfo->nclass == NODE_CLASS_TEXTURE)
			tnode->flag &= ~NODE_ACTIVE_TEXTURE;
	}
	
	node->flag |= NODE_ACTIVE;
	if (node->id)
		node->flag |= NODE_ACTIVE_ID;
	if (node->typeinfo->nclass == NODE_CLASS_TEXTURE)
		node->flag |= NODE_ACTIVE_TEXTURE;
}

int nodeSocketIsHidden(bNodeSocket *sock)
{
	return ((sock->flag & (SOCK_HIDDEN | SOCK_UNAVAIL)) != 0);
}

/* ************** Node Clipboard *********** */

#define USE_NODE_CB_VALIDATE

#ifdef USE_NODE_CB_VALIDATE
/**
 * This data structure is to validate the node on creation,
 * otherwise we may reference missing data.
 *
 * Currently its only used for ID's, but nodes may one day
 * reference other pointers which need validation.
 */
typedef struct bNodeClipboardExtraInfo {
	struct bNodeClipboardExtraInfo *next, *prev;
	ID  *id;
	char id_name[MAX_ID_NAME];
	char library_name[FILE_MAX];
} bNodeClipboardExtraInfo;
#endif  /* USE_NODE_CB_VALIDATE */


typedef struct bNodeClipboard {
	ListBase nodes;

#ifdef USE_NODE_CB_VALIDATE
	ListBase nodes_extra_info;
#endif

	ListBase links;
	int type;
} bNodeClipboard;

static bNodeClipboard node_clipboard = {{NULL}};

void BKE_node_clipboard_init(struct bNodeTree *ntree)
{
	node_clipboard.type = ntree->type;
}

void BKE_node_clipboard_clear(void)
{
	bNode *node, *node_next;
	bNodeLink *link, *link_next;
	
	for (link = node_clipboard.links.first; link; link = link_next) {
		link_next = link->next;
		nodeRemLink(NULL, link);
	}
	BLI_listbase_clear(&node_clipboard.links);
	
	for (node = node_clipboard.nodes.first; node; node = node_next) {
		node_next = node->next;
		node_free_node_ex(NULL, node, false, false);
	}
	BLI_listbase_clear(&node_clipboard.nodes);

#ifdef USE_NODE_CB_VALIDATE
	BLI_freelistN(&node_clipboard.nodes_extra_info);
#endif
}

/* return FALSE when one or more ID's are lost */
bool BKE_node_clipboard_validate(void)
{
	bool ok = true;

#ifdef USE_NODE_CB_VALIDATE
	bNodeClipboardExtraInfo *node_info;
	bNode *node;


	/* lists must be aligned */
	BLI_assert(BLI_countlist(&node_clipboard.nodes) ==
	           BLI_countlist(&node_clipboard.nodes_extra_info));

	for (node = node_clipboard.nodes.first, node_info = node_clipboard.nodes_extra_info.first;
	     node;
	     node = node->next, node_info = node_info->next)
	{
		/* validate the node against the stored node info */

		/* re-assign each loop since we may clear,
		 * open a new file where the ID is valid, and paste again */
		node->id = node_info->id;

		/* currently only validate the ID */
		if (node->id) {
			ListBase *lb = which_libbase(G.main, GS(node_info->id_name));
			BLI_assert(lb != NULL);

			if (BLI_findindex(lb, node_info->id) == -1) {
				/* may assign NULL */
				node->id = BLI_findstring(lb, node_info->id_name + 2, offsetof(ID, name) + 2);

				if (node->id == NULL) {
					ok = false;
				}
			}
		}
	}
#endif  /* USE_NODE_CB_VALIDATE */

	return ok;
}

void BKE_node_clipboard_add_node(bNode *node)
{
#ifdef USE_NODE_CB_VALIDATE
	/* add extra info */
	bNodeClipboardExtraInfo *node_info = MEM_mallocN(sizeof(bNodeClipboardExtraInfo), "bNodeClipboardExtraInfo");

	node_info->id = node->id;
	if (node->id) {
		BLI_strncpy(node_info->id_name, node->id->name, sizeof(node_info->id_name));
		if (node->id->lib) {
			BLI_strncpy(node_info->library_name, node->id->lib->filepath, sizeof(node_info->library_name));
		}
		else {
			node_info->library_name[0] = '\0';
		}
	}
	else {
		node_info->id_name[0] = '\0';
		node_info->library_name[0] = '\0';
	}
	BLI_addtail(&node_clipboard.nodes_extra_info, node_info);
	/* end extra info */
#endif  /* USE_NODE_CB_VALIDATE */

	/* add node */
	BLI_addtail(&node_clipboard.nodes, node);

}

void BKE_node_clipboard_add_link(bNodeLink *link)
{
	BLI_addtail(&node_clipboard.links, link);
}

const ListBase *BKE_node_clipboard_get_nodes(void)
{
	return &node_clipboard.nodes;
}

const ListBase *BKE_node_clipboard_get_links(void)
{
	return &node_clipboard.links;
}

int BKE_node_clipboard_get_type(void)
{
	return node_clipboard.type;
}


/* Node Instance Hash */

/* magic number for initial hash key */
const bNodeInstanceKey NODE_INSTANCE_KEY_BASE = {5381};
const bNodeInstanceKey NODE_INSTANCE_KEY_NONE = {0};

/* Generate a hash key from ntree and node names
 * Uses the djb2 algorithm with xor by Bernstein:
 * http://www.cse.yorku.ca/~oz/hash.html
 */
static bNodeInstanceKey node_hash_int_str(bNodeInstanceKey hash, const char *str)
{
	char c;
	
	while ((c = *str++))
		hash.value = ((hash.value << 5) + hash.value) ^ c; /* (hash * 33) ^ c */
	
	/* separator '\0' character, to avoid ambiguity from concatenated strings */
	hash.value = (hash.value << 5) + hash.value; /* hash * 33 */
	
	return hash;
}

bNodeInstanceKey BKE_node_instance_key(bNodeInstanceKey parent_key, bNodeTree *ntree, bNode *node)
{
	bNodeInstanceKey key;
	
	key = node_hash_int_str(parent_key, ntree->id.name + 2);
	
	if (node)
		key = node_hash_int_str(key, node->name);
	
	return key;
}

static unsigned int node_instance_hash_key(const void *key)
{
	return ((const bNodeInstanceKey *)key)->value;
}

static int node_instance_hash_key_cmp(const void *a, const void *b)
{
	unsigned int value_a = ((const bNodeInstanceKey *)a)->value;
	unsigned int value_b = ((const bNodeInstanceKey *)b)->value;
	if (value_a == value_b)
		return 0;
	else if (value_a < value_b)
		return -1;
	else
		return 1;
}

bNodeInstanceHash *BKE_node_instance_hash_new(const char *info)
{
	bNodeInstanceHash *hash = MEM_mallocN(sizeof(bNodeInstanceHash), info);
	hash->ghash = BLI_ghash_new(node_instance_hash_key, node_instance_hash_key_cmp, "node instance hash ghash");
	return hash;
}

void BKE_node_instance_hash_free(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
	BLI_ghash_free(hash->ghash, NULL, (GHashValFreeFP)valfreefp);
	MEM_freeN(hash);
}

void BKE_node_instance_hash_insert(bNodeInstanceHash *hash, bNodeInstanceKey key, void *value)
{
	bNodeInstanceHashEntry *entry = value;
	entry->key = key;
	entry->tag = 0;
	BLI_ghash_insert(hash->ghash, &entry->key, value);
}

void *BKE_node_instance_hash_lookup(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
	return BLI_ghash_lookup(hash->ghash, &key);
}

int BKE_node_instance_hash_remove(bNodeInstanceHash *hash, bNodeInstanceKey key, bNodeInstanceValueFP valfreefp)
{
	return BLI_ghash_remove(hash->ghash, &key, NULL, (GHashValFreeFP)valfreefp);
}

void BKE_node_instance_hash_clear(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
	BLI_ghash_clear(hash->ghash, NULL, (GHashValFreeFP)valfreefp);
}

void *BKE_node_instance_hash_pop(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
	return BLI_ghash_popkey(hash->ghash, &key, NULL);
}

int BKE_node_instance_hash_haskey(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
	return BLI_ghash_haskey(hash->ghash, &key);
}

int BKE_node_instance_hash_size(bNodeInstanceHash *hash)
{
	return BLI_ghash_size(hash->ghash);
}

void BKE_node_instance_hash_clear_tags(bNodeInstanceHash *hash)
{
	bNodeInstanceHashIterator iter;
	
	NODE_INSTANCE_HASH_ITER(iter, hash) {
		bNodeInstanceHashEntry *value = BKE_node_instance_hash_iterator_get_value(&iter);
		
		value->tag = 0;
	}
}

void BKE_node_instance_hash_tag(bNodeInstanceHash *UNUSED(hash), void *value)
{
	bNodeInstanceHashEntry *entry = value;
	entry->tag = 1;
}

int BKE_node_instance_hash_tag_key(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
	bNodeInstanceHashEntry *entry = BKE_node_instance_hash_lookup(hash, key);
	
	if (entry) {
		entry->tag = 1;
		return TRUE;
	}
	else
		return FALSE;
}

void BKE_node_instance_hash_remove_untagged(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
	/* NOTE: Hash must not be mutated during iterating!
	 * Store tagged entries in a separate list and remove items afterward.
	 */
	bNodeInstanceKey *untagged = MEM_mallocN(sizeof(bNodeInstanceKey) * BKE_node_instance_hash_size(hash), "temporary node instance key list");
	bNodeInstanceHashIterator iter;
	int num_untagged, i;
	
	num_untagged = 0;
	NODE_INSTANCE_HASH_ITER(iter, hash) {
		bNodeInstanceHashEntry *value = BKE_node_instance_hash_iterator_get_value(&iter);
		
		if (!value->tag)
			untagged[num_untagged++] = BKE_node_instance_hash_iterator_get_key(&iter);
	}
	
	for (i = 0; i < num_untagged; ++i) {
		BKE_node_instance_hash_remove(hash, untagged[i], valfreefp);
	}
	
	MEM_freeN(untagged);
}


/* ************** dependency stuff *********** */

/* node is guaranteed to be not checked before */
static int node_get_deplist_recurs(bNodeTree *ntree, bNode *node, bNode ***nsort)
{
	bNode *fromnode;
	bNodeLink *link;
	int level = 0xFFF;
	
	node->done = TRUE;
	
	/* check linked nodes */
	for (link = ntree->links.first; link; link = link->next) {
		if (link->tonode == node) {
			fromnode = link->fromnode;
			if (fromnode->done == 0)
				fromnode->level = node_get_deplist_recurs(ntree, fromnode, nsort);
			if (fromnode->level <= level)
				level = fromnode->level - 1;
		}
	}
	
	/* check parent node */
	if (node->parent) {
		if (node->parent->done == 0)
			node->parent->level = node_get_deplist_recurs(ntree, node->parent, nsort);
		if (node->parent->level <= level)
			level = node->parent->level - 1;
	}
	
	if (nsort) {
		**nsort = node;
		(*nsort)++;
	}
	
	return level;
}

void ntreeGetDependencyList(struct bNodeTree *ntree, struct bNode ***deplist, int *totnodes)
{
	bNode *node, **nsort;
	
	*totnodes = 0;
	
	/* first clear data */
	for (node = ntree->nodes.first; node; node = node->next) {
		node->done = FALSE;
		(*totnodes)++;
	}
	if (*totnodes == 0) {
		*deplist = NULL;
		return;
	}
	
	nsort = *deplist = MEM_callocN((*totnodes) * sizeof(bNode *), "sorted node array");
	
	/* recursive check */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->done == 0) {
			node->level = node_get_deplist_recurs(ntree, node, &nsort);
		}
	}
}

/* only updates node->level for detecting cycles links */
static void ntree_update_node_level(bNodeTree *ntree)
{
	bNode *node;
	
	/* first clear tag */
	for (node = ntree->nodes.first; node; node = node->next) {
		node->done = FALSE;
	}
	
	/* recursive check */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->done == 0) {
			node->level = node_get_deplist_recurs(ntree, node, NULL);
		}
	}
}

static void ntree_update_link_pointers(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	/* first clear data */
	for (node = ntree->nodes.first; node; node = node->next) {
		for (sock = node->inputs.first; sock; sock = sock->next) {
			sock->link = NULL;
			sock->flag &= ~SOCK_IN_USE;
		}
		for (sock = node->outputs.first; sock; sock = sock->next) {
			sock->flag &= ~SOCK_IN_USE;
		}
	}

	for (link = ntree->links.first; link; link = link->next) {
		link->tosock->link = link;
		
		link->fromsock->flag |= SOCK_IN_USE;
		link->tosock->flag |= SOCK_IN_USE;
	}
}

static void ntree_validate_links(bNodeTree *ntree)
{
	bNodeLink *link;
	
	for (link = ntree->links.first; link; link = link->next) {
		link->flag |= NODE_LINK_VALID;
		if (link->fromnode && link->tonode && link->fromnode->level <= link->tonode->level)
			link->flag &= ~NODE_LINK_VALID;
		else if (ntree->typeinfo->validate_link) {
			if (!ntree->typeinfo->validate_link(ntree, link))
				link->flag &= ~NODE_LINK_VALID;
		}
	}
}

void ntreeVerifyNodes(struct Main *main, struct ID *id)
{
	FOREACH_NODETREE(main, ntree, owner_id) {
		bNode *node;
		
		for (node = ntree->nodes.first; node; node = node->next)
			if (node->typeinfo->verifyfunc)
				node->typeinfo->verifyfunc(ntree, node, id);
	} FOREACH_NODETREE_END
}

void ntreeUpdateTree(Main *bmain, bNodeTree *ntree)
{
	bNode *node;
	
	if (!ntree)
		return;
	
	/* avoid reentrant updates, can be caused by RNA update callbacks */
	if (ntree->is_updating)
		return;
	ntree->is_updating = TRUE;
	
	if (ntree->update & (NTREE_UPDATE_LINKS | NTREE_UPDATE_NODES)) {
		/* set the bNodeSocket->link pointers */
		ntree_update_link_pointers(ntree);
	}
	
	/* update individual nodes */
	for (node = ntree->nodes.first; node; node = node->next) {
		/* node tree update tags override individual node update flags */
		if ((node->update & NODE_UPDATE) || (ntree->update & NTREE_UPDATE)) {
			if (node->typeinfo->updatefunc)
				node->typeinfo->updatefunc(ntree, node);
			
			nodeUpdateInternalLinks(ntree, node);
		}
	}
	
	/* generic tree update callback */
	if (ntree->typeinfo->update)
		ntree->typeinfo->update(ntree);
	/* XXX this should be moved into the tree type update callback for tree supporting node groups.
	 * Currently the node tree interface is still a generic feature of the base NodeTree type.
	 */
	if (ntree->update & NTREE_UPDATE_GROUP)
		ntreeInterfaceTypeUpdate(ntree);
	
	/* XXX hack, should be done by depsgraph!! */
	if (bmain)
		ntreeVerifyNodes(bmain, &ntree->id);
	
	if (ntree->update & (NTREE_UPDATE_LINKS | NTREE_UPDATE_NODES)) {
		/* node updates can change sockets or links, repeat link pointer update afterward */
		ntree_update_link_pointers(ntree);
		
		/* update the node level from link dependencies */
		ntree_update_node_level(ntree);
		
		/* check link validity */
		ntree_validate_links(ntree);
	}
	
	/* clear update flags */
	for (node = ntree->nodes.first; node; node = node->next) {
		node->update = 0;
	}
	ntree->update = 0;
	
	ntree->is_updating = FALSE;
}

void nodeUpdate(bNodeTree *ntree, bNode *node)
{
	/* avoid reentrant updates, can be caused by RNA update callbacks */
	if (ntree->is_updating)
		return;
	ntree->is_updating = TRUE;
	
	if (node->typeinfo->updatefunc)
		node->typeinfo->updatefunc(ntree, node);
	
	nodeUpdateInternalLinks(ntree, node);
	
	/* clear update flag */
	node->update = 0;
	
	ntree->is_updating = FALSE;
}

bool nodeUpdateID(bNodeTree *ntree, ID *id)
{
	bNode *node;
	bool changed = false;
	
	if (ELEM(NULL, id, ntree))
		return changed;
	
	/* avoid reentrant updates, can be caused by RNA update callbacks */
	if (ntree->is_updating)
		return changed;
	ntree->is_updating = true;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id == id) {
			changed = true;
			node->update |= NODE_UPDATE_ID;
			if (node->typeinfo->updatefunc)
				node->typeinfo->updatefunc(ntree, node);
			/* clear update flag */
			node->update = 0;
		}
	}
	
	for (node = ntree->nodes.first; node; node = node->next) {
		nodeUpdateInternalLinks(ntree, node);
	}
	
	ntree->is_updating = FALSE;
	return changed;
}

void nodeUpdateInternalLinks(bNodeTree *ntree, bNode *node)
{
	BLI_freelistN(&node->internal_links);
	
	if (node->typeinfo && node->typeinfo->update_internal_links)
		node->typeinfo->update_internal_links(ntree, node);
}


/* nodes that use ID data get synced with local data */
void nodeSynchronizeID(bNode *node, bool copy_to_id)
{
	if (node->id == NULL) return;
	
	if (ELEM(node->type, SH_NODE_MATERIAL, SH_NODE_MATERIAL_EXT)) {
		bNodeSocket *sock;
		Material *ma = (Material *)node->id;
		int a;
		
		/* hrmf, case in loop isn't super fast, but we don't edit 100s of material at same time either! */
		for (a = 0, sock = node->inputs.first; sock; sock = sock->next, a++) {
			if (!nodeSocketIsHidden(sock)) {
				if (copy_to_id) {
					switch (a) {
						case MAT_IN_COLOR:
							copy_v3_v3(&ma->r, ((bNodeSocketValueRGBA *)sock->default_value)->value); break;
						case MAT_IN_SPEC:
							copy_v3_v3(&ma->specr, ((bNodeSocketValueRGBA *)sock->default_value)->value); break;
						case MAT_IN_REFL:
							ma->ref = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
						case MAT_IN_MIR:
							copy_v3_v3(&ma->mirr, ((bNodeSocketValueRGBA *)sock->default_value)->value); break;
						case MAT_IN_AMB:
							ma->amb = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
						case MAT_IN_EMIT:
							ma->emit = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
						case MAT_IN_SPECTRA:
							ma->spectra = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
						case MAT_IN_RAY_MIRROR:
							ma->ray_mirror = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
						case MAT_IN_ALPHA:
							ma->alpha = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
						case MAT_IN_TRANSLUCENCY:
							ma->translucency = ((bNodeSocketValueFloat *)sock->default_value)->value; break;
					}
				}
				else {
					switch (a) {
						case MAT_IN_COLOR:
							copy_v3_v3(((bNodeSocketValueRGBA *)sock->default_value)->value, &ma->r); break;
						case MAT_IN_SPEC:
							copy_v3_v3(((bNodeSocketValueRGBA *)sock->default_value)->value, &ma->specr); break;
						case MAT_IN_REFL:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->ref; break;
						case MAT_IN_MIR:
							copy_v3_v3(((bNodeSocketValueRGBA *)sock->default_value)->value, &ma->mirr); break;
						case MAT_IN_AMB:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->amb; break;
						case MAT_IN_EMIT:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->emit; break;
						case MAT_IN_SPECTRA:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->spectra; break;
						case MAT_IN_RAY_MIRROR:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->ray_mirror; break;
						case MAT_IN_ALPHA:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->alpha; break;
						case MAT_IN_TRANSLUCENCY:
							((bNodeSocketValueFloat *)sock->default_value)->value = ma->translucency; break;
					}
				}
			}
		}
	}
}


/* ************* node type access ********** */

void nodeLabel(bNodeTree *ntree, bNode *node, char *label, int maxlen)
{
	if (node->label[0] != '\0')
		BLI_strncpy(label, node->label, maxlen);
	else if (node->typeinfo->labelfunc)
		node->typeinfo->labelfunc(ntree, node, label, maxlen);
	else
		BLI_strncpy(label, IFACE_(node->typeinfo->ui_name), maxlen);
}

static void node_type_base_defaults(bNodeType *ntype)
{
	/* default size values */
	node_type_size_preset(ntype, NODE_SIZE_DEFAULT);
	ntype->height = 100;
	ntype->minheight = 30;
	ntype->maxheight = FLT_MAX;
}

/* allow this node for any tree type */
static int node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *UNUSED(ntree))
{
	return TRUE;
}

/* use the basic poll function */
static int node_poll_instance_default(bNode *node, bNodeTree *ntree)
{
	return node->typeinfo->poll(node->typeinfo, ntree);
}

void node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
	/* Use static type info header to map static int type to identifier string and RNA struct type.
	 * Associate the RNA struct type with the bNodeType.
	 * Dynamically registered nodes will create an RNA type at runtime
	 * and call RNA_struct_blender_type_set, so this only needs to be done for old RNA types
	 * created in makesrna, which can not be associated to a bNodeType immediately,
	 * since bNodeTypes are registered afterward ...
	 */
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		case ID: \
			BLI_strncpy(ntype->idname, #Category #StructName, sizeof(ntype->idname)); \
			ntype->ext.srna = RNA_struct_find(#Category #StructName); \
			BLI_assert(ntype->ext.srna != NULL); \
			RNA_struct_blender_type_set(ntype->ext.srna, ntype); \
			break;
	
	switch (type) {
#include "NOD_static_types.h"
	}
	
	/* make sure we have a valid type (everything registered) */
	BLI_assert(ntype->idname[0] != '\0');
	
	ntype->type = type;
	BLI_strncpy(ntype->ui_name, name, sizeof(ntype->ui_name));
	ntype->nclass = nclass;
	ntype->flag = flag;

	node_type_base_defaults(ntype);

	ntype->poll = node_poll_default;
	ntype->poll_instance = node_poll_instance_default;
}

void node_type_base_custom(bNodeType *ntype, const char *idname, const char *name, short nclass, short flag)
{
	BLI_strncpy(ntype->idname, idname, sizeof(ntype->idname));
	ntype->type = NODE_CUSTOM;
	BLI_strncpy(ntype->ui_name, name, sizeof(ntype->ui_name));
	ntype->nclass = nclass;
	ntype->flag = flag;

	node_type_base_defaults(ntype);
}

static bool unique_socket_template_identifier_check(void *arg, const char *name)
{
	bNodeSocketTemplate *ntemp;
	struct {bNodeSocketTemplate *list; bNodeSocketTemplate *ntemp;} *data = arg;
	
	for (ntemp = data->list; ntemp->type >= 0; ++ntemp) {
		if (ntemp != data->ntemp) {
			if (STREQ(ntemp->identifier, name)) {
				return true;
			}
		}
	}
	
	return false;
}

static void unique_socket_template_identifier(bNodeSocketTemplate *list, bNodeSocketTemplate *ntemp, const char defname[], char delim)
{
	struct {bNodeSocketTemplate *list; bNodeSocketTemplate *ntemp;} data;
	data.list = list;
	data.ntemp = ntemp;

	BLI_uniquename_cb(unique_socket_template_identifier_check, &data, defname, delim, ntemp->identifier, sizeof(ntemp->identifier));
}

void node_type_socket_templates(struct bNodeType *ntype, struct bNodeSocketTemplate *inputs, struct bNodeSocketTemplate *outputs)
{
	bNodeSocketTemplate *ntemp;
	
	ntype->inputs = inputs;
	ntype->outputs = outputs;
	
	/* automatically generate unique identifiers */
	if (inputs) {
		/* clear identifier strings (uninitialized memory) */
		for (ntemp = inputs; ntemp->type >= 0; ++ntemp)
			ntemp->identifier[0] = '\0';
		
		for (ntemp = inputs; ntemp->type >= 0; ++ntemp) {
			BLI_strncpy(ntemp->identifier, ntemp->name, sizeof(ntemp->identifier));
			unique_socket_template_identifier(inputs, ntemp, ntemp->identifier, '_');
		}
	}
	if (outputs) {
		/* clear identifier strings (uninitialized memory) */
		for (ntemp = outputs; ntemp->type >= 0; ++ntemp)
			ntemp->identifier[0] = '\0';
		
		for (ntemp = outputs; ntemp->type >= 0; ++ntemp) {
			BLI_strncpy(ntemp->identifier, ntemp->name, sizeof(ntemp->identifier));
			unique_socket_template_identifier(outputs, ntemp, ntemp->identifier, '_');
		}
	}
}

void node_type_init(struct bNodeType *ntype, void (*initfunc)(struct bNodeTree *ntree, struct bNode *node))
{
	ntype->initfunc = initfunc;
}

void node_type_size(struct bNodeType *ntype, int width, int minwidth, int maxwidth)
{
	ntype->width = width;
	ntype->minwidth = minwidth;
	if (maxwidth <= minwidth)
		ntype->maxwidth = FLT_MAX;
	else
		ntype->maxwidth = maxwidth;
}

void node_type_size_preset(struct bNodeType *ntype, eNodeSizePreset size)
{
	switch (size) {
		case NODE_SIZE_DEFAULT:
			node_type_size(ntype, 140, 100, 320);
			break;
		case NODE_SIZE_SMALL:
			node_type_size(ntype, 100, 80, 320);
			break;
		case NODE_SIZE_MIDDLE:
			node_type_size(ntype, 150, 120, 320);
			break;
		case NODE_SIZE_LARGE:
			node_type_size(ntype, 240, 140, 320);
			break;
	}
}

void node_type_storage(bNodeType *ntype,
	const char *storagename,
	void (*freefunc)(struct bNode *node),
	void (*copyfunc)(struct bNodeTree *dest_ntree, struct bNode *dest_node, struct bNode *src_node))
{
	if (storagename)
		BLI_strncpy(ntype->storagename, storagename, sizeof(ntype->storagename));
	else
		ntype->storagename[0] = '\0';
	ntype->copyfunc = copyfunc;
	ntype->freefunc = freefunc;
}

void node_type_label(struct bNodeType *ntype, void (*labelfunc)(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen))
{
	ntype->labelfunc = labelfunc;
}

void node_type_update(struct bNodeType *ntype,
                      void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node),
                      void (*verifyfunc)(struct bNodeTree *ntree, struct bNode *node, struct ID *id))
{
	ntype->updatefunc = updatefunc;
	ntype->verifyfunc = verifyfunc;
}

void node_type_exec(struct bNodeType *ntype, NodeInitExecFunction initexecfunc, NodeFreeExecFunction freeexecfunc, NodeExecFunction execfunc)
{
	ntype->initexecfunc = initexecfunc;
	ntype->freeexecfunc = freeexecfunc;
	ntype->execfunc = execfunc;
}

void node_type_gpu(struct bNodeType *ntype, NodeGPUExecFunction gpufunc)
{
	ntype->gpufunc = gpufunc;
}

void node_type_internal_links(bNodeType *ntype, void (*update_internal_links)(bNodeTree *, bNode *))
{
	ntype->update_internal_links = update_internal_links;
}

void node_type_compatibility(struct bNodeType *ntype, short compatibility)
{
	ntype->compatibility = compatibility;
}

/* callbacks for undefined types */

static int node_undefined_poll(bNodeType *UNUSED(ntype), bNodeTree *UNUSED(nodetree))
{
	/* this type can not be added deliberately, it's just a placeholder */
	return false;
}

/* register fallback types used for undefined tree, nodes, sockets */
static void register_undefined_types(void)
{
	/* Note: these types are not registered in the type hashes,
	 * they are just used as placeholders in case the actual types are not registered.
	 */
	
	strcpy(NodeTreeTypeUndefined.idname, "NodeTreeUndefined");
	strcpy(NodeTreeTypeUndefined.ui_name, "Undefined");
	strcpy(NodeTreeTypeUndefined.ui_description, "Undefined Node Tree Type");
	
	node_type_base_custom(&NodeTypeUndefined, "NodeUndefined", "Undefined", 0, 0);
	NodeTypeUndefined.poll = node_undefined_poll;
	
	BLI_strncpy(NodeSocketTypeUndefined.idname, "NodeSocketUndefined", sizeof(NodeSocketTypeUndefined.idname));
	/* extra type info for standard socket types */
	NodeSocketTypeUndefined.type = SOCK_CUSTOM;
	NodeSocketTypeUndefined.subtype = PROP_NONE;
}

static void registerCompositNodes(void)
{
	register_node_type_cmp_group();
	
	register_node_type_cmp_rlayers();
	register_node_type_cmp_image();
	register_node_type_cmp_texture();
	register_node_type_cmp_value();
	register_node_type_cmp_rgb();
	register_node_type_cmp_curve_time();
	register_node_type_cmp_movieclip();
	
	register_node_type_cmp_composite();
	register_node_type_cmp_viewer();
	register_node_type_cmp_splitviewer();
	register_node_type_cmp_output_file();
	register_node_type_cmp_view_levels();
	
	register_node_type_cmp_curve_rgb();
	register_node_type_cmp_mix_rgb();
	register_node_type_cmp_hue_sat();
	register_node_type_cmp_brightcontrast();
	register_node_type_cmp_gamma();
	register_node_type_cmp_invert();
	register_node_type_cmp_alphaover();
	register_node_type_cmp_zcombine();
	register_node_type_cmp_colorbalance();
	register_node_type_cmp_huecorrect();
	
	register_node_type_cmp_normal();
	register_node_type_cmp_curve_vec();
	register_node_type_cmp_map_value();
	register_node_type_cmp_map_range();
	register_node_type_cmp_normalize();
	
	register_node_type_cmp_filter();
	register_node_type_cmp_blur();
	register_node_type_cmp_dblur();
	register_node_type_cmp_bilateralblur();
	register_node_type_cmp_vecblur();
	register_node_type_cmp_dilateerode();
	register_node_type_cmp_inpaint();
	register_node_type_cmp_despeckle();
	register_node_type_cmp_defocus();
	
	register_node_type_cmp_valtorgb();
	register_node_type_cmp_rgbtobw();
	register_node_type_cmp_setalpha();
	register_node_type_cmp_idmask();
	register_node_type_cmp_math();
	register_node_type_cmp_seprgba();
	register_node_type_cmp_combrgba();
	register_node_type_cmp_sephsva();
	register_node_type_cmp_combhsva();
	register_node_type_cmp_sepyuva();
	register_node_type_cmp_combyuva();
	register_node_type_cmp_sepycca();
	register_node_type_cmp_combycca();
	register_node_type_cmp_premulkey();
	
	register_node_type_cmp_diff_matte();
	register_node_type_cmp_distance_matte();
	register_node_type_cmp_chroma_matte();
	register_node_type_cmp_color_matte();
	register_node_type_cmp_channel_matte();
	register_node_type_cmp_color_spill();
	register_node_type_cmp_luma_matte();
	register_node_type_cmp_doubleedgemask();
	register_node_type_cmp_keyingscreen();
	register_node_type_cmp_keying();

	register_node_type_cmp_translate();
	register_node_type_cmp_rotate();
	register_node_type_cmp_scale();
	register_node_type_cmp_flip();
	register_node_type_cmp_crop();
	register_node_type_cmp_displace();
	register_node_type_cmp_mapuv();
	register_node_type_cmp_glare();
	register_node_type_cmp_tonemap();
	register_node_type_cmp_lensdist();
	register_node_type_cmp_transform();
	register_node_type_cmp_stabilize2d();
	register_node_type_cmp_moviedistortion();

	register_node_type_cmp_colorcorrection();
	register_node_type_cmp_boxmask();
	register_node_type_cmp_ellipsemask();
	register_node_type_cmp_bokehimage();
	register_node_type_cmp_bokehblur();
	register_node_type_cmp_switch();
	register_node_type_cmp_pixelate();

	register_node_type_cmp_mask();
	register_node_type_cmp_trackpos();
	register_node_type_cmp_planetrackdeform();
}

static void registerShaderNodes(void) 
{
	register_node_type_sh_group();

	register_node_type_sh_output();
	register_node_type_sh_material();
	register_node_type_sh_camera();
	register_node_type_sh_lamp();
	register_node_type_sh_gamma();
	register_node_type_sh_brightcontrast();
	register_node_type_sh_value();
	register_node_type_sh_rgb();
	register_node_type_sh_wireframe();
	register_node_type_sh_wavelength();
	register_node_type_sh_blackbody();
	register_node_type_sh_mix_rgb();
	register_node_type_sh_valtorgb();
	register_node_type_sh_rgbtobw();
	register_node_type_sh_texture();
	register_node_type_sh_normal();
	register_node_type_sh_geom();
	register_node_type_sh_mapping();
	register_node_type_sh_curve_vec();
	register_node_type_sh_curve_rgb();
	register_node_type_sh_math();
	register_node_type_sh_vect_math();
	register_node_type_sh_vect_transform();
	register_node_type_sh_squeeze();
	register_node_type_sh_material_ext();
	register_node_type_sh_invert();
	register_node_type_sh_seprgb();
	register_node_type_sh_combrgb();
	register_node_type_sh_sephsv();
	register_node_type_sh_combhsv();
	register_node_type_sh_hue_sat();

	register_node_type_sh_attribute();
	register_node_type_sh_geometry();
	register_node_type_sh_light_path();
	register_node_type_sh_light_falloff();
	register_node_type_sh_object_info();
	register_node_type_sh_fresnel();
	register_node_type_sh_layer_weight();
	register_node_type_sh_tex_coord();
	register_node_type_sh_particle_info();
	register_node_type_sh_bump();

	register_node_type_sh_background();
	register_node_type_sh_bsdf_anisotropic();
	register_node_type_sh_bsdf_diffuse();
	register_node_type_sh_bsdf_glossy();
	register_node_type_sh_bsdf_glass();
	register_node_type_sh_bsdf_translucent();
	register_node_type_sh_bsdf_transparent();
	register_node_type_sh_bsdf_velvet();
	register_node_type_sh_bsdf_toon();
	register_node_type_sh_bsdf_hair();
	register_node_type_sh_emission();
	register_node_type_sh_holdout();
	register_node_type_sh_volume_absorption();
	register_node_type_sh_volume_scatter();
	register_node_type_sh_subsurface_scattering();
	register_node_type_sh_mix_shader();
	register_node_type_sh_add_shader();

	register_node_type_sh_output_lamp();
	register_node_type_sh_output_material();
	register_node_type_sh_output_world();

	register_node_type_sh_tex_image();
	register_node_type_sh_tex_environment();
	register_node_type_sh_tex_sky();
	register_node_type_sh_tex_noise();
	register_node_type_sh_tex_wave();
	register_node_type_sh_tex_voronoi();
	register_node_type_sh_tex_musgrave();
	register_node_type_sh_tex_gradient();
	register_node_type_sh_tex_magic();
	register_node_type_sh_tex_checker();
	register_node_type_sh_tex_brick();
}

static void registerTextureNodes(void)
{
	register_node_type_tex_group();

	
	register_node_type_tex_math();
	register_node_type_tex_mix_rgb();
	register_node_type_tex_valtorgb();
	register_node_type_tex_rgbtobw();
	register_node_type_tex_valtonor();
	register_node_type_tex_curve_rgb();
	register_node_type_tex_curve_time();
	register_node_type_tex_invert();
	register_node_type_tex_hue_sat();
	register_node_type_tex_coord();
	register_node_type_tex_distance();
	register_node_type_tex_compose();
	register_node_type_tex_decompose();
	
	register_node_type_tex_output();
	register_node_type_tex_viewer();
	register_node_type_sh_script();
	register_node_type_sh_tangent();
	register_node_type_sh_normal_map();
	register_node_type_sh_hair_info();
	
	register_node_type_tex_checker();
	register_node_type_tex_texture();
	register_node_type_tex_bricks();
	register_node_type_tex_image();
	register_node_type_sh_bsdf_refraction();
	register_node_type_sh_ambient_occlusion();
	
	register_node_type_tex_rotate();
	register_node_type_tex_translate();
	register_node_type_tex_scale();
	register_node_type_tex_at();
	
	register_node_type_tex_proc_voronoi();
	register_node_type_tex_proc_blend();
	register_node_type_tex_proc_magic();
	register_node_type_tex_proc_marble();
	register_node_type_tex_proc_clouds();
	register_node_type_tex_proc_wood();
	register_node_type_tex_proc_musgrave();
	register_node_type_tex_proc_noise();
	register_node_type_tex_proc_stucci();
	register_node_type_tex_proc_distnoise();
}

void init_nodesystem(void) 
{
	nodetreetypes_hash = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "nodetreetypes_hash gh");
	nodetypes_hash = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "nodetypes_hash gh");
	nodesockettypes_hash = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "nodesockettypes_hash gh");

	register_undefined_types();

	register_standard_node_socket_types();

	register_node_tree_type_cmp();
	register_node_tree_type_sh();
	register_node_tree_type_tex();

	register_node_type_frame();
	register_node_type_reroute();
	register_node_type_group_input();
	register_node_type_group_output();
	
	registerCompositNodes();
	registerShaderNodes();
	registerTextureNodes();
}

void free_nodesystem(void) 
{
	if (nodetypes_hash) {
		NODE_TYPES_BEGIN(nt)
			if (nt->ext.free) {
				nt->ext.free(nt->ext.data);
			}
		NODE_TYPES_END

		BLI_ghash_free(nodetypes_hash, NULL, node_free_type);
		nodetypes_hash = NULL;
	}

	if (nodesockettypes_hash) {
		NODE_SOCKET_TYPES_BEGIN(st)
			if (st->ext_socket.free)
				st->ext_socket.free(st->ext_socket.data);
			if (st->ext_interface.free)
				st->ext_interface.free(st->ext_interface.data);
		NODE_SOCKET_TYPES_END

		BLI_ghash_free(nodesockettypes_hash, NULL, node_free_socket_type);
		nodesockettypes_hash = NULL;
	}

	if (nodetreetypes_hash) {
		NODE_TREE_TYPES_BEGIN (nt)
		{
			if (nt->ext.free) {
				nt->ext.free(nt->ext.data);
			}
		}
		NODE_TREE_TYPES_END;

		BLI_ghash_free(nodetreetypes_hash, NULL, ntree_free_type);
		nodetreetypes_hash = NULL;
	}
}


/* -------------------------------------------------------------------- */
/* NodeTree Iterator Helpers (FOREACH_NODETREE) */

void BKE_node_tree_iter_init(struct NodeTreeIterStore *ntreeiter, struct Main *bmain)
{
	ntreeiter->ngroup = bmain->nodetree.first;
	ntreeiter->scene = bmain->scene.first;
	ntreeiter->mat = bmain->mat.first;
	ntreeiter->tex = bmain->tex.first;
	ntreeiter->lamp = bmain->lamp.first;
	ntreeiter->world = bmain->world.first;
}
bool BKE_node_tree_iter_step(struct NodeTreeIterStore *ntreeiter,
                             bNodeTree **r_nodetree, struct ID **r_id)
{
	if (ntreeiter->ngroup) {
		*r_nodetree =       ntreeiter->ngroup;
		*r_id       = (ID *)ntreeiter->ngroup;
		ntreeiter->ngroup = ntreeiter->ngroup->id.next;
	}
	else if (ntreeiter->scene) {
		*r_nodetree =       ntreeiter->scene->nodetree;
		*r_id       = (ID *)ntreeiter->scene;
		ntreeiter->scene =  ntreeiter->scene->id.next;
	}
	else if (ntreeiter->mat) {
		*r_nodetree =       ntreeiter->mat->nodetree;
		*r_id       = (ID *)ntreeiter->mat;
		ntreeiter->mat =    ntreeiter->mat->id.next;
	}
	else if (ntreeiter->tex) {
		*r_nodetree =       ntreeiter->tex->nodetree;
		*r_id       = (ID *)ntreeiter->tex;
		ntreeiter->tex =    ntreeiter->tex->id.next;
	}
	else if (ntreeiter->lamp) {
		*r_nodetree =       ntreeiter->lamp->nodetree;
		*r_id       = (ID *)ntreeiter->lamp;
		ntreeiter->lamp =   ntreeiter->lamp->id.next;
	}
	else if (ntreeiter->world) {
		*r_nodetree =       ntreeiter->world->nodetree;
		*r_id       = (ID *)ntreeiter->world;
		ntreeiter->world  = ntreeiter->world->id.next;
	}
	else {
		return false;
	}

	return true;
}
