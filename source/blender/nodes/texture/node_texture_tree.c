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
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_paint.h"

#include "node_common.h"
#include "node_exec.h"
#include "node_util.h"
#include "NOD_texture.h"
#include "node_texture_util.h"

#include "RNA_access.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"


static void texture_get_from_context(const bContext *C, bNodeTreeType *UNUSED(treetype), bNodeTree **r_ntree, ID **r_id, ID **r_from)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	Tex *tx = NULL;

	if (snode->texfrom == SNODE_TEX_OBJECT) {
		if (ob) {
			tx = give_current_object_texture(ob);
			if (tx) {
				if (ob->type == OB_LAMP)
					*r_from = (ID *)ob->data;
				else
					*r_from = (ID *)give_current_material(ob, ob->actcol);
				
				/* from is not set fully for material nodes, should be ID + Node then */
				*r_id = &tx->id;
				*r_ntree = tx->nodetree;
			}
		}
	}
	else if (snode->texfrom == SNODE_TEX_WORLD) {
		if (scene->world) {
			*r_from = (ID *)scene->world;
			tx = give_current_world_texture(scene->world);
			if (tx) {
				*r_id = &tx->id;
				*r_ntree = tx->nodetree;
			}
		}
	}
	else if (snode->texfrom == SNODE_TEX_BRUSH) {
		struct Brush *brush = NULL;
		
		if (ob && (ob->mode & OB_MODE_SCULPT))
			brush = BKE_paint_brush(&scene->toolsettings->sculpt->paint);
		else
			brush = BKE_paint_brush(&scene->toolsettings->imapaint.paint);

		if (brush) {
			*r_from = (ID *)brush;
			tx = give_current_brush_texture(brush);
			if (tx) {
				*r_id = &tx->id;
				*r_ntree = tx->nodetree;
			}
		}
	}
	else if (snode->texfrom == SNODE_TEX_LINESTYLE) {
		FreestyleLineStyle *linestyle = BKE_linestyle_active_from_scene(scene);
		if (linestyle) {
			*r_from = (ID *)linestyle;
			tx = give_current_linestyle_texture(linestyle);
			if (tx) {
				*r_id = &tx->id;
				*r_ntree = tx->nodetree;
			}
		}
	}
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
	func(calldata, NODE_CLASS_INPUT, N_("Input"));
	func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
	func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
	func(calldata, NODE_CLASS_PATTERN, N_("Patterns"));
	func(calldata, NODE_CLASS_TEXTURE, N_("Textures"));
	func(calldata, NODE_CLASS_CONVERTOR, N_("Convertor"));
	func(calldata, NODE_CLASS_DISTORT, N_("Distort"));
	func(calldata, NODE_CLASS_GROUP, N_("Group"));
	func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
	func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

/* XXX muting disabled in previews because of threading issues with the main execution
 * it works here, but disabled for consistency
 */
#if 1
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
#else
static void localize(bNodeTree *UNUSED(localtree), bNodeTree *UNUSED(ntree))
{
}
#endif

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
	ntree_update_reroute_nodes(ntree);
	
	if (ntree->update & NTREE_UPDATE_NODES) {
		/* clean up preview cache, in case nodes have been removed */
		BKE_node_preview_remove_unused(ntree);
	}
}

bNodeTreeType *ntreeType_Texture;

void register_node_tree_type_tex(void)
{
	bNodeTreeType *tt = ntreeType_Texture = MEM_callocN(sizeof(bNodeTreeType), "texture node tree type");
	
	tt->type = NTREE_TEXTURE;
	strcpy(tt->idname, "TextureNodeTree");
	strcpy(tt->ui_name, "Texture");
	tt->ui_icon = 0;    /* defined in drawnode.c */
	strcpy(tt->ui_description, "Texture nodes");
	
	tt->foreach_nodeclass = foreach_nodeclass;
	tt->update = update;
	tt->localize = localize;
	tt->local_sync = local_sync;
	tt->local_merge = local_merge;
	tt->get_from_context = texture_get_from_context;
	
	tt->ext.srna = &RNA_TextureNodeTree;
	
	ntreeTypeAdd(tt);
}

int ntreeTexTagAnimated(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree == NULL) return 0;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == TEX_NODE_CURVE_TIME) {
			nodeUpdate(ntree, node);
			return 1;
		}
		else if (node->type == NODE_GROUP) {
			if (ntreeTexTagAnimated((bNodeTree *)node->id) ) {
				return 1;
			}
		}
	}
	
	return 0;
}

bNodeTreeExec *ntreeTexBeginExecTree_internal(bNodeExecContext *context, bNodeTree *ntree, bNodeInstanceKey parent_key)
{
	bNodeTreeExec *exec;
	bNode *node;
	
	/* common base initialization */
	exec = ntree_exec_begin(context, ntree, parent_key);
	
	/* allocate the thread stack listbase array */
	exec->threadstack = MEM_callocN(BLENDER_MAX_THREADS * sizeof(ListBase), "thread stack array");
	
	for (node = exec->nodetree->nodes.first; node; node = node->next)
		node->need_exec = 1;
	
	return exec;
}

bNodeTreeExec *ntreeTexBeginExecTree(bNodeTree *ntree)
{
	bNodeExecContext context;
	bNodeTreeExec *exec;
	
	/* XXX hack: prevent exec data from being generated twice.
	 * this should be handled by the renderer!
	 */
	if (ntree->execdata)
		return ntree->execdata;
	
	context.previews = ntree->previews;
	
	exec = ntreeTexBeginExecTree_internal(&context, ntree, NODE_INSTANCE_KEY_BASE);
	
	/* XXX this should not be necessary, but is still used for cmp/sha/tex nodes,
	 * which only store the ntree pointer. Should be fixed at some point!
	 */
	ntree->execdata = exec;
	
	return exec;
}

/* free texture delegates */
static void tex_free_delegates(bNodeTreeExec *exec)
{
	bNodeThreadStack *nts;
	bNodeStack *ns;
	int th, a;
	
	for (th = 0; th < BLENDER_MAX_THREADS; th++)
		for (nts = exec->threadstack[th].first; nts; nts = nts->next)
			for (ns = nts->stack, a = 0; a < exec->stacksize; a++, ns++)
				if (ns->data && !ns->is_copy)
					MEM_freeN(ns->data);
}

void ntreeTexEndExecTree_internal(bNodeTreeExec *exec)
{
	bNodeThreadStack *nts;
	int a;
	
	if (exec->threadstack) {
		tex_free_delegates(exec);
		
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

void ntreeTexEndExecTree(bNodeTreeExec *exec)
{
	if (exec) {
		/* exec may get freed, so assign ntree */
		bNodeTree *ntree = exec->nodetree;
		ntreeTexEndExecTree_internal(exec);
		
		/* XXX clear nodetree backpointer to exec data, same problem as noted in ntreeBeginExecTree */
		ntree->execdata = NULL;
	}
}

int ntreeTexExecTree(
        bNodeTree *nodes,
        TexResult *texres,
        float co[3],
        float dxt[3], float dyt[3],
        int osatex,
        const short thread,
        Tex *UNUSED(tex),
        short which_output,
        int cfra,
        int preview,
        ShadeInput *shi,
        MTex *mtex)
{
	TexCallData data;
	float *nor = texres->nor;
	int retval = TEX_INT;
	bNodeThreadStack *nts = NULL;
	bNodeTreeExec *exec = nodes->execdata;

	data.co = co;
	data.dxt = dxt;
	data.dyt = dyt;
	data.osatex = osatex;
	data.target = texres;
	data.do_preview = preview;
	data.do_manage = (shi) ? shi->do_manage : 0;
	data.thread = thread;
	data.which_output = which_output;
	data.cfra = cfra;
	data.mtex = mtex;
	data.shi = shi;
	
	/* ensure execdata is only initialized once */
	if (!exec) {
		BLI_lock_thread(LOCK_NODES);
		if (!nodes->execdata)
			ntreeTexBeginExecTree(nodes);
		BLI_unlock_thread(LOCK_NODES);

		exec = nodes->execdata;
	}
	
	nts = ntreeGetThreadStack(exec, thread);
	ntreeExecThreadNodes(exec, nts, &data, thread);
	ntreeReleaseThreadStack(nts);

	if (texres->nor) retval |= TEX_NOR;
	retval |= TEX_RGB;
	/* confusing stuff; the texture output node sets this to NULL to indicate no normal socket was set
	 * however, the texture code checks this for other reasons (namely, a normal is required for material) */
	texres->nor = nor;

	return retval;
}
