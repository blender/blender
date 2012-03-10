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
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "GPU_material.h"

#include "RE_shader_ext.h"

#include "node_exec.h"
#include "node_util.h"
#include "node_shader_util.h"

static void foreach_nodetree(Main *main, void *calldata, bNodeTreeCallback func)
{
	Material *ma;
	Lamp *la;
	World *wo;

	for(ma= main->mat.first; ma; ma= ma->id.next)
		if(ma->nodetree)
			func(calldata, &ma->id, ma->nodetree);

	for(la= main->lamp.first; la; la= la->id.next)
		if(la->nodetree)
			func(calldata, &la->id, la->nodetree);

	for(wo= main->world.first; wo; wo= wo->id.next)
		if(wo->nodetree)
			func(calldata, &wo->id, wo->nodetree);
}

static void foreach_nodeclass(Scene *scene, void *calldata, bNodeClassCallback func)
{
	func(calldata, NODE_CLASS_INPUT, IFACE_("Input"));
	func(calldata, NODE_CLASS_OUTPUT, IFACE_("Output"));

	if(scene_use_new_shading_nodes(scene)) {
		func(calldata, NODE_CLASS_SHADER, IFACE_("Shader"));
		func(calldata, NODE_CLASS_TEXTURE, IFACE_("Texture"));
	}

	func(calldata, NODE_CLASS_OP_COLOR, IFACE_("Color"));
	func(calldata, NODE_CLASS_OP_VECTOR, IFACE_("Vector"));
	func(calldata, NODE_CLASS_CONVERTOR, IFACE_("Convertor"));
	func(calldata, NODE_CLASS_GROUP, IFACE_("Group"));
	func(calldata, NODE_CLASS_LAYOUT, IFACE_("Layout"));
}

static void localize(bNodeTree *localtree, bNodeTree *UNUSED(ntree))
{
	bNode *node, *node_next;
	
	/* replace muted nodes by internal links */
	for (node= localtree->nodes.first; node; node= node_next) {
		node_next = node->next;
		
		if (node->flag & NODE_MUTED) {
			nodeInternalRelink(localtree, node);
			nodeFreeNode(localtree, node);
		}
	}
}

static void local_sync(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *lnode;
	
	/* copy over contents of previews */
	for(lnode= localtree->nodes.first; lnode; lnode= lnode->next) {
		if(ntreeNodeExists(ntree, lnode->new_node)) {
			bNode *node= lnode->new_node;
			
			if(node->preview && node->preview->rect) {
				if(lnode->preview && lnode->preview->rect) {
					int xsize= node->preview->xsize;
					int ysize= node->preview->ysize;
					memcpy(node->preview->rect, lnode->preview->rect, 4*xsize + xsize*ysize*sizeof(char)*4);
				}
			}
		}
	}
}

static void update(bNodeTree *ntree)
{
	ntreeSetOutput(ntree);
}

bNodeTreeType ntreeType_Shader = {
	/* type */				NTREE_SHADER,
	/* id_name */			"NTShader Nodetree",
	
	/* node_types */		{ NULL, NULL },
	
	/* free_cache */		NULL,
	/* free_node_cache */	NULL,
	/* foreach_nodetree */	foreach_nodetree,
	/* foreach_nodeclass */	foreach_nodeclass,
	/* localize */			localize,
	/* local_sync */		local_sync,
	/* local_merge */		NULL,
	/* update */			update,
	/* update_node */		NULL,
	/* validate_link */		NULL,
	/* internal_connect */	node_internal_connect_default
};

/* GPU material from shader nodes */

void ntreeGPUMaterialNodes(bNodeTree *ntree, GPUMaterial *mat)
{
	bNodeTreeExec *exec;

	exec = ntreeShaderBeginExecTree(ntree, 1);

	ntreeExecGPUNodes(exec, mat, 1);

	ntreeShaderEndExecTree(exec, 1);
}

/* **************** call to switch lamploop for material node ************ */

void (*node_shader_lamp_loop)(struct ShadeInput *, struct ShadeResult *);

void set_node_shader_lamp_loop(void (*lamp_loop_func)(ShadeInput *, ShadeResult *))
{
	node_shader_lamp_loop= lamp_loop_func;
}


/* XXX Group nodes must set use_tree_data to false, since their trees can be shared by multiple nodes.
 * If use_tree_data is true, the ntree->execdata pointer is checked to avoid multiple execution of top-level trees.
 */
bNodeTreeExec *ntreeShaderBeginExecTree(bNodeTree *ntree, int use_tree_data)
{
	bNodeTreeExec *exec;
	bNode *node;
	
	if (use_tree_data) {
		/* XXX hack: prevent exec data from being generated twice.
		 * this should be handled by the renderer!
		 */
		if (ntree->execdata)
			return ntree->execdata;
	}
	
	/* ensures only a single output node is enabled */
	ntreeSetOutput(ntree);
	
	/* common base initialization */
	exec = ntree_exec_begin(ntree);
	
	/* allocate the thread stack listbase array */
	exec->threadstack= MEM_callocN(BLENDER_MAX_THREADS*sizeof(ListBase), "thread stack array");
	
	for(node= exec->nodetree->nodes.first; node; node= node->next)
		node->need_exec= 1;
	
	if (use_tree_data) {
		/* XXX this should not be necessary, but is still used for cmp/sha/tex nodes,
		 * which only store the ntree pointer. Should be fixed at some point!
		 */
		ntree->execdata = exec;
	}
	
	return exec;
}

/* XXX Group nodes must set use_tree_data to false, since their trees can be shared by multiple nodes.
 * If use_tree_data is true, the ntree->execdata pointer is checked to avoid multiple execution of top-level trees.
 */
void ntreeShaderEndExecTree(bNodeTreeExec *exec, int use_tree_data)
{
	if(exec) {
		bNodeTree *ntree= exec->nodetree;
		bNodeThreadStack *nts;
		int a;
		
		if(exec->threadstack) {
			for(a=0; a<BLENDER_MAX_THREADS; a++) {
				for(nts=exec->threadstack[a].first; nts; nts=nts->next)
					if (nts->stack) MEM_freeN(nts->stack);
				BLI_freelistN(&exec->threadstack[a]);
			}
			
			MEM_freeN(exec->threadstack);
			exec->threadstack= NULL;
		}
		
		ntree_exec_end(exec);
		
		if (use_tree_data) {
			/* XXX clear nodetree backpointer to exec data, same problem as noted in ntreeBeginExecTree */
			ntree->execdata = NULL;
		}
	}
}

void ntreeShaderExecTree(bNodeTree *ntree, ShadeInput *shi, ShadeResult *shr)
{
	ShaderCallData scd;
	/**
	 * \note: preserve material from ShadeInput for material id, nodetree execs change it
	 * fix for bug "[#28012] Mat ID messy with shader nodes"
	 */
	Material *mat = shi->mat;
	bNodeThreadStack *nts = NULL;
	bNodeTreeExec *exec = ntree->execdata;
	
	/* convert caller data to struct */
	scd.shi= shi;
	scd.shr= shr;
	
	/* each material node has own local shaderesult, with optional copying */
	memset(shr, 0, sizeof(ShadeResult));
	
	/* ensure execdata is only initialized once */
	if (!exec) {
		BLI_lock_thread(LOCK_NODES);
		if(!ntree->execdata)
			ntree->execdata = ntreeShaderBeginExecTree(ntree, 1);
		BLI_unlock_thread(LOCK_NODES);

		exec = ntree->execdata;
	}
	
	nts= ntreeGetThreadStack(exec, shi->thread);
	ntreeExecThreadNodes(exec, nts, &scd, shi->thread);
	ntreeReleaseThreadStack(nts);
	
	// \note: set material back to preserved material
	shi->mat = mat;
	/* better not allow negative for now */
	if(shr->combined[0]<0.0f) shr->combined[0]= 0.0f;
	if(shr->combined[1]<0.0f) shr->combined[1]= 0.0f;
	if(shr->combined[2]<0.0f) shr->combined[2]= 0.0f;
}
