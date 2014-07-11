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
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"

#include "RNA_access.h"

#include "GPU_material.h"

#include "RE_shader_ext.h"

#include "node_common.h"
#include "node_exec.h"
#include "node_util.h"
#include "node_shader_util.h"

static int shader_tree_poll(const bContext *C, bNodeTreeType *UNUSED(treetype))
{
	Scene *scene = CTX_data_scene(C);
	/* allow empty engine string too, this is from older versions that didn't have registerable engines yet */
	return (scene->r.engine[0] == '\0' ||
	        STREQ(scene->r.engine, "BLENDER_RENDER") ||
	        STREQ(scene->r.engine, "BLENDER_GAME") ||
	        STREQ(scene->r.engine, "CYCLES"));
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
		FreestyleLineStyle *linestyle = CTX_data_linestyle_from_scene(scene);
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

void ntreeGPUMaterialNodes(bNodeTree *ntree, GPUMaterial *mat, short compatibility)
{
	/* localize tree to create links for reroute and mute */
	bNodeTree *localtree = ntreeLocalize(ntree);
	bNodeTreeExec *exec;

	exec = ntreeShaderBeginExecTree(localtree);
	ntreeExecGPUNodes(exec, mat, 1, compatibility);
	ntreeShaderEndExecTree(exec);

	ntreeFreeTree_ex(localtree, false);
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
