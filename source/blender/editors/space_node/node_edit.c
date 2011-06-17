/*
 * $Id$
 *
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
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_edit.c
 *  \ingroup spnode
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_storage_types.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_report.h"

#include "RE_pipeline.h"

#include "IMB_imbuf_types.h"

#include "ED_node.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_render.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "IMB_imbuf.h"

#include "RNA_enum_types.h"

#include "node_intern.h"

static EnumPropertyItem socket_in_out_items[] = {
	{ SOCK_IN, "IN", 0, "In", "" },
	{ SOCK_OUT, "OUT", 0, "Out", "" },
	{ 0, NULL, 0, NULL, NULL}
};

/* ***************** composite job manager ********************** */

typedef struct CompoJob {
	Scene *scene;
	bNodeTree *ntree;
	bNodeTree *localtree;
	short *stop;
	short *do_update;
	float *progress;
} CompoJob;

/* called by compo, only to check job 'stop' value */
static int compo_breakjob(void *cjv)
{
	CompoJob *cj= cjv;
	
	return *(cj->stop);
}

/* called by compo, wmJob sends notifier */
static void compo_redrawjob(void *cjv, char *UNUSED(str))
{
	CompoJob *cj= cjv;
	
	*(cj->do_update)= 1;
}

static void compo_freejob(void *cjv)
{
	CompoJob *cj= cjv;

	if(cj->localtree) {
		ntreeLocalMerge(cj->localtree, cj->ntree);
	}
	MEM_freeN(cj);
}

/* only now we copy the nodetree, so adding many jobs while
   sliding buttons doesn't frustrate */
static void compo_initjob(void *cjv)
{
	CompoJob *cj= cjv;

	cj->localtree= ntreeLocalize(cj->ntree);
}

/* called before redraw notifiers, it moves finished previews over */
static void compo_updatejob(void *cjv)
{
	CompoJob *cj= cjv;
	
	ntreeLocalSync(cj->localtree, cj->ntree);
}

static void compo_progressjob(void *cjv, float progress)
{
	CompoJob *cj= cjv;
	
	*(cj->progress) = progress;
}


/* only this runs inside thread */
static void compo_startjob(void *cjv, short *stop, short *do_update, float *progress)
{
	CompoJob *cj= cjv;
	bNodeTree *ntree= cj->localtree;

	if(cj->scene->use_nodes==0)
		return;
	
	cj->stop= stop;
	cj->do_update= do_update;
	cj->progress= progress;
	
	ntree->test_break= compo_breakjob;
	ntree->tbh= cj;
	ntree->stats_draw= compo_redrawjob;
	ntree->sdh= cj;
	ntree->progress= compo_progressjob;
	ntree->prh= cj;
	
	// XXX BIF_store_spare();
	
	ntreeCompositExecTree(ntree, &cj->scene->r, 1);	/* 1 is do_previews */
	
	ntree->test_break= NULL;
	ntree->stats_draw= NULL;
	ntree->progress= NULL;

}

void snode_composite_job(const bContext *C, ScrArea *sa)
{
	SpaceNode *snode= sa->spacedata.first;
	wmJob *steve;
	CompoJob *cj;

	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, "Compositing", WM_JOB_EXCL_RENDER|WM_JOB_PROGRESS);
	cj= MEM_callocN(sizeof(CompoJob), "compo job");
	
	/* customdata for preview thread */
	cj->scene= CTX_data_scene(C);
	cj->ntree= snode->nodetree;
	
	/* setup job */
	WM_jobs_customdata(steve, cj, compo_freejob);
	WM_jobs_timer(steve, 0.1, NC_SCENE, NC_SCENE|ND_COMPO_RESULT);
	WM_jobs_callbacks(steve, compo_startjob, compo_initjob, compo_updatejob, NULL);
	
	WM_jobs_start(CTX_wm_manager(C), steve);
	
}

/* ***************************************** */

/* operator poll callback */
static int composite_node_active(bContext *C)
{
	if( ED_operator_node_active(C)) {
		SpaceNode *snode= CTX_wm_space_node(C);
		if(snode->treetype==NTREE_COMPOSIT)
			return 1;
	}
	return 0;
}

/* also checks for edited groups */
static bNode *editnode_get_active(bNodeTree *ntree)
{
	bNode *node;
	
	/* check for edited group */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;
	if(node)
		return nodeGetActive((bNodeTree *)node->id);
	else
		return nodeGetActive(ntree);
}

void snode_dag_update(bContext *UNUSED(C), SpaceNode *snode)
{
	DAG_id_tag_update(snode->id, 0);
}

void snode_notify(bContext *C, SpaceNode *snode)
{
	WM_event_add_notifier(C, NC_NODE|NA_EDITED, NULL);

	if(snode->treetype==NTREE_SHADER)
		WM_event_add_notifier(C, NC_MATERIAL|ND_NODES, snode->id);
	else if(snode->treetype==NTREE_COMPOSIT)
		WM_event_add_notifier(C, NC_SCENE|ND_NODES, snode->id);
	else if(snode->treetype==NTREE_TEXTURE)
		WM_event_add_notifier(C, NC_TEXTURE|ND_NODES, snode->id);
}

bNode *node_tree_get_editgroup(bNodeTree *nodetree)
{
	bNode *gnode;
	
	/* get the groupnode */
	for(gnode= nodetree->nodes.first; gnode; gnode= gnode->next)
		if(gnode->flag & NODE_GROUP_EDIT)
			break;
	return gnode;
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_shader_default(Material *ma)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(ma->nodetree) {
		if (G.f & G_DEBUG)
			printf("error in shader initialize\n");
		return;
	}
	
	ma->nodetree= ntreeAddTree("Shader Nodetree", NTREE_SHADER, FALSE);
	
	out= nodeAddNodeType(ma->nodetree, SH_NODE_OUTPUT, NULL, NULL);
	out->locx= 300.0f; out->locy= 300.0f;
	
	in= nodeAddNodeType(ma->nodetree, SH_NODE_MATERIAL, NULL, NULL);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(ma->nodetree, in);
	
	/* only a link from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(ma->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(ma->nodetree);	/* needed for pointers */
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_composit_default(Scene *sce)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(sce->nodetree) {
		if (G.f & G_DEBUG)
			printf("error in composite initialize\n");
		return;
	}
	
	sce->nodetree= ntreeAddTree("Compositing Nodetree", NTREE_COMPOSIT, FALSE);
	
	out= nodeAddNodeType(sce->nodetree, CMP_NODE_COMPOSITE, NULL, NULL);
	out->locx= 300.0f; out->locy= 400.0f;
	out->id= &sce->id;
	id_us_plus(out->id);
	
	in= nodeAddNodeType(sce->nodetree, CMP_NODE_R_LAYERS, NULL, NULL);
	in->locx= 10.0f; in->locy= 400.0f;
	in->id= &sce->id;
	id_us_plus(in->id);
	nodeSetActive(sce->nodetree, in);
	
	/* links from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(sce->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(sce->nodetree);	/* needed for pointers */
	
	// XXX ntreeCompositForceHidden(sce->nodetree);
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_texture_default(Tex *tx)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(tx->nodetree) {
		if (G.f & G_DEBUG)
			printf("error in texture initialize\n");
		return;
	}
	
	tx->nodetree= ntreeAddTree("Texture Nodetree", NTREE_TEXTURE, FALSE);
	
	out= nodeAddNodeType(tx->nodetree, TEX_NODE_OUTPUT, NULL, NULL);
	out->locx= 300.0f; out->locy= 300.0f;
	
	in= nodeAddNodeType(tx->nodetree, TEX_NODE_CHECKER, NULL, NULL);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(tx->nodetree, in);
	
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(tx->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(tx->nodetree);	/* needed for pointers */
}

/* id is supposed to contain a node tree */
void node_tree_from_ID(ID *id, bNodeTree **ntree, bNodeTree **edittree, int *treetype)
{
	bNode *node= NULL;
	short idtype= GS(id->name);

	if(idtype == ID_MA) {
		*ntree= ((Material*)id)->nodetree;
		if(treetype) *treetype= NTREE_SHADER;
	}
	else if(idtype == ID_SCE) {
		*ntree= ((Scene*)id)->nodetree;
		if(treetype) *treetype= NTREE_COMPOSIT;
	}
	else if(idtype == ID_TE) {
		*ntree= ((Tex*)id)->nodetree;
		if(treetype) *treetype= NTREE_TEXTURE;
	}
	else {
		if(treetype) *treetype= 0;
		return;
	}

	/* find editable group */
	if(edittree) {
		if(*ntree)
			for(node= (*ntree)->nodes.first; node; node= node->next)
				if(node->flag & NODE_GROUP_EDIT)
					break;
		
		if(node && node->id)
			*edittree= (bNodeTree *)node->id;
		else
			*edittree= *ntree;
	}
}

/* Here we set the active tree(s), even called for each redraw now, so keep it fast :) */
void snode_set_context(SpaceNode *snode, Scene *scene)
{
	Object *ob= OBACT;
	
	snode->nodetree= NULL;
	snode->edittree= NULL;
	snode->id= snode->from= NULL;
	
	if(snode->treetype==NTREE_SHADER) {
		/* need active object, or we allow pinning... */
		if(ob) {
			Material *ma= give_current_material(ob, ob->actcol);
			if(ma) {
				snode->from= &ob->id;
				snode->id= &ma->id;
			}
		}
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		snode->from= NULL;
		snode->id= &scene->id;
		
		/* bit clumsy but reliable way to see if we draw first time */
		if(snode->nodetree==NULL)
			ntreeCompositForceHidden(scene->nodetree, scene);
	}
	else if(snode->treetype==NTREE_TEXTURE) {
		Tex *tx= NULL;

		if(snode->texfrom==SNODE_TEX_OBJECT) {
			if(ob) {
				tx= give_current_object_texture(ob);

				if(ob->type == OB_LAMP)
					snode->from= (ID*)ob->data;
				else
					snode->from= (ID*)give_current_material(ob, ob->actcol);

				/* from is not set fully for material nodes, should be ID + Node then */
				snode->id= &tx->id;
			}
		}
		else if(snode->texfrom==SNODE_TEX_WORLD) {
			tx= give_current_world_texture(scene->world);
			snode->from= (ID *)scene->world;
			snode->id= &tx->id;
		}
		else {
			struct Brush *brush= NULL;
			
			if(ob && (ob->mode & OB_MODE_SCULPT))
				brush= paint_brush(&scene->toolsettings->sculpt->paint);
			else
				brush= paint_brush(&scene->toolsettings->imapaint.paint);

			if (brush) {
				snode->from= (ID *)brush;
				tx= give_current_brush_texture(brush);
				snode->id= &tx->id;
			}
		}
	}

	if(snode->id)
		node_tree_from_ID(snode->id, &snode->nodetree, &snode->edittree, NULL);
}

static void snode_tag_changed(SpaceNode *snode, bNode *node)
{
	bNode *gnode;
	
	if (node)
		NodeTagChanged(snode->edittree, node);
	
	/* if inside group, tag entire group */
	gnode= node_tree_get_editgroup(snode->nodetree);
	if(gnode)
		NodeTagIDChanged(snode->nodetree, gnode->id);
}

void node_set_active(SpaceNode *snode, bNode *node)
{
	nodeSetActive(snode->edittree, node);
	
	if(node->type!=NODE_GROUP) {
		int was_output= (node->flag & NODE_DO_OUTPUT);
		
		/* tree specific activate calls */
		if(snode->treetype==NTREE_SHADER) {
			/* when we select a material, active texture is cleared, for buttons */
			if(node->id && GS(node->id->name)==ID_MA)
				nodeClearActiveID(snode->edittree, ID_TE);
			
			if(node->type==SH_NODE_OUTPUT) {
				bNode *tnode;
				
				for(tnode= snode->edittree->nodes.first; tnode; tnode= tnode->next)
					if( tnode->type==SH_NODE_OUTPUT)
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if(was_output==0)
					ED_node_changed_update(snode->id, node);
			}

			WM_main_add_notifier(NC_MATERIAL|ND_NODES, node->id);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			Scene *scene= (Scene*)snode->id;

			/* make active viewer, currently only 1 supported... */
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				bNode *tnode;
				

				for(tnode= snode->edittree->nodes.first; tnode; tnode= tnode->next)
					if( ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if(was_output==0) {
					snode_tag_changed(snode, node);
					
					ED_node_changed_update(snode->id, node);
				}
				
				/* addnode() doesnt link this yet... */
				node->id= (ID *)BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
			}
			else if(node->type==CMP_NODE_R_LAYERS) {
				if(node->id==NULL || node->id==(ID *)scene) {
					scene->r.actlay= node->custom1;
				}
			}
			else if(node->type==CMP_NODE_COMPOSITE) {
				bNode *tnode;
				
				for(tnode= snode->edittree->nodes.first; tnode; tnode= tnode->next)
					if( tnode->type==CMP_NODE_COMPOSITE)
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				ED_node_changed_update(snode->id, node);
			}
		}
		else if(snode->treetype==NTREE_TEXTURE) {
			// XXX
#if 0
			if(node->id)
				; // XXX BIF_preview_changed(-1);
			// allqueue(REDRAWBUTSSHADING, 1);
			// allqueue(REDRAWIPO, 0);
#endif
		}
	}
}

/* when links in groups change, inputs/outputs change, nodes added/deleted... */
void node_tree_verify_groups(bNodeTree *nodetree)
{
	bNode *gnode;
	
	gnode= node_tree_get_editgroup(nodetree);
	
	/* does all materials */
	if(gnode)
		nodeGroupVerify((bNodeTree *)gnode->id);
	
}

/* ***************** Edit Group operator ************* */

void snode_make_group_editable(SpaceNode *snode, bNode *gnode)
{
	bNode *node;
	
	/* make sure nothing has group editing on */
	for(node= snode->nodetree->nodes.first; node; node= node->next)
		node->flag &= ~NODE_GROUP_EDIT;
	
	if(gnode==NULL) {
		/* with NULL argument we do a toggle */
		if(snode->edittree==snode->nodetree)
			gnode= nodeGetActive(snode->nodetree);
	}
	
	if(gnode && gnode->type==NODE_GROUP && gnode->id) {
		if(gnode->id->lib)
			ntreeMakeLocal((bNodeTree *)gnode->id);

		gnode->flag |= NODE_GROUP_EDIT;
		snode->edittree= (bNodeTree *)gnode->id;
		
		/* deselect all other nodes, so we can also do grabbing of entire subtree */
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			node->flag &= ~SELECT;
		gnode->flag |= SELECT;
		
	}
	else 
		snode->edittree= snode->nodetree;
	
	ntreeSolveOrder(snode->nodetree);
}

static int node_group_edit_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	ED_preview_kill_jobs(C);

	gnode= nodeGetActive(snode->edittree);
	snode_make_group_editable(snode, gnode);

	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);

	return OPERATOR_FINISHED;
}

static int node_group_edit_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	gnode= nodeGetActive(snode->edittree);
	if(gnode && gnode->type==NODE_GROUP && gnode->id && gnode->id->lib) {
		uiPupMenuOkee(C, op->type->idname, "Make group local?");
		return OPERATOR_CANCELLED;
	}

	return node_group_edit_exec(C, op);
}

void NODE_OT_group_edit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Edit Group";
	ot->description = "Edit node group";
	ot->idname = "NODE_OT_group_edit";
	
	/* api callbacks */
	ot->invoke = node_group_edit_invoke;
	ot->exec = node_group_edit_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ***************** Add Group Socket operator ************* */

static int node_group_socket_add_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int in_out= -1;
	char name[32]= "";
	int type= SOCK_VALUE;
	bNodeTree *ngroup= snode->edittree;
	bNodeSocket *sock;
	
	ED_preview_kill_jobs(C);
	
	if (RNA_property_is_set(op->ptr, "name"))
		RNA_string_get(op->ptr, "name", name);
	
	if (RNA_property_is_set(op->ptr, "type"))
		type = RNA_enum_get(op->ptr, "type");
	
	if (RNA_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;
	
	sock = nodeGroupAddSocket(ngroup, name, type, in_out);
	
	node_tree_verify_groups(snode->nodetree);
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Group Socket";
	ot->description = "Add node group socket";
	ot->idname = "NODE_OT_group_socket_add";
	
	/* api callbacks */
	ot->exec = node_group_socket_add_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
	RNA_def_string(ot->srna, "name", "", 32, "Name", "Group socket name");
	RNA_def_enum(ot->srna, "type", node_socket_type_items, SOCK_VALUE, "Type", "Type of the group socket");
}

/* ***************** Remove Group Socket operator ************* */

static int node_group_socket_remove_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int index= -1;
	int in_out= -1;
	bNodeTree *ngroup= snode->edittree;
	bNodeSocket *sock;
	
	ED_preview_kill_jobs(C);
	
	if (RNA_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;
	
	if (RNA_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;
	
	sock = (bNodeSocket*)BLI_findlink(in_out==SOCK_IN ? &ngroup->inputs : &ngroup->outputs, index);
	if (sock) {
		nodeGroupRemoveSocket(ngroup, sock, in_out);
		node_tree_verify_groups(snode->nodetree);
		
		snode_notify(C, snode);
	}
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Group Socket";
	ot->description = "Removed node group socket";
	ot->idname = "NODE_OT_group_socket_remove";
	
	/* api callbacks */
	ot->exec = node_group_socket_remove_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
}

/* ***************** Move Group Socket Up operator ************* */

static int node_group_socket_move_up_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int index= -1;
	int in_out= -1;
	bNodeTree *ngroup= snode->edittree;
	bNodeSocket *sock, *prev;
	
	ED_preview_kill_jobs(C);
	
	if (RNA_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;
	
	if (RNA_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;
	
	/* swap */
	if (in_out==SOCK_IN) {
		sock = (bNodeSocket*)BLI_findlink(&ngroup->inputs, index);
		prev = sock->prev;
		/* can't move up the first socket */
		if (!prev)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->inputs, sock);
		BLI_insertlinkbefore(&ngroup->inputs, prev, sock);
	}
	else if (in_out==SOCK_OUT) {
		sock = (bNodeSocket*)BLI_findlink(&ngroup->outputs, index);
		prev = sock->prev;
		/* can't move up the first socket */
		if (!prev)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->outputs, sock);
		BLI_insertlinkbefore(&ngroup->outputs, prev, sock);
	}
	node_tree_verify_groups(snode->nodetree);
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_move_up(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Group Socket Up";
	ot->description = "Move up node group socket";
	ot->idname = "NODE_OT_group_socket_move_up";
	
	/* api callbacks */
	ot->exec = node_group_socket_move_up_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
}

/* ***************** Move Group Socket Up operator ************* */

static int node_group_socket_move_down_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int index= -1;
	int in_out= -1;
	bNodeTree *ngroup= snode->edittree;
	bNodeSocket *sock, *next;
	
	ED_preview_kill_jobs(C);
	
	if (RNA_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;
	
	if (RNA_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;
	
	/* swap */
	if (in_out==SOCK_IN) {
		sock = (bNodeSocket*)BLI_findlink(&ngroup->inputs, index);
		next = sock->next;
		/* can't move down the last socket */
		if (!next)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->inputs, sock);
		BLI_insertlinkafter(&ngroup->inputs, next, sock);
	}
	else if (in_out==SOCK_OUT) {
		sock = (bNodeSocket*)BLI_findlink(&ngroup->outputs, index);
		next = sock->next;
		/* can't move down the last socket */
		if (!next)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->outputs, sock);
		BLI_insertlinkafter(&ngroup->outputs, next, sock);
	}
	node_tree_verify_groups(snode->nodetree);
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_move_down(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Group Socket Down";
	ot->description = "Move down node group socket";
	ot->idname = "NODE_OT_group_socket_move_down";
	
	/* api callbacks */
	ot->exec = node_group_socket_move_down_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
}

/* ******************** Ungroup operator ********************** */

static int node_group_ungroup_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	ED_preview_kill_jobs(C);

	/* are we inside of a group? */
	gnode= node_tree_get_editgroup(snode->nodetree);
	if(gnode)
		snode_make_group_editable(snode, NULL);
	
	gnode= nodeGetActive(snode->edittree);
	if(gnode==NULL)
		return OPERATOR_CANCELLED;
	
	if(gnode->type!=NODE_GROUP) {
		BKE_report(op->reports, RPT_WARNING, "Not a group");
		return OPERATOR_CANCELLED;
	}
	else if(!nodeGroupUnGroup(snode->edittree, gnode)) {
		BKE_report(op->reports, RPT_WARNING, "Can't ungroup");
		return OPERATOR_CANCELLED;
	}

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_group_ungroup(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Ungroup";
	ot->description = "Ungroup selected nodes";
	ot->idname = "NODE_OT_group_ungroup";
	
	/* api callbacks */
	ot->exec = node_group_ungroup_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************** Node generic ************** */

/* allows to walk the list in order of visibility */
bNode *next_node(bNodeTree *ntree)
{
	static bNode *current=NULL, *last= NULL;
	
	if(ntree) {
		/* set current to the first selected node */
		for(current= ntree->nodes.last; current; current= current->prev)
			if(current->flag & NODE_SELECT)
				break;
		
		/* set last to the first unselected node */
		for(last= ntree->nodes.last; last; last= last->prev)
			if((last->flag & NODE_SELECT)==0)
				break;
		
		if(current==NULL)
			current= last;
		
		return NULL;
	}
	/* no nodes, or we are ready */
	if(current==NULL)
		return NULL;
	
	/* now we walk the list backwards, but we always return current */
	if(current->flag & NODE_SELECT) {
		bNode *node= current;
		
		/* find previous selected */
		current= current->prev;
		while(current && (current->flag & NODE_SELECT)==0)
			current= current->prev;
		
		/* find first unselected */
		if(current==NULL)
			current= last;
		
		return node;
	}
	else {
		bNode *node= current;
		
		/* find previous unselected */
		current= current->prev;
		while(current && (current->flag & NODE_SELECT))
			current= current->prev;
		
		return node;
	}
	
	return NULL;
}

/* is rct in visible part of node? */
static bNode *visible_node(SpaceNode *snode, rctf *rct)
{
	bNode *tnode;
	
	for(next_node(snode->edittree); (tnode=next_node(NULL));) {
		if(BLI_isect_rctf(&tnode->totr, rct, NULL))
			break;
	}
	return tnode;
}

/* **************************** */

typedef struct NodeViewMove {
	int mvalo[2];
	int xmin, ymin, xmax, ymax;
} NodeViewMove;

static int snode_bg_viewmove_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	NodeViewMove *nvm= op->customdata;

	switch (event->type) {
		case MOUSEMOVE:
			
			snode->xof -= (nvm->mvalo[0]-event->mval[0]);
			snode->yof -= (nvm->mvalo[1]-event->mval[1]);
			nvm->mvalo[0]= event->mval[0];
			nvm->mvalo[1]= event->mval[1];
			
			/* prevent dragging image outside of the window and losing it! */
			CLAMP(snode->xof, nvm->xmin, nvm->xmax);
			CLAMP(snode->yof, nvm->ymin, nvm->ymax);
			
			ED_region_tag_redraw(ar);
			
			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			
			MEM_freeN(nvm);
			op->customdata= NULL;

			WM_event_add_notifier(C, NC_SPACE|ND_SPACE_NODE, NULL);
			
			return OPERATOR_FINISHED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

static int snode_bg_viewmove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	NodeViewMove *nvm;
	Image *ima;
	ImBuf *ibuf;
	int pad= 10;
	void *lock;
	
	ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf= BKE_image_acquire_ibuf(ima, NULL, &lock);
	
	if(ibuf == NULL) {
		BKE_image_release_ibuf(ima, lock);
		return OPERATOR_CANCELLED;
	}

	nvm= MEM_callocN(sizeof(NodeViewMove), "NodeViewMove struct");
	op->customdata= nvm;
	nvm->mvalo[0]= event->mval[0];
	nvm->mvalo[1]= event->mval[1];

	nvm->xmin = -(ar->winx/2) - ibuf->x/2 + pad;
	nvm->xmax = ar->winx/2 + ibuf->x/2 - pad;
	nvm->ymin = -(ar->winy/2) - ibuf->y/2 + pad;
	nvm->ymax = ar->winy/2 + ibuf->y/2 - pad;

	BKE_image_release_ibuf(ima, lock);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static int snode_bg_viewmove_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

void NODE_OT_backimage_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Background Image Move";
	ot->description = "Move Node backdrop";
	ot->idname= "NODE_OT_backimage_move";
	
	/* api callbacks */
	ot->invoke= snode_bg_viewmove_invoke;
	ot->modal= snode_bg_viewmove_modal;
	ot->poll= composite_node_active;
	ot->cancel= snode_bg_viewmove_cancel;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;
}

static int backimage_zoom(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	float fac= RNA_float_get(op->ptr, "factor");

	snode->zoom *= fac;
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}


void NODE_OT_backimage_zoom(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Background Image Zoom";
	ot->idname= "NODE_OT_backimage_zoom";
	
	/* api callbacks */
	ot->exec= backimage_zoom;
	ot->poll= composite_node_active;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* internal */
	RNA_def_float(ot->srna, "factor", 1.2f, 0.0f, 10.0f, "Factor", "", 0.0f, 10.0f);
}

/******************** sample backdrop operator ********************/

typedef struct ImageSampleInfo {
	ARegionType *art;
	void *draw_handle;
	int x, y;
	int channels;
	int color_manage;

	char col[4];
	float colf[4];

	int draw;
} ImageSampleInfo;

static void sample_draw(const bContext *C, ARegion *ar, void *arg_info)
{
	ImageSampleInfo *info= arg_info;

	draw_nodespace_color_info(ar, (CTX_data_scene(C)->r.color_mgt_flag & R_COLOR_MANAGEMENT), info->channels,
							  info->x, info->y, info->col, info->colf);
}

static void sample_apply(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	ImageSampleInfo *info= op->customdata;
	void *lock;
	Image *ima;
	ImBuf *ibuf;
	float fx, fy, bufx, bufy;
	
	ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf= BKE_image_acquire_ibuf(ima, NULL, &lock);
	if(!ibuf)
		return;
	
	if(!ibuf->rect) {
		if(info->color_manage)
			ibuf->profile = IB_PROFILE_LINEAR_RGB;
		else
			ibuf->profile = IB_PROFILE_NONE;
		IMB_rect_from_float(ibuf);
	}

	/* map the mouse coords to the backdrop image space */
	bufx = ibuf->x * snode->zoom;
	bufy = ibuf->y * snode->zoom;
	fx = (bufx > 0.0f ? ((float)event->mval[0] - 0.5f*ar->winx - snode->xof) / bufx + 0.5f : 0.0f);
	fy = (bufy > 0.0f ? ((float)event->mval[1] - 0.5f*ar->winy - snode->yof) / bufy + 0.5f : 0.0f);

	if(fx>=0.0f && fy>=0.0f && fx<1.0f && fy<1.0f) {
		float *fp;
		char *cp;
		int x= (int)(fx*ibuf->x), y= (int)(fy*ibuf->y);

		CLAMP(x, 0, ibuf->x-1);
		CLAMP(y, 0, ibuf->y-1);

		info->x= x;
		info->y= y;
		info->draw= 1;
		info->channels= ibuf->channels;

		if(ibuf->rect) {
			cp= (char *)(ibuf->rect + y*ibuf->x + x);

			info->col[0]= cp[0];
			info->col[1]= cp[1];
			info->col[2]= cp[2];
			info->col[3]= cp[3];

			info->colf[0]= (float)cp[0]/255.0f;
			info->colf[1]= (float)cp[1]/255.0f;
			info->colf[2]= (float)cp[2]/255.0f;
			info->colf[3]= (float)cp[3]/255.0f;
		}
		if(ibuf->rect_float) {
			fp= (ibuf->rect_float + (ibuf->channels)*(y*ibuf->x + x));

			info->colf[0]= fp[0];
			info->colf[1]= fp[1];
			info->colf[2]= fp[2];
			info->colf[3]= fp[3];
		}
	}
	else
		info->draw= 0;

	BKE_image_release_ibuf(ima, lock);
	
	ED_area_tag_redraw(CTX_wm_area(C));
}

static void sample_exit(bContext *C, wmOperator *op)
{
	ImageSampleInfo *info= op->customdata;

	ED_region_draw_cb_exit(info->art, info->draw_handle);
	ED_area_tag_redraw(CTX_wm_area(C));
	MEM_freeN(info);
}

static int sample_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	ImageSampleInfo *info;

	if(snode->treetype!=NTREE_COMPOSIT || !(snode->flag & SNODE_BACKDRAW))
		return OPERATOR_CANCELLED;
	
	info= MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo");
	info->art= ar->type;
	info->draw_handle = ED_region_draw_cb_activate(ar->type, sample_draw, info, REGION_DRAW_POST_PIXEL);
	op->customdata= info;

	sample_apply(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int sample_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case LEFTMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			sample_exit(C, op);
			return OPERATOR_CANCELLED;
		case MOUSEMOVE:
			sample_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int sample_cancel(bContext *C, wmOperator *op)
{
	sample_exit(C, op);
	return OPERATOR_CANCELLED;
}

void NODE_OT_backimage_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Backimage Sample";
	ot->idname= "NODE_OT_backimage_sample";
	
	/* api callbacks */
	ot->invoke= sample_invoke;
	ot->modal= sample_modal;
	ot->cancel= sample_cancel;
	ot->poll= ED_operator_node_active;

	/* flags */
	ot->flag= OPTYPE_BLOCKING;
}

/* ********************** size widget operator ******************** */

typedef struct NodeSizeWidget {
	float mxstart;
	float oldwidth;
} NodeSizeWidget;

static int node_resize_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNode *node= editnode_get_active(snode->edittree);
	NodeSizeWidget *nsw= op->customdata;
	float mx, my;
	
	switch (event->type) {
		case MOUSEMOVE:
			
			UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
									 &mx, &my);
			
			if (node) {
				if(node->flag & NODE_HIDDEN) {
					node->miniwidth= nsw->oldwidth + mx - nsw->mxstart;
					CLAMP(node->miniwidth, 0.0f, 100.0f);
				}
				else {
					node->width= nsw->oldwidth + mx - nsw->mxstart;
					CLAMP(node->width, UI_DPI_FAC*node->typeinfo->minwidth, UI_DPI_FAC*node->typeinfo->maxwidth);
				}
			}
				
			ED_region_tag_redraw(ar);

			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			
			MEM_freeN(nsw);
			op->customdata= NULL;
			
			return OPERATOR_FINISHED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

static int node_resize_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNode *node= editnode_get_active(snode->edittree);
	
	if(node) {
		rctf totr;
		
		/* convert mouse coordinates to v2d space */
		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
								 &snode->mx, &snode->my);
		
		totr= node->totr;
		
		if(node->flag & NODE_HIDDEN) {
			/* right part of node */
			totr.xmin= node->totr.xmax-20.0f;
		}
		else {
			/* bottom right corner */
			totr.xmin= totr.xmax-10.0f;
			totr.ymax= totr.ymin+10.0f;
		}
		
		if(BLI_in_rctf(&totr, snode->mx, snode->my)) {
			NodeSizeWidget *nsw= MEM_callocN(sizeof(NodeSizeWidget), "size widget op data");
			
			op->customdata= nsw;
			nsw->mxstart= snode->mx;
			
			/* store old */
			if(node->flag & NODE_HIDDEN)
				nsw->oldwidth= node->miniwidth;
			else
				nsw->oldwidth= node->width;
			
			/* add modal handler */
			WM_event_add_modal_handler(C, op);

			return OPERATOR_RUNNING_MODAL;
		}
	}
	return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
}

static int node_resize_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

void NODE_OT_resize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Resize Node";
	ot->idname= "NODE_OT_resize";
	
	/* api callbacks */
	ot->invoke= node_resize_invoke;
	ot->modal= node_resize_modal;
	ot->poll= ED_operator_node_active;
	ot->cancel= node_resize_cancel;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;
}

/* ********************** select ******************** */


/* no undo here! */
void node_deselectall(SpaceNode *snode)
{
	bNode *node;
	
	for(node= snode->edittree->nodes.first; node; node= node->next)
		node->flag &= ~SELECT;
}

/* return 1 if we need redraw otherwise zero. */
int node_select_same_type(SpaceNode *snode)
{
	bNode *nac, *p;
	int redraw;

	/* search for the active node. */
	for (nac= snode->edittree->nodes.first; nac; nac= nac->next) {
		if (nac->flag & SELECT)
			break;
	}

	/* no active node, return. */
	if (!nac)
		return(0);

	redraw= 0;
	for (p= snode->edittree->nodes.first; p; p= p->next) {
		if (p->type != nac->type && p->flag & SELECT) {
			/* if it's selected but different type, unselect */
			redraw= 1;
			p->flag &= ~SELECT;
		}
		else if (p->type == nac->type && (!(p->flag & SELECT))) {
			/* if it's the same type and is not selected, select! */
			redraw= 1;
			p->flag |= SELECT;
		}
	}
	return(redraw);
}

/* return 1 if we need redraw, otherwise zero.
 * dir can be 0 == next or 0 != prev.
 */
int node_select_same_type_np(SpaceNode *snode, int dir)
{
	bNode *nac, *p;

	/* search the active one. */
	for (nac= snode->edittree->nodes.first; nac; nac= nac->next) {
		if (nac->flag & SELECT)
			break;
	}

	/* no active node, return. */
	if (!nac)
		return(0);

	if (dir == 0)
		p= nac->next;
	else
		p= nac->prev;

	while (p) {
		/* Now search the next with the same type. */
		if (p->type == nac->type)
			break;

		if (dir == 0)
			p= p->next;
		else
			p= p->prev;
	}

	if (p) {
		node_deselectall(snode);
		p->flag |= SELECT;
		return(1);
	}
	return(0);
}

int node_has_hidden_sockets(bNode *node)
{
	bNodeSocket *sock;
	
	for(sock= node->inputs.first; sock; sock= sock->next)
		if(sock->flag & SOCK_HIDDEN)
			return 1;
	for(sock= node->outputs.first; sock; sock= sock->next)
		if(sock->flag & SOCK_HIDDEN)
			return 1;
	return 0;
}

static void node_link_viewer(SpaceNode *snode, bNode *tonode)
{
	bNode *node;

	/* context check */
	if(tonode==NULL || tonode->outputs.first==NULL)
		return;
	if( ELEM(tonode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
		return;
	
	/* get viewer */
	for(node= snode->edittree->nodes.first; node; node= node->next)
		if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
			if(node->flag & NODE_DO_OUTPUT)
				break;
	/* no viewer, we make one active */
	if(node==NULL) {
		for(node= snode->edittree->nodes.first; node; node= node->next) {
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				node->flag |= NODE_DO_OUTPUT;
				break;
			}
		}
	}
		
	if(node) {
		bNodeLink *link;
		bNodeSocket *sock= NULL;

		/* try to find an already connected socket to cycle to the next */
		for(link= snode->edittree->links.first; link; link= link->next)
			if(link->tonode==node && link->fromnode==tonode)
				if(link->tosock==node->inputs.first)
					break;

		if(link) {
			/* unlink existing connection */
			sock= link->fromsock;
			nodeRemLink(snode->edittree, link);

			/* find a socket after the previously connected socket */
			for(sock=sock->next; sock; sock= sock->next)
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
					break;
		}

		/* find a socket starting from the first socket */
		if(!sock) {
			for(sock= tonode->outputs.first; sock; sock= sock->next)
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
					break;
		}
		
		if(sock) {
			/* get link to viewer */
			for(link= snode->edittree->links.first; link; link= link->next)
				if(link->tonode==node && link->tosock==node->inputs.first)
					break;
			
			if(link==NULL) {
				nodeAddLink(snode->edittree, tonode, sock, node, node->inputs.first);
			}
			else {
				link->fromnode= tonode;
				link->fromsock= sock;
			}
			ntreeSolveOrder(snode->edittree);
			snode_tag_changed(snode, node);
		}
	}
}


static int node_active_link_viewer(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	
	node= editnode_get_active(snode->edittree);
	
	if(!node)
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	node_link_viewer(snode, node);
	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}



void NODE_OT_link_viewer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Link to Viewer Node";
	ot->description = "Link to Viewer Node";
	ot->idname= "NODE_OT_link_viewer";
	
	/* api callbacks */
	ot->exec= node_active_link_viewer;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



/* return 0, nothing done */
static int node_mouse_groupheader(SpaceNode *snode)
{
	bNode *gnode;
	float mx=0, my=0;
// XXX	int mval[2];
	
	gnode= node_tree_get_editgroup(snode->nodetree);
	if(gnode==NULL) return 0;
	
// XXX	getmouseco_areawin(mval);
// XXX	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* click in header or outside? */
	if(BLI_in_rctf(&gnode->totr, mx, my)==0) {
		rctf rect= gnode->totr;
		
		rect.ymax += NODE_DY;
		if(BLI_in_rctf(&rect, mx, my)==0)
			snode_make_group_editable(snode, NULL);	/* toggles, so exits editmode */
//		else
// XXX			transform_nodes(snode->nodetree, 'g', "Move group");
		
		return 1;
	}
	return 0;
}

/* checks snode->mouse position, and returns found node/socket */
/* type is SOCK_IN and/or SOCK_OUT */
static int find_indicated_socket(SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, int in_out)
{
	bNode *node;
	bNodeSocket *sock;
	rctf rect;
	
	/* check if we click in a socket */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		
		rect.xmin = snode->mx - (NODE_SOCKSIZE+4);
		rect.ymin = snode->my - (NODE_SOCKSIZE+4);
		rect.xmax = snode->mx + (NODE_SOCKSIZE+4);
		rect.ymax = snode->my + (NODE_SOCKSIZE+4);
		
		if (!(node->flag & NODE_HIDDEN)) {
			/* extra padding inside and out - allow dragging on the text areas too */
			if (in_out == SOCK_IN) {
				rect.xmax += NODE_SOCKSIZE;
				rect.xmin -= NODE_SOCKSIZE*4;
			} else if (in_out == SOCK_OUT) {
				rect.xmax += NODE_SOCKSIZE*4;
				rect.xmin -= NODE_SOCKSIZE;
			}
		}
		
		if(in_out & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if(node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
		if(in_out & SOCK_OUT) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if(node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
	}
	
	/* check group sockets
	 * NB: using ngroup->outputs as input sockets and vice versa here!
	 */
	if(in_out & SOCK_IN) {
		for(sock= snode->edittree->outputs.first; sock; sock= sock->next) {
			if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
				if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= NULL;	/* NULL node pointer indicates group socket */
					*sockp= sock;
					return 1;
				}
			}
		}
	}
	if(in_out & SOCK_OUT) {
		for(sock= snode->edittree->inputs.first; sock; sock= sock->next) {
			if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
				if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= NULL;	/* NULL node pointer indicates group socket */
					*sockp= sock;
					return 1;
				}
			}
		}
	}
	
	return 0;
}

static int node_socket_hilights(SpaceNode *snode, int in_out)
{
	bNode *node;
	bNodeSocket *sock, *tsock, *socksel= NULL;
	short redraw= 0;
	
	if(snode->edittree==NULL) return 0;
	
	/* deselect sockets */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next) {
			if(sock->flag & SELECT) {
				sock->flag &= ~SELECT;
				redraw++;
				socksel= sock;
			}
		}
		for(sock= node->outputs.first; sock; sock= sock->next) {
			if(sock->flag & SELECT) {
				sock->flag &= ~SELECT;
				redraw++;
				socksel= sock;
			}
		}
	}
	
	// XXX mousepos should be set here!
	
	if(find_indicated_socket(snode, &node, &tsock, in_out)) {
		tsock->flag |= SELECT;
		if(redraw==1 && tsock==socksel) redraw= 0;
		else redraw= 1;
	}
	
	return redraw;
}

static int outside_group_rect(SpaceNode *snode)
{
	bNode *gnode= node_tree_get_editgroup(snode->nodetree);
	if (gnode) {
		return (snode->mx < gnode->totr.xmin || snode->mx >= gnode->totr.xmax
				|| snode->my < gnode->totr.ymin || snode->my >= gnode->totr.ymax);
	}
	return 0;
}

/* ****************** Add *********************** */


typedef struct bNodeListItem {
	struct bNodeListItem *next, *prev;
	struct bNode *node;	
} bNodeListItem;

static int sort_nodes_locx(void *a, void *b)
{
	bNodeListItem *nli1 = (bNodeListItem *)a;
	bNodeListItem *nli2 = (bNodeListItem *)b;
	bNode *node1 = nli1->node;
	bNode *node2 = nli2->node;
	
	if (node1->locx > node2->locx)
		return 1;
	else 
		return 0;
}

static int socket_is_available(bNodeTree *ntree, bNodeSocket *sock, int allow_used)
{
	if (sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))
		return 0;
	
	if (!allow_used) {
		if (nodeCountSocketLinks(ntree, sock) > 0)
			return 0;
	}
	return 1;
}

static bNodeSocket *best_socket_output(bNodeTree *ntree, bNode *node, bNodeSocket *sock_target, int allow_multiple)
{
	bNodeSocket *sock;
	
	/* first try to find a socket with a matching name */
	for (sock=node->outputs.first; sock; sock=sock->next) {

		if (!socket_is_available(ntree, sock, allow_multiple))
			continue;

		/* check for same types */
		if (sock->type == sock_target->type) {
			if (strcmp(sock->name, sock_target->name)==0)
				return sock;
		}
	}
	
	/* otherwise settle for the first available socket of the right type */
	for (sock=node->outputs.first; sock; sock=sock->next) {

		if (!socket_is_available(ntree, sock, allow_multiple))
			continue;
		
		/* check for same types */
		if (sock->type == sock_target->type) {
			return sock;
		}
	}
	
	return NULL;
}

/* this is a bit complicated, but designed to prioritise finding 
 * sockets of higher types, such as image, first */
static bNodeSocket *best_socket_input(bNodeTree *ntree, bNode *node, int num, int replace)
{
	bNodeSocket *sock;
	int socktype, maxtype=0;
	int a = 0;
	
	for (sock=node->inputs.first; sock; sock=sock->next) {
		maxtype = MAX2(sock->type, maxtype);
	}
	
	/* find sockets of higher 'types' first (i.e. image) */
	for (socktype=maxtype; socktype >= 0; socktype--) {
		for (sock=node->inputs.first; sock; sock=sock->next) {
			
			if (!socket_is_available(ntree, sock, replace)) {
				a++;
				continue;
			}
				
			if (sock->type == socktype) {
				/* increment to make sure we don't keep finding 
				 * the same socket on every attempt running this function */
				a++;
				if (a > num)
					return sock;
			}
		}
	}
	
	return NULL;
}

void snode_autoconnect(SpaceNode *snode, int allow_multiple, int replace)
{
	ListBase *nodelist = MEM_callocN(sizeof(ListBase), "items_list");
	bNodeListItem *nli;
	bNode *node;
	int i, numlinks=0;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & NODE_SELECT) {
			nli = MEM_mallocN(sizeof(bNodeListItem), "temporary node list item");
			nli->node = node;
			BLI_addtail(nodelist, nli);
		}
	}
	
	/* sort nodes left to right */
	BLI_sortlist(nodelist, sort_nodes_locx);
	
	for (nli=nodelist->first; nli; nli=nli->next) {
		bNode *node_fr, *node_to;
		bNodeSocket *sock_fr, *sock_to;
		
		if (nli->next == NULL) break;
		
		node_fr = nli->node;
		node_to = nli->next->node;
		
		/* check over input sockets first */
		for (i=0; i<BLI_countlist(&node_to->inputs); i++) {
			
			/* find the best guess input socket */
			sock_to = best_socket_input(snode->edittree, node_to, i, replace);
			if (!sock_to) continue;
			
			/* check for an appropriate output socket to connect from */
			sock_fr = best_socket_output(snode->edittree, node_fr, sock_to, allow_multiple);
			if (!sock_fr) continue;
			
			/* then we can connect */
			if (replace)
				nodeRemSocketLinks(snode->edittree, sock_to);
			nodeAddLink(snode->edittree, node_fr, sock_fr, node_to, sock_to);
			snode_tag_changed(snode, node_to);
			++numlinks;
			break;
		}
	}
	
	if (numlinks > 0) {
		node_tree_verify_groups(snode->nodetree);
		ntreeSolveOrder(snode->edittree);
	}
	
	BLI_freelistN(nodelist);
	MEM_freeN(nodelist);
}

/* can be called from menus too, but they should do own undopush and redraws */
bNode *node_add_node(SpaceNode *snode, Scene *scene, int type, float locx, float locy)
{
	bNode *node= NULL, *gnode;
	
	node_deselectall(snode);
	
	if(type>=NODE_DYNAMIC_MENU) {
		node= nodeAddNodeType(snode->edittree, type, NULL, NULL);
	}
	else if(type>=NODE_GROUP_MENU) {
		if(snode->edittree!=snode->nodetree) {
			// XXX error("Can not add a Group in a Group");
			return NULL;
		}
		else {
			bNodeTree *ngroup= BLI_findlink(&G.main->nodetree, type-NODE_GROUP_MENU);
			if(ngroup)
				node= nodeAddNodeType(snode->edittree, NODE_GROUP, ngroup, NULL);
		}
	}
	else
		node= nodeAddNodeType(snode->edittree, type, NULL, NULL);
	
	/* generics */
	if(node) {
		node->locx= locx;
		node->locy= locy + 60.0f;		// arbitrary.. so its visible, (0,0) is top of node
		node->flag |= SELECT;
		
		gnode= node_tree_get_editgroup(snode->nodetree);
		if(gnode) {
			node->locx -= gnode->locx;
			node->locy -= gnode->locy;
		}

		node_tree_verify_groups(snode->nodetree);
		node_set_active(snode, node);
		
		if(snode->nodetree->type==NTREE_COMPOSIT) {
			if(ELEM4(node->type, CMP_NODE_R_LAYERS, CMP_NODE_COMPOSITE, CMP_NODE_DEFOCUS, CMP_NODE_OUTPUT_FILE))
				node->id = &scene->id;
			
			ntreeCompositForceHidden(snode->edittree, scene);
		}
			
		if(node->id)
			id_us_plus(node->id);
			
		snode_tag_changed(snode, node);
	}
	
	if(snode->nodetree->type==NTREE_TEXTURE) {
		ntreeTexCheckCyclics(snode->edittree);
	}
	
	return node;
}

/* ****************** Duplicate *********************** */

static int node_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree= snode->edittree;
	bNode *node, *newnode, *last;
	
	ED_preview_kill_jobs(C);
	
	last = ntree->nodes.last;
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			newnode = nodeCopyNode(ntree, node);
			
			/* deselect old node, select the copy instead */
			node->flag &= ~(NODE_SELECT|NODE_ACTIVE);
			newnode->flag |= NODE_SELECT;
			
			if(newnode->id) {
				/* simple id user adjustment, node internal functions dont touch this
				 * but operators and readfile.c do. */
				id_us_plus(newnode->id);
				/* to ensure redraws or rerenders happen */
				ED_node_changed_update(snode->id, newnode);
			}
		}
		
		/* make sure we don't copy new nodes again! */
		if (node==last)
			break;
	}
	
	ntreeSolveOrder(ntree);
	
	node_tree_verify_groups(snode->nodetree);
	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Nodes";
	ot->description = "Duplicate the nodes";
	ot->idname= "NODE_OT_duplicate";
	
	/* api callbacks */
	ot->exec= node_duplicate_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************************** add link op ******************** */

static void node_remove_extra_links(SpaceNode *snode, bNodeSocket *tsock, bNodeLink *link)
{
	bNodeLink *tlink;
	bNodeSocket *sock;
	
	if(tsock && nodeCountSocketLinks(snode->edittree, link->tosock) > tsock->limit) {
		
		for(tlink= snode->edittree->links.first; tlink; tlink= tlink->next) {
			if(link!=tlink && tlink->tosock==link->tosock)
				break;
		}
		if(tlink) {
			/* try to move the existing link to the next available socket */
			if (tlink->tonode) {
				/* is there a free input socket with same type? */
				for(sock= tlink->tonode->inputs.first; sock; sock= sock->next) {
					if(sock->type==tlink->fromsock->type)
						if(nodeCountSocketLinks(snode->edittree, sock) < sock->limit)
							break;
				}
				if(sock) {
					tlink->tosock= sock;
					sock->flag &= ~SOCK_HIDDEN;
				}
				else {
					nodeRemLink(snode->edittree, tlink);
				}
			}
			else
				nodeRemLink(snode->edittree, tlink);
		}
	}
}

/* loop that adds a nodelink, called by function below  */
/* in_out = starting socket */
static int node_link_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNodeLinkDrag *nldrag= op->customdata;
	bNode *tnode, *node;
	bNodeSocket *tsock= NULL, *sock;
	bNodeLink *link;
	int in_out;

	in_out= nldrag->in_out;
	node= nldrag->node;
	sock= nldrag->sock;
	link= nldrag->link;
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
							 &snode->mx, &snode->my);

	switch (event->type) {
		case MOUSEMOVE:
			
			if(in_out==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if( link->tosock!= tsock && (!tnode || (tnode!=node && link->tonode!=tnode)) ) {
							link->tonode= tnode;
							link->tosock= tsock;
							if (link->prev==NULL && link->next==NULL)
								BLI_addtail(&snode->edittree->links, link);
							ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
						}
					}
				}
				else {
					BLI_remlink(&snode->edittree->links, link);
					link->prev = link->next = NULL;
					link->tonode= NULL;
					link->tosock= NULL;
				}
			}
			else {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_OUT)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(nodeCountSocketLinks(snode->edittree, tsock) < tsock->limit) {
							if( link->fromsock!= tsock && (!tnode || (tnode!=node && link->fromnode!=tnode)) ) {
								link->fromnode= tnode;
								link->fromsock= tsock;
								if (link->prev==NULL && link->next==NULL)
									BLI_addtail(&snode->edittree->links, link);
								ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
							}
						}
					}
				}
				else {
					BLI_remlink(&snode->edittree->links, link);
					link->prev = link->next = NULL;
					link->fromnode= NULL;
					link->fromsock= NULL;
				}
			}
			/* hilight target sockets only */
			node_socket_hilights(snode, in_out==SOCK_OUT?SOCK_IN:SOCK_OUT);
			ED_region_tag_redraw(ar);
			break;
			
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			if(link->tosock && link->fromsock) {
				/* send changed events for original tonode and new */
				snode_tag_changed(snode, link->tonode);
				
				/* we might need to remove a link */
				if(in_out==SOCK_OUT)
					node_remove_extra_links(snode, link->tosock, link);
			}
			else if (outside_group_rect(snode) && (link->tonode || link->fromnode)) {
				/* automatically add new group socket */
				if (link->tonode && link->tosock) {
					link->fromsock = nodeGroupExposeSocket(snode->edittree, link->tosock, SOCK_IN);
					link->fromnode = NULL;
					if (link->prev==NULL && link->next==NULL)
						BLI_addtail(&snode->edittree->links, link);
				}
				else if (link->fromnode && link->fromsock) {
					link->tosock = nodeGroupExposeSocket(snode->edittree, link->fromsock, SOCK_OUT);
					link->tonode = NULL;
					if (link->prev==NULL && link->next==NULL)
						BLI_addtail(&snode->edittree->links, link);
				}
			}
			else
				nodeRemLink(snode->edittree, link);
			
			ntreeSolveOrder(snode->edittree);
			node_tree_verify_groups(snode->nodetree);
			snode_notify(C, snode);
			snode_dag_update(C, snode);
			
			BLI_remlink(&snode->linkdrag, nldrag);
			MEM_freeN(nldrag);
			
			return OPERATOR_FINISHED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

/* return 1 when socket clicked */
static int node_link_init(SpaceNode *snode, bNodeLinkDrag *nldrag)
{
	bNodeLink *link;

	/* output indicated? */
	if(find_indicated_socket(snode, &nldrag->node, &nldrag->sock, SOCK_OUT)) {
		if(nodeCountSocketLinks(snode->edittree, nldrag->sock) < nldrag->sock->limit)
			return SOCK_OUT;
		else {
			/* find if we break a link */
			for(link= snode->edittree->links.first; link; link= link->next) {
				if(link->fromsock==nldrag->sock)
					break;
			}
			if(link) {
				nldrag->node= link->tonode;
				nldrag->sock= link->tosock;
				nodeRemLink(snode->edittree, link);
				return SOCK_IN;
			}
		}
	}
	/* or an input? */
	else if(find_indicated_socket(snode, &nldrag->node, &nldrag->sock, SOCK_IN)) {
		if(nodeCountSocketLinks(snode->edittree, nldrag->sock) < nldrag->sock->limit)
			return SOCK_IN;
		else {
			/* find if we break a link */
			for(link= snode->edittree->links.first; link; link= link->next) {
				if(link->tosock==nldrag->sock)
					break;
			}
			if(link) {
				/* send changed event to original tonode */
				if(link->tonode) 
					snode_tag_changed(snode, link->tonode);
				
				nldrag->node= link->fromnode;
				nldrag->sock= link->fromsock;
				nodeRemLink(snode->edittree, link);
				return SOCK_OUT;
			}
		}
	}
	
	return 0;
}

static int node_link_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNodeLinkDrag *nldrag= MEM_callocN(sizeof(bNodeLinkDrag), "drag link op customdata");
	
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
							 &snode->mx, &snode->my);

	ED_preview_kill_jobs(C);

	nldrag->in_out= node_link_init(snode, nldrag);
		
	if(nldrag->in_out) {
		op->customdata= nldrag;
		
		/* we make a temporal link */
		if(nldrag->in_out==SOCK_OUT) {
			nldrag->link= MEM_callocN(sizeof(bNodeLink), "link");
			nldrag->link->fromnode= nldrag->node;
			nldrag->link->fromsock= nldrag->sock;
			nldrag->link->tonode= NULL;
			nldrag->link->tosock= NULL;
		}
		else {
			nldrag->link= MEM_callocN(sizeof(bNodeLink), "link");
			nldrag->link->fromnode= NULL;
			nldrag->link->fromsock= NULL;
			nldrag->link->tonode= nldrag->node;
			nldrag->link->tosock= nldrag->sock;
		}
		BLI_addtail(&snode->linkdrag, nldrag);
		
		/* add modal handler */
		WM_event_add_modal_handler(C, op);
		
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		MEM_freeN(nldrag);
		return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
	}
}

static int node_link_cancel(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeLinkDrag *nldrag= op->customdata;

	nodeRemLink(snode->edittree, nldrag->link);
	BLI_remlink(&snode->linkdrag, nldrag);
	MEM_freeN(nldrag);

	return OPERATOR_CANCELLED;
}

void NODE_OT_link(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Link Nodes";
	ot->idname= "NODE_OT_link";
	
	/* api callbacks */
	ot->invoke= node_link_invoke;
	ot->modal= node_link_modal;
//	ot->exec= node_link_exec;
	ot->poll= ED_operator_node_active;
	ot->cancel= node_link_cancel;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

/* ********************** Make Link operator ***************** */

/* makes a link between selected output and input sockets */
static int node_make_link_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	int replace = RNA_boolean_get(op->ptr, "replace");

	ED_preview_kill_jobs(C);

	snode_autoconnect(snode, 1, replace);

	node_tree_verify_groups(snode->nodetree);
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_link_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Links";
	ot->description= "Makes a link between selected output in input sockets";
	ot->idname= "NODE_OT_link_make";
	
	/* callbacks */
	ot->exec= node_make_link_exec;
	ot->poll= ED_operator_node_active; // XXX we need a special poll which checks that there are selected input/output sockets
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "replace", 0, "Replace", "Replace socket connections with the new links");
}

/* ********************** Cut Link operator ***************** */

#define LINK_RESOL 12
static int cut_links_intersect(bNodeLink *link, float mcoords[][2], int tot)
{
	float coord_array[LINK_RESOL+1][2];
	int i, b;
	
	if(node_link_bezier_points(NULL, NULL, link, coord_array, LINK_RESOL)) {

		for(i=0; i<tot-1; i++)
			for(b=0; b<LINK_RESOL; b++)
				if(isect_line_line_v2(mcoords[i], mcoords[i+1], coord_array[b], coord_array[b+1]) > 0)
					return 1;
	}
	return 0;
}

static int cut_links_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	float mcoords[256][2];
	int i= 0;
	
	RNA_BEGIN(op->ptr, itemptr, "path") {
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		UI_view2d_region_to_view(&ar->v2d, (short)loc[0], (short)loc[1], 
								 &mcoords[i][0], &mcoords[i][1]);
		i++;
		if(i>= 256) break;
	}
	RNA_END;
	
	if(i>1) {
		bNodeLink *link, *next;

		ED_preview_kill_jobs(C);
		
		for(link= snode->edittree->links.first; link; link= next) {
			next= link->next;
			
			if(cut_links_intersect(link, mcoords, i)) {
				snode_tag_changed(snode, link->tonode);
				nodeRemLink(snode->edittree, link);
			}
		}
		
		ntreeSolveOrder(snode->edittree);
		node_tree_verify_groups(snode->nodetree);
		snode_notify(C, snode);
		snode_dag_update(C, snode);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
}

void NODE_OT_links_cut(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name= "Cut links";
	ot->idname= "NODE_OT_links_cut";
	
	ot->invoke= WM_gesture_lines_invoke;
	ot->modal= WM_gesture_lines_modal;
	ot->exec= cut_links_exec;
	ot->cancel= WM_gesture_lines_cancel;
	
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	prop= RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_KNIFECURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/* ******************************** */
// XXX some code needing updating to operators...


/* goes over all scenes, reads render layers */
static int node_read_renderlayers_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain= CTX_data_main(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *curscene= CTX_data_scene(C), *scene;
	bNode *node;

	ED_preview_kill_jobs(C);

	/* first tag scenes unread */
	for(scene= bmain->scene.first; scene; scene= scene->id.next) 
		scene->id.flag |= LIB_DOIT;

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			ID *id= node->id;
			if(id->flag & LIB_DOIT) {
				RE_ReadRenderResult(curscene, (Scene *)id);
				ntreeCompositTagRender((Scene *)id);
				id->flag &= ~LIB_DOIT;
			}
		}
	}
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_read_renderlayers(wmOperatorType *ot)
{
	
	ot->name= "Read Render Layers";
	ot->idname= "NODE_OT_read_renderlayers";
	
	ot->exec= node_read_renderlayers_exec;
	
	ot->poll= composite_node_active;
	
	/* flags */
	ot->flag= 0;
}

static int node_read_fullsamplelayers_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain= CTX_data_main(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *curscene= CTX_data_scene(C);
	Render *re= RE_NewRender(curscene->id.name);

	WM_cursor_wait(1);

	RE_MergeFullSample(re, bmain, curscene, snode->nodetree);
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	WM_cursor_wait(0);
	return OPERATOR_FINISHED;
}


void NODE_OT_read_fullsamplelayers(wmOperatorType *ot)
{
	
	ot->name= "Read Full Sample Layers";
	ot->idname= "NODE_OT_read_fullsamplelayers";
	
	ot->exec= node_read_fullsamplelayers_exec;
	
	ot->poll= composite_node_active;
	
	/* flags */
	ot->flag= 0;
}

int node_render_changed_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *sce= CTX_data_scene(C);
	bNode *node;
	
	for(node= sce->nodetree->nodes.first; node; node= node->next) {
		if(node->id==(ID *)sce && node->need_exec) {
			break;
		}
	}
	if(node) {
		SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
		
		if(srl) {
			PointerRNA op_ptr;
			
			WM_operator_properties_create(&op_ptr, "RENDER_OT_render");
			RNA_string_set(&op_ptr, "layer", srl->name);
			RNA_string_set(&op_ptr, "scene", sce->id.name+2);
			
			/* to keep keypositions */
			sce->r.scemode |= R_NO_FRAME_UPDATE;
			
			WM_operator_name_call(C, "RENDER_OT_render", WM_OP_INVOKE_DEFAULT, &op_ptr);

			WM_operator_properties_free(&op_ptr);
			
			return OPERATOR_FINISHED;
		}
		   
	}
	return OPERATOR_CANCELLED;
}

void NODE_OT_render_changed(wmOperatorType *ot)
{
	
	ot->name= "Render Changed Layer";
	ot->idname= "NODE_OT_render_changed";
	
	ot->exec= node_render_changed_exec;
	
	ot->poll= composite_node_active;
	
	/* flags */
	ot->flag= 0;
}


/* ****************** Make Group operator ******************* */

static int node_group_make_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;
	
	if(snode->edittree!=snode->nodetree) {
		BKE_report(op->reports, RPT_WARNING, "Can not add a new Group in a Group");
		return OPERATOR_CANCELLED;
	}
	
	/* for time being... is too complex to handle */
	if(snode->treetype==NTREE_COMPOSIT) {
		for(gnode=snode->nodetree->nodes.first; gnode; gnode= gnode->next) {
			if(gnode->flag & SELECT)
				if(gnode->type==CMP_NODE_R_LAYERS)
					break;
		}
		
		if(gnode) {
			BKE_report(op->reports, RPT_WARNING, "Can not add RenderLayer in a Group");
			return OPERATOR_CANCELLED;
		}
	}

	ED_preview_kill_jobs(C);
	
	gnode= nodeMakeGroupFromSelected(snode->nodetree);
	if(gnode==NULL) {
		BKE_report(op->reports, RPT_WARNING, "Can not make Group");
		return OPERATOR_CANCELLED;
	}
	else {
		nodeSetActive(snode->nodetree, gnode);
		ntreeSolveOrder(snode->nodetree);
	}
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Group";
	ot->description = "Make group from selected nodes";
	ot->idname = "NODE_OT_group_make";
	
	/* api callbacks */
	ot->exec = node_group_make_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Hide operator *********************** */

static void node_flag_toggle_exec(SpaceNode *snode, int toggle_flag)
{
	int tot_eq= 0, tot_neq= 0;
	bNode *node;

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {

			if(toggle_flag== NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW)==0)
				continue;

			if(node->flag & toggle_flag)
				tot_eq++;
			else
				tot_neq++;
		}
	}
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {

			if(toggle_flag== NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW)==0)
				continue;

			if( (tot_eq && tot_neq) || tot_eq==0)
				node->flag |= toggle_flag;
			else
				node->flag &= ~toggle_flag;
		}
	}
}

static int node_hide_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	/* sanity checking (poll callback checks this already) */
	if((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;
	
	node_flag_toggle_exec(snode, NODE_HIDDEN);
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_hide_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide";
	ot->description= "Toggle hiding of selected nodes";
	ot->idname= "NODE_OT_hide_toggle";
	
	/* callbacks */
	ot->exec= node_hide_exec;
	ot->poll= ED_operator_node_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int node_preview_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);

	/* sanity checking (poll callback checks this already) */
	if((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	node_flag_toggle_exec(snode, NODE_PREVIEW);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_preview_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Node Preview";
	ot->description= "Toggle preview display for selected nodes";
	ot->idname= "NODE_OT_preview_toggle";

	/* callbacks */
	ot->exec= node_preview_exec;
	ot->poll= ED_operator_node_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int node_socket_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	int hidden= 0;

	/* sanity checking (poll callback checks this already) */
	if((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node_has_hidden_sockets(node)) {
				hidden= 1;
				break;
			}
		}
	}

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			node_set_hidden_sockets(snode, node, !hidden);
		}
	}

	node_tree_verify_groups(snode->nodetree);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_hide_socket_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Hidden Node Sockets";
	ot->description= "Toggle unused node socket display";
	ot->idname= "NODE_OT_hide_socket_toggle";

	/* callbacks */
	ot->exec= node_socket_toggle_exec;
	ot->poll= ED_operator_node_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Mute operator *********************** */

static int node_mute_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;

	/* no disabling inside of groups */
	if(node_tree_get_editgroup(snode->nodetree))
		return OPERATOR_CANCELLED;
	
	ED_preview_kill_jobs(C);

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->inputs.first && node->outputs.first) {
				node->flag ^= NODE_MUTED;
				snode_tag_changed(snode, node);
			}
		}
	}
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_mute_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Node Mute";
	ot->description= "Toggle muting of the nodes";
	ot->idname= "NODE_OT_mute_toggle";
	
	/* callbacks */
	ot->exec= node_mute_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Delete operator ******************* */

static int node_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node, *next;
	
	ED_preview_kill_jobs(C);

	for(node= snode->edittree->nodes.first; node; node= next) {
		next= node->next;
		if(node->flag & SELECT) {
			/* check id user here, nodeFreeNode is called for free dbase too */
			if(node->id)
				node->id->us--;
			nodeFreeNode(snode->edittree, node);
		}
	}
	
	node_tree_verify_groups(snode->nodetree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
	ot->description = "Delete selected nodes";
	ot->idname= "NODE_OT_delete";
	
	/* api callbacks */
	ot->exec= node_delete_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Show Cyclic Dependencies Operator  ******************* */

static int node_show_cycles_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	/* this is just a wrapper around this call... */
	ntreeSolveOrder(snode->edittree);
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_show_cyclic_dependencies(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Show Cyclic Dependencies";
	ot->description= "Sort the nodes and show the cyclic dependencies between the nodes";
	ot->idname= "NODE_OT_show_cyclic_dependencies";
	
	/* callbacks */
	ot->exec= node_show_cycles_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Add File Node Operator  ******************* */

static int node_add_file_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	Image *ima= NULL;
	int ntype=0;

	/* check input variables */
	if (RNA_property_is_set(op->ptr, "filepath"))
	{
		char path[FILE_MAX];
		RNA_string_get(op->ptr, "filepath", path);

		errno= 0;

		ima= BKE_add_image_file(path);

		if(!ima) {
			BKE_reportf(op->reports, RPT_ERROR, "Can't read: \"%s\", %s.", path, errno ? strerror(errno) : "Unsupported image format");
			return OPERATOR_CANCELLED;
		}
	}
	else if(RNA_property_is_set(op->ptr, "name"))
	{
		char name[32];
		RNA_string_get(op->ptr, "name", name);
		ima= (Image *)find_id("IM", name);

		if(!ima) {
			BKE_reportf(op->reports, RPT_ERROR, "Image named \"%s\", not found.", name);
			return OPERATOR_CANCELLED;
		}
	}
	
	node_deselectall(snode);
	
	if (snode->nodetree->type==NTREE_COMPOSIT)
		ntype = CMP_NODE_IMAGE;

	ED_preview_kill_jobs(C);
	
	node = node_add_node(snode, scene, ntype, snode->mx, snode->my);
	
	if (!node) {
		BKE_report(op->reports, RPT_WARNING, "Could not add an image node.");
		return OPERATOR_CANCELLED;
	}
	
	node->id = (ID *)ima;
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

static int node_add_file_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	
	/* convert mouse coordinates to v2d space */
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
							 &snode->mx, &snode->my);
	
	if (RNA_property_is_set(op->ptr, "filepath") || RNA_property_is_set(op->ptr, "name"))
		return node_add_file_exec(C, op);
	else
		return WM_operator_filesel(C, op, event);
}

void NODE_OT_add_file(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add File Node";
	ot->description= "Add a file node to the current node editor";
	ot->idname= "NODE_OT_add_file";
	
	/* callbacks */
	ot->exec= node_add_file_exec;
	ot->invoke= node_add_file_invoke;
	ot->poll= composite_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH);  //XXX TODO, relative_path
	RNA_def_string(ot->srna, "name", "Image", 24, "Name", "Datablock name to assign.");
}

