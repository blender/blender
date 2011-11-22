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

/** \file blender/nodes/texture/node_texture_tree.c
 *  \ingroup nodes
 */


#include <string.h>

#include "DNA_texture_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "node_exec.h"
#include "node_util.h"
#include "NOD_texture.h"
#include "node_texture_util.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"


static void foreach_nodetree(Main *main, void *calldata, bNodeTreeCallback func)
{
	Tex *tx;
	for(tx= main->tex.first; tx; tx= tx->id.next) {
		if(tx->nodetree) {
			func(calldata, &tx->id, tx->nodetree);
		}
	}
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
	func(calldata, NODE_CLASS_INPUT, IFACE_("Input"));
	func(calldata, NODE_CLASS_OUTPUT, IFACE_("Output"));
	func(calldata, NODE_CLASS_OP_COLOR, IFACE_("Color"));
	func(calldata, NODE_CLASS_PATTERN, IFACE_("Patterns"));
	func(calldata, NODE_CLASS_TEXTURE, IFACE_("Textures"));
	func(calldata, NODE_CLASS_CONVERTOR, IFACE_("Convertor"));
	func(calldata, NODE_CLASS_DISTORT, IFACE_("Distort"));
	func(calldata, NODE_CLASS_GROUP, IFACE_("Group"));
	func(calldata, NODE_CLASS_LAYOUT, IFACE_("Layout"));
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

bNodeTreeType ntreeType_Texture = {
	/* type */				NTREE_TEXTURE,
	/* id_name */			"NTTexture Nodetree",
	
	/* node_types */		{ NULL, NULL },
	
	/* free_cache */			NULL,
	/* free_node_cache */		NULL,
	/* foreach_nodetree */	foreach_nodetree,
	/* foreach_nodeclass */	foreach_nodeclass,
	/* localize */			NULL,
	/* local_sync */		local_sync,
	/* local_merge */		NULL,
	/* update */			NULL,
	/* update_node */		NULL,
	/* validate_link */		NULL,
	/* mute node */			node_tex_pass_on,
	/* mute links node */	node_mute_get_links,
	/* gpu mute node */		NULL
};

int ntreeTexTagAnimated(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return 0;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==TEX_NODE_CURVE_TIME) {
			nodeUpdate(ntree, node);
			return 1;
		}
		else if(node->type==NODE_GROUP) {
			if( ntreeTexTagAnimated((bNodeTree *)node->id) ) {
				return 1;
			}
		}
	}
	
	return 0;
}

/* XXX Group nodes must set use_tree_data to false, since their trees can be shared by multiple nodes.
 * If use_tree_data is true, the ntree->execdata pointer is checked to avoid multiple execution of top-level trees.
 */
bNodeTreeExec *ntreeTexBeginExecTree(bNodeTree *ntree, int use_tree_data)
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

/* free texture delegates */
static void tex_free_delegates(bNodeTreeExec *exec)
{
	bNodeThreadStack *nts;
	bNodeStack *ns;
	int th, a;
	
	for(th=0; th<BLENDER_MAX_THREADS; th++)
		for(nts=exec->threadstack[th].first; nts; nts=nts->next)
			for(ns= nts->stack, a=0; a<exec->stacksize; a++, ns++)
				if(ns->data && !ns->is_copy)
					MEM_freeN(ns->data);
}

/* XXX Group nodes must set use_tree_data to false, since their trees can be shared by multiple nodes.
 * If use_tree_data is true, the ntree->execdata pointer is checked to avoid multiple execution of top-level trees.
 */
void ntreeTexEndExecTree(bNodeTreeExec *exec, int use_tree_data)
{
	if(exec) {
		bNodeTree *ntree= exec->nodetree;
		bNodeThreadStack *nts;
		int a;
		
		if(exec->threadstack) {
			tex_free_delegates(exec);
			
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

int ntreeTexExecTree(
	bNodeTree *nodes,
	TexResult *texres,
	float *co,
	float *dxt, float *dyt,
	int osatex,
	short thread, 
	Tex *UNUSED(tex), 
	short which_output, 
	int cfra,
	int preview,
	ShadeInput *shi,
	MTex *mtex
){
	TexCallData data;
	float *nor= texres->nor;
	int retval = TEX_INT;
	bNodeThreadStack *nts = NULL;
	bNodeTreeExec *exec= nodes->execdata;

	data.co = co;
	data.dxt = dxt;
	data.dyt = dyt;
	data.osatex = osatex;
	data.target = texres;
	data.do_preview = preview;
	data.thread = thread;
	data.which_output = which_output;
	data.cfra= cfra;
	data.mtex= mtex;
	data.shi= shi;
	
	/* ensure execdata is only initialized once */
	if (!exec) {
		BLI_lock_thread(LOCK_NODES);
		if(!nodes->execdata)
			ntreeTexBeginExecTree(nodes, 1);
		BLI_unlock_thread(LOCK_NODES);

		exec= nodes->execdata;
	}
	
	nts= ntreeGetThreadStack(exec, thread);
	ntreeExecThreadNodes(exec, nts, &data, thread);
	ntreeReleaseThreadStack(nts);

	if(texres->nor) retval |= TEX_NOR;
	retval |= TEX_RGB;
	/* confusing stuff; the texture output node sets this to NULL to indicate no normal socket was set
	   however, the texture code checks this for other reasons (namely, a normal is required for material) */
	texres->nor= nor;

	return retval;
}
