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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/node_shader_tree.c
 *  \ingroup nodes
 */


#include <string.h>

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"
#include "DNA_linestyle_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_linestyle.h"
#include "BKE_node.h"
#include "BKE_scene.h"

#include "RNA_access.h"

#include "GPU_material.h"

#include "RE_shader_ext.h"

#include "NOD_common.h"

#include "node_common.h"
#include "node_exec.h"
#include "node_util.h"
#include "node_shader_util.h"

static int shader_tree_poll(const bContext *C, bNodeTreeType *UNUSED(treetype))
{
	Scene *scene = CTX_data_scene(C);
	/* allow empty engine string too, this is from older versions that didn't have registerable engines yet */
	return (scene->r.engine[0] == '\0' ||
	        STREQ(scene->r.engine, RE_engine_id_BLENDER_RENDER) ||
	        STREQ(scene->r.engine, RE_engine_id_BLENDER_GAME) ||
	        STREQ(scene->r.engine, RE_engine_id_CYCLES) ||
	        !BKE_scene_use_shading_nodes_custom(scene));
}

static void shader_get_from_context(const bContext *C, bNodeTreeType *UNUSED(treetype), bNodeTree **r_ntree, ID **r_id, ID **r_from)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	
	if ((snode->shaderfrom == SNODE_SHADER_OBJECT) ||
	    (BKE_scene_use_new_shading_nodes(scene) == false))
	{
		if (ob) {
			*r_from = &ob->id;
			if (ob->type == OB_LAMP) {
				*r_id = ob->data;
				*r_ntree = ((Lamp *)ob->data)->nodetree;
			}
			else {
				Material *ma = give_current_material(ob, ob->actcol);
				if (ma) {
					*r_id = &ma->id;
					*r_ntree = ma->nodetree;
				}
			}
		}
	}
#ifdef WITH_FREESTYLE
	else if (snode->shaderfrom == SNODE_SHADER_LINESTYLE) {
		FreestyleLineStyle *linestyle = BKE_linestyle_active_from_scene(scene);
		if (linestyle) {
			*r_from = NULL;
			*r_id = &linestyle->id;
			*r_ntree = linestyle->nodetree;
		}
	}
#endif
	else { /* SNODE_SHADER_WORLD */
		if (scene->world) {
			*r_from = NULL;
			*r_id = &scene->world->id;
			*r_ntree = scene->world->nodetree;
		}
	}
}

static void foreach_nodeclass(Scene *scene, void *calldata, bNodeClassCallback func)
{
	func(calldata, NODE_CLASS_INPUT, N_("Input"));
	func(calldata, NODE_CLASS_OUTPUT, N_("Output"));

	if (BKE_scene_use_new_shading_nodes(scene)) {
		func(calldata, NODE_CLASS_SHADER, N_("Shader"));
		func(calldata, NODE_CLASS_TEXTURE, N_("Texture"));
	}
	
	func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
	func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
	func(calldata, NODE_CLASS_CONVERTOR, N_("Convertor"));
	func(calldata, NODE_CLASS_SCRIPT, N_("Script"));
	func(calldata, NODE_CLASS_GROUP, N_("Group"));
	func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
	func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static void localize(bNodeTree *localtree, bNodeTree *UNUSED(ntree))
{
	bNode *node, *node_next;
	
	/* replace muted nodes and reroute nodes by internal links */
	for (node = localtree->nodes.first; node; node = node_next) {
		node_next = node->next;
		
		if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
			nodeInternalRelink(localtree, node);
			nodeFreeNode(localtree, node);
		}
	}
}

static void local_sync(bNodeTree *localtree, bNodeTree *ntree)
{
	BKE_node_preview_sync_tree(ntree, localtree);
}

static void local_merge(bNodeTree *localtree, bNodeTree *ntree)
{
	BKE_node_preview_merge_tree(ntree, localtree, true);
}

static void update(bNodeTree *ntree)
{
	ntreeSetOutput(ntree);
	
	ntree_update_reroute_nodes(ntree);
	
	if (ntree->update & NTREE_UPDATE_NODES) {
		/* clean up preview cache, in case nodes have been removed */
		BKE_node_preview_remove_unused(ntree);
	}
}

bNodeTreeType *ntreeType_Shader;

void register_node_tree_type_sh(void)
{
	bNodeTreeType *tt = ntreeType_Shader = MEM_callocN(sizeof(bNodeTreeType), "shader node tree type");
	
	tt->type = NTREE_SHADER;
	strcpy(tt->idname, "ShaderNodeTree");
	strcpy(tt->ui_name, "Shader");
	tt->ui_icon = 0;    /* defined in drawnode.c */
	strcpy(tt->ui_description, "Shader nodes");
	
	tt->foreach_nodeclass = foreach_nodeclass;
	tt->localize = localize;
	tt->local_sync = local_sync;
	tt->local_merge = local_merge;
	tt->update = update;
	tt->poll = shader_tree_poll;
	tt->get_from_context = shader_get_from_context;
	
	tt->ext.srna = &RNA_ShaderNodeTree;
	
	ntreeTypeAdd(tt);
}

/* GPU material from shader nodes */

static void ntree_shader_link_builtin_normal(bNodeTree *ntree,
                                             bNode *node_from,
                                             bNodeSocket *socket_from,
                                             bNode *displacement_node,
                                             bNodeSocket *displacement_socket);

/* Find an output node of the shader tree.
 *
 * NOTE: it will only return output which is NOT in the group, which isn't how
 * render engines works but it's how the GPU shader compilation works. This we
 * can change in the future and make it a generic function, but for now it stays
 * private here.
 */
static bNode *ntree_shader_output_node(bNodeTree *ntree)
{
	/* Make sure we only have single node tagged as output. */
	ntreeSetOutput(ntree);
	for (bNode *node = ntree->nodes.first; node != NULL; node = node->next) {
		if (node->flag & NODE_DO_OUTPUT) {
			return node;
		}
	}
	return NULL;
}

/* Find socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_socket(ListBase *sockets,
                                                  const char *identifier)
{
	for (bNodeSocket *sock = sockets->first; sock != NULL; sock = sock->next) {
		if (STREQ(sock->identifier, identifier)) {
			return sock;
		}
	}
	return NULL;
}

/* Find input socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_input(bNode *node,
                                                 const char *identifier)
{
	return ntree_shader_node_find_socket(&node->inputs, identifier);
}

/* Find output socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_output(bNode *node,
                                                  const char *identifier)
{
	return ntree_shader_node_find_socket(&node->outputs, identifier);
}

/* Check whether shader has a displacement.
 *
 * Will also return a node and it's socket which is connected to a displacement
 * output. Additionally, link which is attached to the displacement output is
 * also returned.
 */
static bool ntree_shader_has_displacement(bNodeTree *ntree,
                                          bNode **r_node,
                                          bNodeSocket **r_socket,
                                          bNodeLink **r_link)
{
	bNode *output_node = ntree_shader_output_node(ntree);
	if (output_node == NULL) {
		/* We can't have displacement without output node, apparently. */
		return false;
	}
	/* Make sure sockets links pointers are correct. */
	ntreeUpdateTree(G.main, ntree);
	bNodeSocket *displacement = ntree_shader_node_find_input(output_node,
	                                                         "Displacement");

	if (displacement == NULL) {
		/* Non-cycles node is used as an output. */
		return false;
	}
	if (displacement->link != NULL) {
		*r_node = displacement->link->fromnode;
		*r_socket = displacement->link->fromsock;
		*r_link = displacement->link;
	}
	return displacement->link != NULL;
}

static bool ntree_shader_relink_node_normal(bNodeTree *ntree,
                                            bNode *node,
                                            bNode *node_from,
                                            bNodeSocket *socket_from)
{
	bNodeSocket *sock = ntree_shader_node_find_input(node, "Normal");
	/* TODO(sergey): Can we do something smarter here than just a name-based
	 * matching?
	 */
	if (sock == NULL) {
		/* There's no Normal input, nothing to link. */
		return false;
	}
	if (sock->link != NULL) {
		/* Something is linked to the normal input already. can't
		 * use other input for that.
		 */
		return false;
	}
	/* Create connection between specified node and the normal input. */
	nodeAddLink(ntree, node_from, socket_from, node, sock);
	return true;
}

static void ntree_shader_link_builtin_group_normal(
        bNodeTree *ntree,
        bNode *group_node,
        bNode *node_from,
        bNodeSocket *socket_from,
        bNode *displacement_node,
        bNodeSocket *displacement_socket)
{
	bNodeTree *group_ntree = (bNodeTree *)group_node->id;
	/* Create input socket to plug displacement connection to. */
	bNodeSocket *group_normal_socket =
	        ntreeAddSocketInterface(group_ntree,
	                                SOCK_IN,
	                                "NodeSocketVector",
	                                "Normal");
	/* Need to update tree so all node instances nodes gets proper sockets. */
	bNode *group_input_node = ntreeFindType(group_ntree, NODE_GROUP_INPUT);
	node_group_verify(ntree, group_node, &group_ntree->id);
	if (group_input_node)
		node_group_input_verify(group_ntree, group_input_node, &group_ntree->id);
	ntreeUpdateTree(G.main, group_ntree);
	/* Assumes sockets are always added at the end. */
	bNodeSocket *group_node_normal_socket = group_node->inputs.last;
	if (displacement_node == group_node) {
		/* If displacement is coming from this node group we need to perform
		 * some internal re-linking in order to avoid cycles.
		 */
		bNode *group_output_node = ntreeFindType(group_ntree, NODE_GROUP_OUTPUT);
		if (group_output_node == NULL) {
			return;
		}
		bNodeSocket *group_output_node_displacement_socket =
		        nodeFindSocket(group_output_node,
		                       SOCK_IN,
		                       displacement_socket->identifier);
		bNodeLink *group_displacement_link = group_output_node_displacement_socket->link;
		if (group_displacement_link == NULL) {
			/* Displacement output is not connected to anything, can just stop
			 * right away.
			 */
			return;
		}
		/* This code is similar to ntree_shader_relink_displacement() */
		bNode *group_displacement_node = group_displacement_link->fromnode;
		bNodeSocket *group_displacement_socket = group_displacement_link->fromsock;
		/* Create and link bump node.
		 * Can't re-use bump node from parent tree because it'll cause cycle.
		 */
		bNode *bump_node = nodeAddStaticNode(NULL, group_ntree, SH_NODE_BUMP);
		bNodeSocket *bump_input_socket = ntree_shader_node_find_input(bump_node, "Height");
		bNodeSocket *bump_output_socket = ntree_shader_node_find_output(bump_node, "Normal");
		BLI_assert(bump_input_socket != NULL);
		BLI_assert(bump_output_socket != NULL);
		nodeAddLink(group_ntree,
		            group_displacement_node, group_displacement_socket,
		            bump_node, bump_input_socket);
		/* Relink normals inside of the instanced tree. */
		ntree_shader_link_builtin_normal(group_ntree,
		                                 bump_node,
		                                 bump_output_socket,
		                                 group_displacement_node,
		                                 group_displacement_socket);
		ntreeUpdateTree(G.main, group_ntree);
	}
	else if (group_input_node) {
		/* Connect group node normal input. */
		nodeAddLink(ntree,
		            node_from, socket_from,
		            group_node, group_node_normal_socket);
		BLI_assert(group_input_node != NULL);
		bNodeSocket *group_input_node_normal_socket =
		        nodeFindSocket(group_input_node,
		                       SOCK_OUT,
		                       group_normal_socket->identifier);
		BLI_assert(group_input_node_normal_socket != NULL);
		/* Relink normals inside of the instanced tree. */
		ntree_shader_link_builtin_normal(group_ntree,
		                                 group_input_node,
		                                 group_input_node_normal_socket,
		                                 displacement_node,
		                                 displacement_socket);
		ntreeUpdateTree(G.main, group_ntree);
	}
}

/* Use specified node and socket as an input for unconnected normal sockets. */
static void ntree_shader_link_builtin_normal(bNodeTree *ntree,
                                             bNode *node_from,
                                             bNodeSocket *socket_from,
                                             bNode *displacement_node,
                                             bNodeSocket *displacement_socket)
{
	for (bNode *node = ntree->nodes.first; node != NULL; node = node->next) {
		if (node == node_from) {
			/* Don't connect node itself! */
			continue;
		}
		if (node->type == NODE_GROUP && node->id) {
			/* Special re-linking for group nodes. */
			ntree_shader_link_builtin_group_normal(ntree,
			                                       node,
			                                       node_from,
			                                       socket_from,
			                                       displacement_node,
			                                       displacement_socket);
			continue;
		}
		if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
			/* Group inputs and outputs needs nothing special. */
			continue;
		}
		ntree_shader_relink_node_normal(ntree, node, node_from, socket_from);
	}
}

/* Re-link displacement output to unconnected normal sockets via bump node.
 * This way material with have proper displacement in the viewport.
 */
static void ntree_shader_relink_displacement(bNodeTree *ntree,
                                             short compatibility)
{
	if (compatibility != NODE_NEW_SHADING) {
		/* We can only deal with new shading system here. */
		return;
	}
	bNode *displacement_node;
	bNodeSocket *displacement_socket;
	bNodeLink *displacement_link;
	if (!ntree_shader_has_displacement(ntree,
	                                   &displacement_node,
	                                   &displacement_socket,
	                                   &displacement_link))
	{
		/* There is no displacement output connected, nothing to re-link. */
		return;
	}
	/* We have to disconnect displacement output socket, otherwise we'll have
	 * cycles in the Cycles material :)
	 */
	nodeRemLink(ntree, displacement_link);
	/* We can't connect displacement to normal directly, use bump node for that
	 * and hope that it gives good enough approximation.
	 */
	bNode *bump_node = nodeAddStaticNode(NULL, ntree, SH_NODE_BUMP);
	bNodeSocket *bump_input_socket = ntree_shader_node_find_input(bump_node, "Height");
	bNodeSocket *bump_output_socket = ntree_shader_node_find_output(bump_node, "Normal");
	BLI_assert(bump_input_socket != NULL);
	BLI_assert(bump_output_socket != NULL);
	/* Connect bump node to where displacement output was originally
	 * connected to.
	 */
	nodeAddLink(ntree,
	            displacement_node, displacement_socket,
	            bump_node, bump_input_socket);
	/* Connect all free-standing Normal inputs. */
	ntree_shader_link_builtin_normal(ntree,
	                                 bump_node,
	                                 bump_output_socket,
	                                 displacement_node,
	                                 displacement_socket);
	/* TODO(sergey): Reconnect Geometry Info->Normal sockets to the new
	 * bump node.
	 */
	/* We modified the tree, it needs to be updated now. */
	ntreeUpdateTree(G.main, ntree);
}

void ntreeGPUMaterialNodes(bNodeTree *ntree, GPUMaterial *mat, short compatibility)
{
	/* localize tree to create links for reroute and mute */
	bNodeTree *localtree = ntreeLocalize(ntree);
	bNodeTreeExec *exec;

	/* Perform all needed modifications on the tree in order to support
	 * displacement/bump mapping.
	 */
	ntree_shader_relink_displacement(localtree, compatibility);

	exec = ntreeShaderBeginExecTree(localtree);
	ntreeExecGPUNodes(exec, mat, 1, compatibility);
	ntreeShaderEndExecTree(exec);

	ntreeFreeTree(localtree);
	MEM_freeN(localtree);
}

/* **************** call to switch lamploop for material node ************ */

void (*node_shader_lamp_loop)(struct ShadeInput *, struct ShadeResult *);

void set_node_shader_lamp_loop(void (*lamp_loop_func)(ShadeInput *, ShadeResult *))
{
	node_shader_lamp_loop = lamp_loop_func;
}


bNodeTreeExec *ntreeShaderBeginExecTree_internal(bNodeExecContext *context, bNodeTree *ntree, bNodeInstanceKey parent_key)
{
	bNodeTreeExec *exec;
	bNode *node;
	
	/* ensures only a single output node is enabled */
	ntreeSetOutput(ntree);
	
	/* common base initialization */
	exec = ntree_exec_begin(context, ntree, parent_key);
	
	/* allocate the thread stack listbase array */
	exec->threadstack = MEM_callocN(BLENDER_MAX_THREADS * sizeof(ListBase), "thread stack array");
	
	for (node = exec->nodetree->nodes.first; node; node = node->next)
		node->need_exec = 1;
	
	return exec;
}

bNodeTreeExec *ntreeShaderBeginExecTree(bNodeTree *ntree)
{
	bNodeExecContext context;
	bNodeTreeExec *exec;
	
	/* XXX hack: prevent exec data from being generated twice.
	 * this should be handled by the renderer!
	 */
	if (ntree->execdata)
		return ntree->execdata;
	
	context.previews = ntree->previews;
	
	exec = ntreeShaderBeginExecTree_internal(&context, ntree, NODE_INSTANCE_KEY_BASE);
	
	/* XXX this should not be necessary, but is still used for cmp/sha/tex nodes,
	 * which only store the ntree pointer. Should be fixed at some point!
	 */
	ntree->execdata = exec;
	
	return exec;
}

void ntreeShaderEndExecTree_internal(bNodeTreeExec *exec)
{
	bNodeThreadStack *nts;
	int a;
	
	if (exec->threadstack) {
		for (a = 0; a < BLENDER_MAX_THREADS; a++) {
			for (nts = exec->threadstack[a].first; nts; nts = nts->next)
				if (nts->stack) MEM_freeN(nts->stack);
			BLI_freelistN(&exec->threadstack[a]);
		}
		
		MEM_freeN(exec->threadstack);
		exec->threadstack = NULL;
	}
	
	ntree_exec_end(exec);
}

void ntreeShaderEndExecTree(bNodeTreeExec *exec)
{
	if (exec) {
		/* exec may get freed, so assign ntree */
		bNodeTree *ntree = exec->nodetree;
		ntreeShaderEndExecTree_internal(exec);
		
		/* XXX clear nodetree backpointer to exec data, same problem as noted in ntreeBeginExecTree */
		ntree->execdata = NULL;
	}
}

/* only for Blender internal */
bool ntreeShaderExecTree(bNodeTree *ntree, ShadeInput *shi, ShadeResult *shr)
{
	ShaderCallData scd;
	/**
	 * \note: preserve material from ShadeInput for material id, nodetree execs change it
	 * fix for bug "[#28012] Mat ID messy with shader nodes"
	 */
	Material *mat = shi->mat;
	bNodeThreadStack *nts = NULL;
	bNodeTreeExec *exec = ntree->execdata;
	int compat;
	
	/* convert caller data to struct */
	scd.shi = shi;
	scd.shr = shr;
	
	/* each material node has own local shaderesult, with optional copying */
	memset(shr, 0, sizeof(ShadeResult));
	
	/* ensure execdata is only initialized once */
	if (!exec) {
		BLI_lock_thread(LOCK_NODES);
		if (!ntree->execdata)
			ntree->execdata = ntreeShaderBeginExecTree(ntree);
		BLI_unlock_thread(LOCK_NODES);

		exec = ntree->execdata;
	}
	
	nts = ntreeGetThreadStack(exec, shi->thread);
	compat = ntreeExecThreadNodes(exec, nts, &scd, shi->thread);
	ntreeReleaseThreadStack(nts);
	
	// \note: set material back to preserved material
	shi->mat = mat;
		
	/* better not allow negative for now */
	if (shr->combined[0] < 0.0f) shr->combined[0] = 0.0f;
	if (shr->combined[1] < 0.0f) shr->combined[1] = 0.0f;
	if (shr->combined[2] < 0.0f) shr->combined[2] = 0.0f;
	
	/* if compat is zero, it has been using non-compatible nodes */
	return compat;
}
