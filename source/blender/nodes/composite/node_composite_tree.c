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

/** \file blender/nodes/composite/node_composite_tree.c
 *  \ingroup nodes
 */


#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "BLF_translation.h"

#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_tracking.h"
#include "BKE_utildefines.h"

#include "node_exec.h"
#include "node_util.h"

#include "PIL_time.h"

#include "RNA_access.h"

#include "NOD_composite.h"
#include "node_composite_util.h"

#ifdef WITH_COMPOSITOR
	#include "COM_compositor.h"
#endif

static void foreach_nodetree(Main *main, void *calldata, bNodeTreeCallback func)
{
	Scene *sce;
	for (sce= main->scene.first; sce; sce= sce->id.next) {
		if (sce->nodetree) {
			func(calldata, &sce->id, sce->nodetree);
		}
	}
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
	func(calldata, NODE_CLASS_INPUT, N_("Input"));
	func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
	func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
	func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
	func(calldata, NODE_CLASS_OP_FILTER, N_("Filter"));
	func(calldata, NODE_CLASS_CONVERTOR, N_("Convertor"));
	func(calldata, NODE_CLASS_MATTE, N_("Matte"));
	func(calldata, NODE_CLASS_DISTORT, N_("Distort"));
	func(calldata, NODE_CLASS_GROUP, N_("Group"));
	func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static void free_node_cache(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *sock;
	
	for (sock= node->outputs.first; sock; sock= sock->next) {
		if (sock->cache) {
			free_compbuf(sock->cache);
			sock->cache= NULL;
		}
	}
}

static void free_cache(bNodeTree *ntree)
{
	bNode *node;
	for (node= ntree->nodes.first; node; node= node->next)
		free_node_cache(ntree, node);
}

static void update_node(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock;

	for (sock= node->outputs.first; sock; sock= sock->next) {
		if (sock->cache) {
			//free_compbuf(sock->cache);
			//sock->cache= NULL;
		}
	}
	node->need_exec= 1;
	/* individual node update call */
	if (node->typeinfo->updatefunc)
		node->typeinfo->updatefunc(ntree, node);
}

/* local tree then owns all compbufs */
static void localize(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *node, *node_next;
	bNodeSocket *sock;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		/* ensure new user input gets handled ok */
		node->need_exec= 0;
		node->new_node->original = node;
		
		/* move over the compbufs */
		/* right after ntreeCopyTree() oldsock pointers are valid */
		
		if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
			if (node->id) {
				if (node->flag & NODE_DO_OUTPUT)
					node->new_node->id= (ID *)node->id;
				else
					node->new_node->id= NULL;
			}
		}
		
		/* copy over the preview buffers to update graduatly */
		if (node->preview) {
			bNodePreview *preview = MEM_callocN(sizeof(bNodePreview), "Preview");
			preview->pad = node->preview->pad;
			preview->xsize = node->preview->xsize;
			preview->ysize = node->preview->ysize;
			preview->rect = MEM_dupallocN(node->preview->rect);
			node->new_node->preview = preview;
		}
		
		for (sock= node->outputs.first; sock; sock= sock->next) {
			sock->new_sock->cache= sock->cache;
			compbuf_set_node(sock->new_sock->cache, node->new_node);
			
			sock->cache= NULL;
			sock->new_sock->new_sock= sock;
		}
	}
	
	/* replace muted nodes and reroute nodes by internal links */
	for (node= localtree->nodes.first; node; node= node_next) {
		node_next = node->next;
		
		if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
			/* make sure the update tag isn't lost when removing the muted node.
			 * propagate this to all downstream nodes.
			 */
			if (node->need_exec) {
				bNodeLink *link;
				for (link=localtree->links.first; link; link=link->next)
					if (link->fromnode==node && link->tonode)
						link->tonode->need_exec = 1;
			}
			
			nodeInternalRelink(localtree, node);
			nodeFreeNode(localtree, node);
		}
	}
}

static void local_sync(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *lnode;
	
	/* move over the compbufs and previews */
	for (lnode= localtree->nodes.first; lnode; lnode= lnode->next) {
		if ( (lnode->exec & NODE_READY) && !(lnode->exec & NODE_SKIPPED) ) {
			if (ntreeNodeExists(ntree, lnode->new_node)) {
				
				if (lnode->preview && lnode->preview->rect) {
					nodeFreePreview(lnode->new_node);
					lnode->new_node->preview= lnode->preview;
					lnode->preview= NULL;
				}
				
			}
		}
	}
}

static void local_merge(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *lnode;
	bNodeSocket *lsock;
	
	/* move over the compbufs and previews */
	for (lnode= localtree->nodes.first; lnode; lnode= lnode->next) {
		if (ntreeNodeExists(ntree, lnode->new_node)) {
			if (ELEM(lnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				if (lnode->id && (lnode->flag & NODE_DO_OUTPUT)) {
					/* image_merge does sanity check for pointers */
					BKE_image_merge((Image *)lnode->new_node->id, (Image *)lnode->id);
				}
			}
			else if (lnode->type==CMP_NODE_MOVIEDISTORTION) {
				/* special case for distortion node: distortion context is allocating in exec function
				 * and to achive much better performance on further calls this context should be
				 * copied back to original node */
				if (lnode->storage) {
					if (lnode->new_node->storage)
						BKE_tracking_distortion_free(lnode->new_node->storage);

					lnode->new_node->storage= BKE_tracking_distortion_copy(lnode->storage);
				}
			}
			
			for (lsock= lnode->outputs.first; lsock; lsock= lsock->next) {
				if (ntreeOutputExists(lnode->new_node, lsock->new_sock)) {
					lsock->new_sock->cache= lsock->cache;
					compbuf_set_node(lsock->new_sock->cache, lnode->new_node);
					lsock->cache= NULL;
					lsock->new_sock= NULL;
				}
			}
		}
	}
}

static void update(bNodeTree *ntree)
{
	ntreeSetOutput(ntree);
}

bNodeTreeType ntreeType_Composite = {
	/* type */				NTREE_COMPOSIT,
	/* idname */			"NTCompositing Nodetree",
	
	/* node_types */		{ NULL, NULL },
	
	/* free_cache */		free_cache,
	/* free_node_cache */	free_node_cache,
	/* foreach_nodetree */	foreach_nodetree,
	/* foreach_nodeclass */	foreach_nodeclass,
	/* localize */			localize,
	/* local_sync */		local_sync,
	/* local_merge */		local_merge,
	/* update */			update,
	/* update_node */		update_node,
	/* validate_link */		NULL,
	/* internal_connect */	node_internal_connect_default
};


/* XXX Group nodes must set use_tree_data to false, since their trees can be shared by multiple nodes.
 * If use_tree_data is true, the ntree->execdata pointer is checked to avoid multiple execution of top-level trees.
 */
struct bNodeTreeExec *ntreeCompositBeginExecTree(bNodeTree *ntree, int use_tree_data)
{
	bNodeTreeExec *exec;
	bNode *node;
	bNodeSocket *sock;
	
	if (use_tree_data) {
		/* XXX hack: prevent exec data from being generated twice.
		 * this should be handled by the renderer!
		 */
		if (ntree->execdata)
			return ntree->execdata;
	}
	
	/* ensures only a single output node is enabled */
	ntreeSetOutput(ntree);
	
	exec = ntree_exec_begin(ntree);
	
	for (node= exec->nodetree->nodes.first; node; node= node->next) {
		/* initialize needed for groups */
		node->exec= 0;	
		
		for (sock= node->outputs.first; sock; sock= sock->next) {
			bNodeStack *ns= node_get_socket_stack(exec->stack, sock);
			if (ns && sock->cache) {
				ns->data= sock->cache;
				sock->cache= NULL;
			}
		}
		/* cannot initialize them while using in threads */
		if (ELEM4(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB, CMP_NODE_HUECORRECT)) {
			curvemapping_initialize(node->storage);
			if (node->type==CMP_NODE_CURVE_RGB)
				curvemapping_premultiply(node->storage, 0);
		}
	}
	
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
void ntreeCompositEndExecTree(bNodeTreeExec *exec, int use_tree_data)
{
	if (exec) {
		bNodeTree *ntree= exec->nodetree;
		bNode *node;
		bNodeStack *ns;
		
		for (node= exec->nodetree->nodes.first; node; node= node->next) {
			bNodeSocket *sock;
			
			for (sock= node->outputs.first; sock; sock= sock->next) {
				ns = node_get_socket_stack(exec->stack, sock);
				if (ns && ns->data) {
					sock->cache= ns->data;
					ns->data= NULL;
				}
			}
			if (node->type==CMP_NODE_CURVE_RGB)
				curvemapping_premultiply(node->storage, 1);
			
			node->need_exec= 0;
		}
	
		ntree_exec_end(exec);
		
		if (use_tree_data) {
			/* XXX clear nodetree backpointer to exec data, same problem as noted in ntreeBeginExecTree */
			ntree->execdata = NULL;
		}
	}
}

#ifdef WITH_COMPOSITOR

/* ***************************** threaded version for execute composite nodes ************* */
/* these are nodes without input, only giving values */
/* or nodes with only value inputs */
static int node_only_value(bNode *node)
{
	bNodeSocket *sock;
	
	if (ELEM3(node->type, CMP_NODE_TIME, CMP_NODE_VALUE, CMP_NODE_RGB))
		return 1;
	
	/* doing this for all node types goes wrong. memory free errors */
	if (node->inputs.first && node->type==CMP_NODE_MAP_VALUE) {
		int retval= 1;
		for (sock= node->inputs.first; sock; sock= sock->next) {
			if (sock->link)
				retval &= node_only_value(sock->link->fromnode);
		}
		return retval;
	}
	return 0;
}

/* not changing info, for thread callback */
typedef struct ThreadData {
	bNodeStack *stack;
	RenderData *rd;
} ThreadData;

static void *exec_composite_node(void *nodeexec_v)
{
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeExec *nodeexec= nodeexec_v;
	bNode *node= nodeexec->node;
	ThreadData *thd= (ThreadData *)node->threaddata;
	
	node_get_stack(node, thd->stack, nsin, nsout);
	
	if (node->typeinfo->execfunc)
		node->typeinfo->execfunc(thd->rd, node, nsin, nsout);
	else if (node->typeinfo->newexecfunc)
		node->typeinfo->newexecfunc(thd->rd, 0, node, nodeexec->data, nsin, nsout);
	
	node->exec |= NODE_READY;
	return NULL;
}

/* return total of executable nodes, for timecursor */
static int setExecutableNodes(bNodeTreeExec *exec, ThreadData *thd)
{
	bNodeTree *ntree = exec->nodetree;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeExec *nodeexec;
	bNode *node;
	bNodeSocket *sock;
	int n, totnode= 0, group_edit= 0;
	
	/* if we are in group edit, viewer nodes get skipped when group has viewer */
	for (node= ntree->nodes.first; node; node= node->next)
		if (node->type==NODE_GROUP && (node->flag & NODE_GROUP_EDIT))
			if (ntreeHasType((bNodeTree *)node->id, CMP_NODE_VIEWER))
				group_edit= 1;
	
	/* NB: using the exec data list here to have valid dependency sort */
	for (n=0, nodeexec=exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
		int a;
		node = nodeexec->node;
		
		node_get_stack(node, exec->stack, nsin, nsout);
		
		/* test the outputs */
		/* skip value-only nodes (should be in type!) */
		if (!node_only_value(node)) {
			for (a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
				if (nsout[a]->data==NULL && nsout[a]->hasoutput) {
					node->need_exec= 1;
					break;
				}
			}
		}
		
		/* test the inputs */
		for (a=0, sock= node->inputs.first; sock; sock= sock->next, a++) {
			/* skip viewer nodes in bg render or group edit */
			if ( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER) && (G.background || group_edit))
				node->need_exec= 0;
			/* is sock in use? */
			else if (sock->link) {
				bNodeLink *link= sock->link;
				
				/* this is the test for a cyclic case */
				if (link->fromnode==NULL || link->tonode==NULL);
				else if (link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF) {
					if (link->fromnode->need_exec) {
						node->need_exec= 1;
						break;
					}
				}
				else {
					node->need_exec= 0;
					printf("Node %s skipped, cyclic dependency\n", node->name);
				}
			}
		}
		
		if (node->need_exec) {
			
			/* free output buffers */
			for (a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
				if (nsout[a]->data) {
					free_compbuf(nsout[a]->data);
					nsout[a]->data= NULL;
				}
			}
			totnode++;
			/* printf("node needs exec %s\n", node->name); */
			
			/* tag for getExecutableNode() */
			node->exec= 0;
		}
		else {
			/* tag for getExecutableNode() */
			node->exec= NODE_READY|NODE_FINISHED|NODE_SKIPPED;
			
		}
	}
	
	/* last step: set the stack values for only-value nodes */
	/* just does all now, compared to a full buffer exec this is nothing */
	if (totnode) {
		for (n=0, nodeexec=exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
			node = nodeexec->node;
			if (node->need_exec==0 && node_only_value(node)) {
				if (node->typeinfo->execfunc) {
					node_get_stack(node, exec->stack, nsin, nsout);
					node->typeinfo->execfunc(thd->rd, node, nsin, nsout);
				}
			}
		}
	}
	
	return totnode;
}

/* while executing tree, free buffers from nodes that are not needed anymore */
static void freeExecutableNode(bNodeTreeExec *exec)
{
	/* node outputs can be freed when:
	- not a render result or image node
	- when node outputs go to nodes all being set NODE_FINISHED
	*/
	bNodeTree *ntree = exec->nodetree;
	bNodeExec *nodeexec;
	bNode *node;
	bNodeSocket *sock;
	int n;
	
	/* set exec flag for finished nodes that might need freed */
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->type!=CMP_NODE_R_LAYERS)
			if (node->exec & NODE_FINISHED)
				node->exec |= NODE_FREEBUFS;
	}
	/* clear this flag for input links that are not done yet.
	 * Using the exec data for valid dependency sort.
	 */
	for (n=0, nodeexec=exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
		node = nodeexec->node;
		if ((node->exec & NODE_FINISHED)==0) {
			for (sock= node->inputs.first; sock; sock= sock->next)
				if (sock->link)
					sock->link->fromnode->exec &= ~NODE_FREEBUFS;
		}
	}
	/* now we can free buffers */
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->exec & NODE_FREEBUFS) {
			for (sock= node->outputs.first; sock; sock= sock->next) {
				bNodeStack *ns= node_get_socket_stack(exec->stack, sock);
				if (ns && ns->data) {
					free_compbuf(ns->data);
					ns->data= NULL;
					// printf("freed buf node %s\n", node->name);
				}
			}
		}
	}
}

static bNodeExec *getExecutableNode(bNodeTreeExec *exec)
{
	bNodeExec *nodeexec;
	bNodeSocket *sock;
	int n;
	
	for (n=0, nodeexec=exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
		if (nodeexec->node->exec==0) {
			/* input sockets should be ready */
			for (sock= nodeexec->node->inputs.first; sock; sock= sock->next) {
				if (sock->link && sock->link->fromnode)
					if ((sock->link->fromnode->exec & NODE_READY)==0)
						break;
			}
			if (sock==NULL)
				return nodeexec;
		}
	}
	return NULL;
}

/* check if texture nodes need exec or end */
static  void ntree_composite_texnode(bNodeTree *ntree, int init)
{
	bNode *node;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->type==CMP_NODE_TEXTURE && node->id) {
			Tex *tex= (Tex *)node->id;
			if (tex->nodetree && tex->use_nodes) {
				/* has internal flag to detect it only does it once */
				if (init) {
					if (!tex->nodetree->execdata)
						tex->nodetree->execdata = ntreeTexBeginExecTree(tex->nodetree, 1); 
				}
				else
					ntreeTexEndExecTree(tex->nodetree->execdata, 1);
					tex->nodetree->execdata = NULL;
			}
		}
	}

}

/* optimized tree execute test for compositing */
static void ntreeCompositExecTreeOld(bNodeTree *ntree, RenderData *rd, int do_preview)
{
	bNodeExec *nodeexec;
	bNode *node;
	ListBase threads;
	ThreadData thdata;
	int totnode, curnode, rendering= 1, n;
	bNodeTreeExec *exec= ntree->execdata;
	
	if (ntree == NULL) return;
	
	if (do_preview)
		ntreeInitPreview(ntree, 0, 0);
	
	if (!ntree->execdata) {
		/* XXX this is the top-level tree, so we use the ntree->execdata pointer. */
		exec = ntreeCompositBeginExecTree(ntree, 1);
	}
	ntree_composite_texnode(ntree, 1);
	
	/* prevent unlucky accidents */
	if (G.background)
		rd->scemode &= ~R_COMP_CROP;
	
	/* setup callerdata for thread callback */
	thdata.rd= rd;
	thdata.stack= exec->stack;
	
	/* fixed seed, for example noise texture */
	BLI_srandom(rd->cfra);

	/* sets need_exec tags in nodes */
	curnode = totnode= setExecutableNodes(exec, &thdata);

	BLI_init_threads(&threads, exec_composite_node, rd->threads);
	
	while (rendering) {
		
		if (BLI_available_threads(&threads)) {
			nodeexec= getExecutableNode(exec);
			if (nodeexec) {
				node = nodeexec->node;
				if (ntree->progress && totnode)
					ntree->progress(ntree->prh, (1.0f - curnode/(float)totnode));
				if (ntree->stats_draw) {
					char str[128];
					BLI_snprintf(str, sizeof(str), "Compositing %d %s", curnode, node->name);
					ntree->stats_draw(ntree->sdh, str);
				}
				curnode--;
				
				node->threaddata = &thdata;
				node->exec= NODE_PROCESSING;
				BLI_insert_thread(&threads, nodeexec);
			}
			else
				PIL_sleep_ms(50);
		}
		else
			PIL_sleep_ms(50);
		
		rendering= 0;
		/* test for ESC */
		if (ntree->test_break && ntree->test_break(ntree->tbh)) {
			for (node= ntree->nodes.first; node; node= node->next)
				node->exec |= NODE_READY;
		}
		
		/* check for ready ones, and if we need to continue */
		for (n=0, nodeexec=exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
			node = nodeexec->node;
			if (node->exec & NODE_READY) {
				if ((node->exec & NODE_FINISHED)==0) {
					BLI_remove_thread(&threads, nodeexec); /* this waits for running thread to finish btw */
					node->exec |= NODE_FINISHED;
					
					/* freeing unused buffers */
					if (rd->scemode & R_COMP_FREE)
						freeExecutableNode(exec);
				}
			}
			else rendering= 1;
		}
	}
	
	BLI_end_threads(&threads);
	
	/* XXX top-level tree uses the ntree->execdata pointer */
	ntreeCompositEndExecTree(exec, 1);
}
#endif

void ntreeCompositExecTree(bNodeTree *ntree, RenderData *rd, int rendering, int do_preview)
{
#ifdef WITH_COMPOSITOR
	if (G.rt == 200)
		ntreeCompositExecTreeOld(ntree, rd, do_preview);
	else
		COM_execute(rd, ntree, rendering);
#else
	(void)ntree, (void)rd, (void)rendering, (void)do_preview;
#endif
}

/* *********************************************** */

/* clumsy checking... should do dynamic outputs once */
static void force_hidden_passes(bNode *node, int passflag)
{
	bNodeSocket *sock;
	
	for (sock= node->outputs.first; sock; sock= sock->next)
		sock->flag &= ~SOCK_UNAVAIL;
	
	if (!(passflag & SCE_PASS_COMBINED)) {
		sock= BLI_findlink(&node->outputs, RRES_OUT_IMAGE);
		sock->flag |= SOCK_UNAVAIL;
		sock= BLI_findlink(&node->outputs, RRES_OUT_ALPHA);
		sock->flag |= SOCK_UNAVAIL;
	}
	
	sock= BLI_findlink(&node->outputs, RRES_OUT_Z);
	if (!(passflag & SCE_PASS_Z)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_NORMAL);
	if (!(passflag & SCE_PASS_NORMAL)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_VEC);
	if (!(passflag & SCE_PASS_VECTOR)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_UV);
	if (!(passflag & SCE_PASS_UV)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_RGBA);
	if (!(passflag & SCE_PASS_RGBA)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_DIFF);
	if (!(passflag & SCE_PASS_DIFFUSE)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_SPEC);
	if (!(passflag & SCE_PASS_SPEC)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_SHADOW);
	if (!(passflag & SCE_PASS_SHADOW)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_AO);
	if (!(passflag & SCE_PASS_AO)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_REFLECT);
	if (!(passflag & SCE_PASS_REFLECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_REFRACT);
	if (!(passflag & SCE_PASS_REFRACT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDIRECT);
	if (!(passflag & SCE_PASS_INDIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDEXOB);
	if (!(passflag & SCE_PASS_INDEXOB)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDEXMA);
	if (!(passflag & SCE_PASS_INDEXMA)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_MIST);
	if (!(passflag & SCE_PASS_MIST)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_EMIT);
	if (!(passflag & SCE_PASS_EMIT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_ENV);
	if (!(passflag & SCE_PASS_ENVIRONMENT)) sock->flag |= SOCK_UNAVAIL;

	sock= BLI_findlink(&node->outputs, RRES_OUT_DIFF_DIRECT);
	if (!(passflag & SCE_PASS_DIFFUSE_DIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_DIFF_INDIRECT);
	if (!(passflag & SCE_PASS_DIFFUSE_INDIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_DIFF_COLOR);
	if (!(passflag & SCE_PASS_DIFFUSE_COLOR)) sock->flag |= SOCK_UNAVAIL;

	sock= BLI_findlink(&node->outputs, RRES_OUT_GLOSSY_DIRECT);
	if (!(passflag & SCE_PASS_GLOSSY_DIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_GLOSSY_INDIRECT);
	if (!(passflag & SCE_PASS_GLOSSY_INDIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_GLOSSY_COLOR);
	if (!(passflag & SCE_PASS_GLOSSY_COLOR)) sock->flag |= SOCK_UNAVAIL;

	sock= BLI_findlink(&node->outputs, RRES_OUT_TRANSM_DIRECT);
	if (!(passflag & SCE_PASS_TRANSM_DIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_TRANSM_INDIRECT);
	if (!(passflag & SCE_PASS_TRANSM_INDIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_TRANSM_COLOR);
	if (!(passflag & SCE_PASS_TRANSM_COLOR)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_TRANSM_COLOR);
}

/* based on rules, force sockets hidden always */
void ntreeCompositForceHidden(bNodeTree *ntree, Scene *curscene)
{
	bNode *node;
	
	if (ntree==NULL) return;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		if ( node->type==CMP_NODE_R_LAYERS) {
			Scene *sce= node->id?(Scene *)node->id:curscene;
			SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
			if (srl)
				force_hidden_passes(node, srl->passflag);
		}
		/* XXX this stuff is called all the time, don't want that.
		 * Updates should only happen when actually necessary.
		 */
		#if 0
		else if ( node->type==CMP_NODE_IMAGE) {
			nodeUpdate(ntree, node);
		}
		#endif
	}

}

/* called from render pipeline, to tag render input and output */
/* need to do all scenes, to prevent errors when you re-render 1 scene */
void ntreeCompositTagRender(Scene *curscene)
{
	Scene *sce;
	
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			
			for (node= sce->nodetree->nodes.first; node; node= node->next) {
				if (node->id==(ID *)curscene || node->type==CMP_NODE_COMPOSITE)
					nodeUpdate(sce->nodetree, node);
				else if (node->type==CMP_NODE_TEXTURE) /* uses scene sizex/sizey */
					nodeUpdate(sce->nodetree, node);
			}
		}
	}
}

static int node_animation_properties(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock;
	const ListBase *lb;
	Link *link;
	PointerRNA ptr;
	PropertyRNA *prop;
	
	/* check to see if any of the node's properties have fcurves */
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);
	lb = RNA_struct_type_properties(ptr.type);
	
	for (link=lb->first; link; link=link->next) {
		int driven, len=1, index;
		prop = (PropertyRNA *)link;
		
		if (RNA_property_array_check(prop))
			len = RNA_property_array_length(&ptr, prop);
		
		for (index=0; index<len; index++) {
			if (rna_get_fcurve(&ptr, prop, index, NULL, &driven)) {
				nodeUpdate(ntree, node);
				return 1;
			}
		}
	}
	
	/* now check node sockets */
	for (sock = node->inputs.first; sock; sock=sock->next) {
		int driven, len=1, index;
		
		RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
		prop = RNA_struct_find_property(&ptr, "default_value");
		if (prop) {
			if (RNA_property_array_check(prop))
				len = RNA_property_array_length(&ptr, prop);
			
			for (index=0; index<len; index++) {
				if (rna_get_fcurve(&ptr, prop, index, NULL, &driven)) {
					nodeUpdate(ntree, node);
					return 1;
				}
			}
		}
	}

	return 0;
}

/* tags nodes that have animation capabilities */
int ntreeCompositTagAnimated(bNodeTree *ntree)
{
	bNode *node;
	int tagged= 0;
	
	if (ntree==NULL) return 0;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		
		tagged = node_animation_properties(ntree, node);
		
		/* otherwise always tag these node types */
		if (node->type==CMP_NODE_IMAGE) {
			Image *ima= (Image *)node->id;
			if (ima && ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				nodeUpdate(ntree, node);
				tagged= 1;
			}
		}
		else if (node->type==CMP_NODE_TIME) {
			nodeUpdate(ntree, node);
			tagged= 1;
		}
		/* here was tag render layer, but this is called after a render, so re-composites fail */
		else if (node->type==NODE_GROUP) {
			if ( ntreeCompositTagAnimated((bNodeTree *)node->id) ) {
				nodeUpdate(ntree, node);
			}
		}
		else if (ELEM(node->type, CMP_NODE_MOVIECLIP, CMP_NODE_TRANSFORM)) {
			nodeUpdate(ntree, node);
			tagged= 1;
		}
		else if (node->type==CMP_NODE_MASK) {
			nodeUpdate(ntree, node);
			tagged= 1;
		}
	}
	
	return tagged;
}


/* called from image window preview */
void ntreeCompositTagGenerators(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree==NULL) return;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		if ( ELEM(node->type, CMP_NODE_R_LAYERS, CMP_NODE_IMAGE))
			nodeUpdate(ntree, node);
	}
}

/* XXX after render animation system gets a refresh, this call allows composite to end clean */
void ntreeCompositClearTags(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree==NULL) return;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		node->need_exec= 0;
		if (node->type==NODE_GROUP)
			ntreeCompositClearTags((bNodeTree *)node->id);
	}
}
