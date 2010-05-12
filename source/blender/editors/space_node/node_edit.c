/**
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_storage_types.h"

#include "RE_pipeline.h"

#include "IMB_imbuf_types.h"

#include "ED_node.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "node_intern.h"

#define SOCK_IN		1
#define SOCK_OUT	2

/* ***************** composite job manager ********************** */

typedef struct CompoJob {
	Scene *scene;
	bNodeTree *ntree;
	bNodeTree *localtree;
	short *stop;
	short *do_update;
} CompoJob;

/* called by compo, only to check job 'stop' value */
static int compo_breakjob(void *cjv)
{
	CompoJob *cj= cjv;
	
	return *(cj->stop);
}

/* called by compo, wmJob sends notifier */
static void compo_redrawjob(void *cjv, char *str)
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


/* only this runs inside thread */
static void compo_startjob(void *cjv, short *stop, short *do_update)
{
	CompoJob *cj= cjv;
	bNodeTree *ntree= cj->localtree;

	if(cj->scene->use_nodes==0)
		return;
	
	cj->stop= stop;
	cj->do_update= do_update;
	
	ntree->test_break= compo_breakjob;
	ntree->tbh= cj;
	ntree->stats_draw= compo_redrawjob;
	ntree->sdh= cj;
	
	// XXX BIF_store_spare();
	
	ntreeCompositExecTree(ntree, &cj->scene->r, 1);	/* 1 is do_previews */
	
	ntree->test_break= NULL;
	ntree->stats_draw= NULL;

}

void snode_composite_job(const bContext *C, ScrArea *sa)
{
	SpaceNode *snode= sa->spacedata.first;
	wmJob *steve;
	CompoJob *cj;

	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, WM_JOB_EXCL_RENDER);
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

/* also checks for edited groups */
bNode *editnode_get_active(bNodeTree *ntree)
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

void snode_notify(bContext *C, SpaceNode *snode)
{
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
	
	ma->nodetree= ntreeAddTree(NTREE_SHADER);
	
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
	
	sce->nodetree= ntreeAddTree(NTREE_COMPOSIT);
	
	out= nodeAddNodeType(sce->nodetree, CMP_NODE_COMPOSITE, NULL, NULL);
	out->locx= 300.0f; out->locy= 400.0f;
	out->id= &sce->id;
	
	in= nodeAddNodeType(sce->nodetree, CMP_NODE_R_LAYERS, NULL, NULL);
	in->locx= 10.0f; in->locy= 400.0f;
	in->id= &sce->id;
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
	
	tx->nodetree= ntreeAddTree(NTREE_TEXTURE);
	
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
			Brush *brush= NULL;
			
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

void node_set_active(SpaceNode *snode, bNode *node)
{
	nodeSetActive(snode->edittree, node);
	
	if(node->type!=NODE_GROUP) {
		/* tree specific activate calls */
		if(snode->treetype==NTREE_SHADER) {
			/* when we select a material, active texture is cleared, for buttons */
			if(node->id && GS(node->id->name)==ID_MA)
				nodeClearActiveID(snode->edittree, ID_TE);

			// XXX
#if 0
			if(node->id)
				; // XXX BIF_preview_changed(-1);	/* temp hack to force texture preview to update */
			
			// allqueue(REDRAWBUTSSHADING, 1);
			// allqueue(REDRAWIPO, 0);
#endif
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			Scene *scene= (Scene*)snode->id;

			/* make active viewer, currently only 1 supported... */
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				bNode *tnode;
				int was_output= (node->flag & NODE_DO_OUTPUT);

				for(tnode= snode->edittree->nodes.first; tnode; tnode= tnode->next)
					if( ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if(was_output==0) {
					bNode *gnode;
					
					NodeTagChanged(snode->edittree, node);
					
					/* if inside group, tag entire group */
					gnode= node_tree_get_editgroup(snode->nodetree);
					if(gnode)
						NodeTagIDChanged(snode->nodetree, gnode->id);
					
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
		nodeVerifyGroup((bNodeTree *)gnode->id);
	
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

static int node_group_edit_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	gnode= nodeGetActive(snode->edittree);
	snode_make_group_editable(snode, gnode);

	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);

	return OPERATOR_FINISHED;
}

static int node_group_edit_invoke(bContext *C, wmOperator *op, wmEvent *event)
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

/* ******************** Ungroup operator ********************** */

static int node_group_ungroup_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	/* are we inside of a group? */
	gnode= node_tree_get_editgroup(snode->nodetree);
	if(gnode)
		snode_make_group_editable(snode, NULL);
	
	gnode= nodeGetActive(snode->edittree);
	if(gnode==NULL)
		return OPERATOR_CANCELLED;
	
	if(gnode->type!=NODE_GROUP) {
		BKE_report(op->reports, RPT_ERROR, "Not a group");
		return OPERATOR_CANCELLED;
	}
	else if(!nodeGroupUnGroup(snode->edittree, gnode)) {
		BKE_report(op->reports, RPT_ERROR, "Can't ungroup");
		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);

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
	short mvalo[2];
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
	
	ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf= BKE_image_get_ibuf(ima, NULL);
	
	if(ibuf == NULL)
		return OPERATOR_CANCELLED;

	nvm= MEM_callocN(sizeof(NodeViewMove), "NodeViewMove struct");
	op->customdata= nvm;
	nvm->mvalo[0]= event->mval[0];
	nvm->mvalo[1]= event->mval[1];

	nvm->xmin = -(ar->winx/2) - ibuf->x/2 + pad;
	nvm->xmax = ar->winx/2 + ibuf->x/2 - pad;
	nvm->ymin = -(ar->winy/2) - ibuf->y/2 + pad;
	nvm->ymax = ar->winy/2 + ibuf->y/2 - pad;
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}


void NODE_OT_backimage_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Background Image Move";
	ot->idname= "NODE_OT_backimage_move";
	
	/* api callbacks */
	ot->invoke= snode_bg_viewmove_invoke;
	ot->modal= snode_bg_viewmove_modal;
	ot->poll= ED_operator_node_active;
	
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
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* internal */
	RNA_def_float(ot->srna, "factor", 1.2f, 0.0f, 10.0f, "Factor", "", 0.0f, 10.0f);
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
			
			UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, 
									 &mx, &my);
			
			if (node) {
				if(node->flag & NODE_HIDDEN) {
					node->miniwidth= nsw->oldwidth + mx - nsw->mxstart;
					CLAMP(node->miniwidth, 0.0f, 100.0f);
				}
				else {
					node->width= nsw->oldwidth + mx - nsw->mxstart;
					CLAMP(node->width, node->typeinfo->minwidth, node->typeinfo->maxwidth);
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
		UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, 
								 &snode->mx, &snode->my);
		
		/* rect we're interested in is just the bottom right corner */
		totr= node->totr;
		totr.xmin= totr.xmax-10.0f;
		totr.ymax= totr.ymin+10.0f;
		
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

void NODE_OT_resize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Resize Node";
	ot->idname= "NODE_OT_resize";
	
	/* api callbacks */
	ot->invoke= node_resize_invoke;
	ot->modal= node_resize_modal;
	ot->poll= ED_operator_node_active;
	
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
		bNodeSocket *sock;
		
		/* get a good socket to view from */
		for(sock= tonode->outputs.first; sock; sock= sock->next)
			if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
				break;
		
		if(sock) {
			bNodeLink *link;
			
			/* get link to viewer */
			for(link= snode->edittree->links.first; link; link= link->next)
				if(link->tonode==node)
					break;
			
			if(link==NULL) {
				nodeAddLink(snode->edittree, tonode, sock, node, node->inputs.first);
			}
			else {
				link->fromnode= tonode;
				link->fromsock= sock;
			}
			ntreeSolveOrder(snode->edittree);
			NodeTagChanged(snode->edittree, node);
		}
	}
}


static int node_active_link_viewer(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	
	
	node= editnode_get_active(snode->edittree);
	
	if(node) {
		node_link_viewer(snode, node);
		snode_notify(C, snode);
	}
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
/*static*/ int node_mouse_groupheader(SpaceNode *snode)
{
	bNode *gnode;
	float mx=0, my=0;
// XXX	short mval[2];
	
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

/* ****************** Add *********************** */


typedef struct bNodeListItem {
	struct bNodeListItem *next, *prev;
	struct bNode *node;	
} bNodeListItem;

int sort_nodes_locx(void *a, void *b)
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
	int i;
	
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
			NodeTagChanged(snode->edittree, node_to);
			break;
		}
	}
	
	ntreeSolveOrder(snode->edittree);
	
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
		node->locy= locy + 60.0f;		// arbitrary.. so its visible
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
			
		NodeTagChanged(snode->edittree, node);
	}
	
	if(snode->nodetree->type==NTREE_TEXTURE) {
		ntreeTexCheckCyclics(snode->edittree);
	}
	
	return node;
}

/* ****************** Duplicate *********************** */

static int node_duplicate_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	ntreeCopyTree(snode->edittree, 1);	/* 1 == internally selected nodes */
	
	ntreeSolveOrder(snode->edittree);
	node_tree_verify_groups(snode->nodetree);
	snode_notify(C, snode);

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

/* temp data to pass on to modal */
typedef struct NodeLinkDrag
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	int in_out;
} NodeLinkDrag;

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
	}
}

/* loop that adds a nodelink, called by function below  */
/* in_out = starting socket */
static int node_link_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	NodeLinkDrag *nldrag= op->customdata;
	bNode *tnode, *node;
	bNodeSocket *tsock= NULL, *sock;
	bNodeLink *link;
	int in_out;

	in_out= nldrag->in_out;
	node= nldrag->node;
	sock= nldrag->sock;
	link= nldrag->link;
	
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, 
							 &snode->mx, &snode->my);

	switch (event->type) {
		case MOUSEMOVE:
			
			if(in_out==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(tnode!=node  && link->tonode!=tnode && link->tosock!= tsock) {
							link->tonode= tnode;
							link->tosock= tsock;
							ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
						}
					}
				}
				else {
					link->tonode= NULL;
					link->tosock= NULL;
				}
			}
			else {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_OUT)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(nodeCountSocketLinks(snode->edittree, tsock) < tsock->limit) {
							if(tnode!=node && link->fromnode!=tnode && link->fromsock!= tsock) {
								link->fromnode= tnode;
								link->fromsock= tsock;
								ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
							}
						}
					}
				}
				else {
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
	
			/* remove link? */
			if(link->tonode==NULL || link->fromnode==NULL) {
				nodeRemLink(snode->edittree, link);
			}
			else {
				/* send changed events for original tonode and new */
				if(link->tonode) 
					NodeTagChanged(snode->edittree, link->tonode);
				
				/* we might need to remove a link */
				if(in_out==SOCK_OUT) node_remove_extra_links(snode, link->tosock, link);
			}
			
			ntreeSolveOrder(snode->edittree);
			node_tree_verify_groups(snode->nodetree);
			snode_notify(C, snode);
			
			MEM_freeN(op->customdata);
			op->customdata= NULL;
			
			return OPERATOR_FINISHED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

/* return 1 when socket clicked */
static int node_link_init(SpaceNode *snode, NodeLinkDrag *nldrag)
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
					NodeTagChanged(snode->edittree, link->tonode);
				
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
	NodeLinkDrag *nldrag= MEM_callocN(sizeof(NodeLinkDrag), "drag link op customdata");
	
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, 
							 &snode->mx, &snode->my);

	nldrag->in_out= node_link_init(snode, nldrag);
		
	if(nldrag->in_out) {
		op->customdata= nldrag;
		
		/* we make a temporal link */
		if(nldrag->in_out==SOCK_OUT)
			nldrag->link= nodeAddLink(snode->edittree, nldrag->node, nldrag->sock, NULL, NULL);
		else
			nldrag->link= nodeAddLink(snode->edittree, NULL, NULL, nldrag->node, nldrag->sock);
		
		/* add modal handler */
		WM_event_add_modal_handler(C, op);
		
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		MEM_freeN(nldrag);
		return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
	}
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
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

/* ********************** Make Link operator ***************** */

/* makes a link between selected output and input sockets */
static int node_make_link_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	int replace = RNA_boolean_get(op->ptr, "replace");

	snode_autoconnect(snode, 0, replace);

	node_tree_verify_groups(snode->nodetree);
	snode_notify(C, snode);
	
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
			for(b=0; b<LINK_RESOL-1; b++)
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
		
		for(link= snode->edittree->links.first; link; link= next) {
			next= link->next;
			
			if(cut_links_intersect(link, mcoords, i)) {
				NodeTagChanged(snode->edittree, link->tonode);
				nodeRemLink(snode->edittree, link);
			}
		}
		
		ntreeSolveOrder(snode->edittree);
		node_tree_verify_groups(snode->nodetree);
		snode_notify(C, snode);
		
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
static int node_read_renderlayers_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *curscene= CTX_data_scene(C), *scene;
	bNode *node;

	/* first tag scenes unread */
	for(scene= G.main->scene.first; scene; scene= scene->id.next) 
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
	return OPERATOR_FINISHED;
}

void NODE_OT_read_renderlayers(wmOperatorType *ot)
{
	
	ot->name= "Read Render Layers";
	ot->idname= "NODE_OT_read_renderlayers";
	
	ot->exec= node_read_renderlayers_exec;
	
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= 0;
}

static int node_read_fullsamplelayers_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *curscene= CTX_data_scene(C);
	Render *re= RE_NewRender(curscene->id.name);

//	WM_cursor_wait(1);

	RE_MergeFullSample(re, curscene, snode->nodetree);
	snode_notify(C, snode);
	
//	WM_cursor_wait(0);
	return OPERATOR_FINISHED;
}


void NODE_OT_read_fullsamplelayers(wmOperatorType *ot)
{
	
	ot->name= "Read Full Sample Layers";
	ot->idname= "NODE_OT_read_fullsamplelayers";
	
	ot->exec= node_read_fullsamplelayers_exec;
	
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= 0;
}


/* ************************* */

void imagepaint_composite_tags(bNodeTree *ntree, Image *image, ImageUser *iuser)
{
	bNode *node;
	
	if(ntree==NULL)
		return;
	
	/* search for renderresults */
	if(image->type==IMA_TYPE_R_RESULT) {
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				/* imageuser comes from ImageWin, so indexes are offset 1 */
				if(node->custom1==iuser->layer-1)
					NodeTagChanged(ntree, node);
			}
		}
	}
	else {
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->id== &image->id)
				NodeTagChanged(ntree, node);
		}
	}
}

/* ****************** Make Group operator ******************* */

static int node_group_make_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;
	
	if(snode->edittree!=snode->nodetree) {
		BKE_report(op->reports, RPT_ERROR, "Can not add a new Group in a Group");
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
			BKE_report(op->reports, RPT_ERROR, "Can not add RenderLayer in a Group");
			return OPERATOR_CANCELLED;
		}
	}
	
	gnode= nodeMakeGroupFromSelected(snode->nodetree);
	if(gnode==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Can not make Group");
		return OPERATOR_CANCELLED;
	}
	else {
		nodeSetActive(snode->nodetree, gnode);
		ntreeSolveOrder(snode->nodetree);
	}
	
	snode_notify(C, snode);
	
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

static int node_hide_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	int nothidden=0, ishidden=0;
	
	/* sanity checking (poll callback checks this already) */
	if((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_HIDDEN)
				ishidden++;
			else
				nothidden++;
		}
	}
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if( (ishidden && nothidden) || ishidden==0)
				node->flag |= NODE_HIDDEN;
			else 
				node->flag &= ~NODE_HIDDEN;
		}
	}
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide";
	ot->description= "Toggle hiding of the nodes";
	ot->idname= "NODE_OT_hide";
	
	/* callbacks */
	ot->exec= node_hide_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Mute operator *********************** */

static int node_mute_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;

	/* no disabling inside of groups */
	if(node_tree_get_editgroup(snode->nodetree))
		return OPERATOR_CANCELLED;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->inputs.first && node->outputs.first) {
				node->flag ^= NODE_MUTED;
				NodeTagChanged(snode->edittree, node);
			}
		}
	}
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_mute(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mute";
	ot->description= "Toggle muting of the nodes";
	ot->idname= "NODE_OT_mute";
	
	/* callbacks */
	ot->exec= node_mute_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Delete operator ******************* */

static int node_delete_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node, *next;
	
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

static int node_show_cycles_exec(bContext *C, wmOperator *op)
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
	if (RNA_property_is_set(op->ptr, "path"))
	{
		char path[FILE_MAX];
		RNA_string_get(op->ptr, "path", path);
		ima= BKE_add_image_file(path, scene ? scene->r.cfra : 1);
	}
	else if(RNA_property_is_set(op->ptr, "name"))
	{
		char name[32];
		RNA_string_get(op->ptr, "name", name);
		ima= (Image *)find_id("IM", name);
	}
	
	if(!ima) {
		BKE_report(op->reports, RPT_ERROR, "Not an Image.");
		return OPERATOR_CANCELLED;
	}
	
	
	node_deselectall(snode);
	
	if (snode->nodetree->type==NTREE_COMPOSIT)
		ntype = CMP_NODE_IMAGE;
	
	node = node_add_node(snode, scene, ntype, snode->mx, snode->my);
	
	if (!node) {
		BKE_report(op->reports, RPT_ERROR, "Could not add an image node.");
		return OPERATOR_CANCELLED;
	}
	
	node->id = (ID *)ima;
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

static int node_add_file_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	
	/* convert mouse coordinates to v2d space */
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, 
							 &snode->mx, &snode->my);
	
	if (RNA_property_is_set(op->ptr, "path") || RNA_property_is_set(op->ptr, "name"))
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
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE, FILE_SPECIAL, FILE_OPENFILE);
	RNA_def_string(ot->srna, "name", "Image", 24, "Name", "Datablock name to assign.");
}

