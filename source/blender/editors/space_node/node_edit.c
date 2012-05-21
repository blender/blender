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

#include "DNA_ID.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_report.h"

#include "RE_pipeline.h"

#include "IMB_imbuf_types.h"

#include "ED_node.h"
#include "ED_image.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_render.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "IMB_imbuf.h"

#include "RNA_enum_types.h"

#include "GPU_material.h"

#include "node_intern.h"

static EnumPropertyItem socket_in_out_items[] = {
	{ SOCK_IN, "SOCK_IN", 0, "Input", "" },
	{ SOCK_OUT, "SOCK_OUT", 0, "Output", "" },
	{ 0, NULL, 0, NULL, NULL },
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
	
	*(cj->do_update) = TRUE;
}

static void compo_freejob(void *cjv)
{
	CompoJob *cj= cjv;

	if (cj->localtree) {
		ntreeLocalMerge(cj->localtree, cj->ntree);
	}
	MEM_freeN(cj);
}

/* only now we copy the nodetree, so adding many jobs while
 * sliding buttons doesn't frustrate */
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

	if (cj->scene->use_nodes == FALSE)
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
	
	ntreeCompositExecTree(ntree, &cj->scene->r, 0, 1);	/* 1 is do_previews */

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
	if ( ED_operator_node_active(C)) {
		SpaceNode *snode= CTX_wm_space_node(C);
		if (snode->treetype==NTREE_COMPOSIT)
			return 1;
	}
	return 0;
}

/* also checks for edited groups */
static bNode *editnode_get_active(bNodeTree *ntree)
{
	bNode *node;
	
	/* check for edited group */
	for (node= ntree->nodes.first; node; node= node->next)
		if (nodeGroupEditGet(node))
			break;
	if (node)
		return nodeGetActive((bNodeTree *)node->id);
	else
		return nodeGetActive(ntree);
}

static int has_nodetree(bNodeTree *ntree, bNodeTree *lookup)
{
	bNode *node;
	
	if (ntree == lookup)
		return 1;
	
	for (node=ntree->nodes.first; node; node=node->next)
		if (node->type == NODE_GROUP && node->id)
			if (has_nodetree((bNodeTree*)node->id, lookup))
				return 1;
	
	return 0;
}

static void snode_dag_update_group(void *calldata, ID *owner_id, bNodeTree *ntree)
{
	if (has_nodetree(ntree, calldata))
		DAG_id_tag_update(owner_id, 0);
}

void snode_dag_update(bContext *C, SpaceNode *snode)
{
	Main *bmain = CTX_data_main(C);

	/* for groups, update all ID's using this */
	if (snode->edittree!=snode->nodetree) {
		bNodeTreeType *tti= ntreeGetType(snode->edittree->type);
		tti->foreach_nodetree(bmain, snode->edittree, snode_dag_update_group);
	}

	DAG_id_tag_update(snode->id, 0);
}

void snode_notify(bContext *C, SpaceNode *snode)
{
	WM_event_add_notifier(C, NC_NODE|NA_EDITED, NULL);

	if (snode->treetype==NTREE_SHADER)
		WM_event_add_notifier(C, NC_MATERIAL|ND_NODES, snode->id);
	else if (snode->treetype==NTREE_COMPOSIT)
		WM_event_add_notifier(C, NC_SCENE|ND_NODES, snode->id);
	else if (snode->treetype==NTREE_TEXTURE)
		WM_event_add_notifier(C, NC_TEXTURE|ND_NODES, snode->id);
}

bNode *node_tree_get_editgroup(bNodeTree *nodetree)
{
	bNode *gnode;
	
	/* get the groupnode */
	for (gnode= nodetree->nodes.first; gnode; gnode= gnode->next)
		if (nodeGroupEditGet(gnode))
			break;
	return gnode;
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_shader_default(Scene *scene, ID *id)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock, *sock;
	bNodeTree *ntree;
	bNodeTemplate ntemp;
	int output_type, shader_type;
	float color[3], strength = 1.0f;
	
	ntree= ntreeAddTree("Shader Nodetree", NTREE_SHADER, 0);

	switch (GS(id->name)) {
		case ID_MA: {
			Material *ma= (Material*)id;
			ma->nodetree = ntree;

			if (BKE_scene_use_new_shading_nodes(scene)) {
				output_type = SH_NODE_OUTPUT_MATERIAL;
				shader_type = SH_NODE_BSDF_DIFFUSE;
			}
			else {
				output_type = SH_NODE_OUTPUT;
				shader_type = SH_NODE_MATERIAL;
			}

			copy_v3_v3(color, &ma->r);
			strength= 0.0f;
			break;
		}
		case ID_WO: {
			World *wo= (World*)id;
			wo->nodetree = ntree;

			output_type = SH_NODE_OUTPUT_WORLD;
			shader_type = SH_NODE_BACKGROUND;

			copy_v3_v3(color, &wo->horr);
			strength= 1.0f;
			break;
		}
		case ID_LA: {
			Lamp *la= (Lamp*)id;
			la->nodetree = ntree;

			output_type = SH_NODE_OUTPUT_LAMP;
			shader_type = SH_NODE_EMISSION;

			copy_v3_v3(color, &la->r);
			if (la->type == LA_LOCAL || la->type == LA_SPOT || la->type == LA_AREA)
				strength= 100.0f;
			else
				strength= 1.0f;
			break;
		}
		default:
			printf("ED_node_shader_default called on wrong ID type.\n");
			return;
	}
	
	ntemp.type = output_type;
	out= nodeAddNode(ntree, &ntemp);
	out->locx= 300.0f; out->locy= 300.0f;
	
	ntemp.type = shader_type;
	in= nodeAddNode(ntree, &ntemp);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(ntree, in);
	
	/* only a link from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(ntree, in, fromsock, out, tosock);

	/* default values */
	if (BKE_scene_use_new_shading_nodes(scene)) {
		sock= in->inputs.first;
		copy_v3_v3(((bNodeSocketValueRGBA*)sock->default_value)->value, color);

		if (strength != 0.0f) {
			sock= in->inputs.last;
			((bNodeSocketValueFloat*)sock->default_value)->value= strength;
		}
	}
	
	ntreeUpdateTree(ntree);
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_composit_default(Scene *sce)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	bNodeTemplate ntemp;
	
	/* but lets check it anyway */
	if (sce->nodetree) {
		if (G.debug & G_DEBUG)
			printf("error in composite initialize\n");
		return;
	}
	
	sce->nodetree= ntreeAddTree("Compositing Nodetree", NTREE_COMPOSIT, 0);

	sce->nodetree->chunksize = 256;
	sce->nodetree->edit_quality = NTREE_QUALITY_HIGH;
	sce->nodetree->render_quality = NTREE_QUALITY_HIGH;
	
	ntemp.type = CMP_NODE_COMPOSITE;
	out= nodeAddNode(sce->nodetree, &ntemp);
	out->locx= 300.0f; out->locy= 400.0f;
	out->id= &sce->id;
	id_us_plus(out->id);
	
	ntemp.type = CMP_NODE_R_LAYERS;
	in= nodeAddNode(sce->nodetree, &ntemp);
	in->locx= 10.0f; in->locy= 400.0f;
	in->id= &sce->id;
	id_us_plus(in->id);
	nodeSetActive(sce->nodetree, in);
	
	/* links from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(sce->nodetree, in, fromsock, out, tosock);
	
	ntreeUpdateTree(sce->nodetree);
	
	// XXX ntreeCompositForceHidden(sce->nodetree);
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void ED_node_texture_default(Tex *tx)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	bNodeTemplate ntemp;
	
	/* but lets check it anyway */
	if (tx->nodetree) {
		if (G.debug & G_DEBUG)
			printf("error in texture initialize\n");
		return;
	}
	
	tx->nodetree= ntreeAddTree("Texture Nodetree", NTREE_TEXTURE, 0);
	
	ntemp.type = TEX_NODE_OUTPUT;
	out= nodeAddNode(tx->nodetree, &ntemp);
	out->locx= 300.0f; out->locy= 300.0f;
	
	ntemp.type = TEX_NODE_CHECKER;
	in= nodeAddNode(tx->nodetree, &ntemp);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(tx->nodetree, in);
	
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(tx->nodetree, in, fromsock, out, tosock);
	
	ntreeUpdateTree(tx->nodetree);
}

/* id is supposed to contain a node tree */
void node_tree_from_ID(ID *id, bNodeTree **ntree, bNodeTree **edittree, int *treetype)
{
	if (id) {
		bNode *node= NULL;
		short idtype= GS(id->name);
	
		if (idtype == ID_NT) {
			*ntree= (bNodeTree*)id;
			if (treetype) *treetype= (*ntree)->type;
		}
		else if (idtype == ID_MA) {
			*ntree= ((Material*)id)->nodetree;
			if (treetype) *treetype= NTREE_SHADER;
		}
		else if (idtype == ID_LA) {
			*ntree= ((Lamp*)id)->nodetree;
			if (treetype) *treetype= NTREE_SHADER;
		}
		else if (idtype == ID_WO) {
			*ntree= ((World*)id)->nodetree;
			if (treetype) *treetype= NTREE_SHADER;
		}
		else if (idtype == ID_SCE) {
			*ntree= ((Scene*)id)->nodetree;
			if (treetype) *treetype= NTREE_COMPOSIT;
		}
		else if (idtype == ID_TE) {
			*ntree= ((Tex*)id)->nodetree;
			if (treetype) *treetype= NTREE_TEXTURE;
		}
		else {
			if (treetype) *treetype= 0;
			return;
		}
	
		/* find editable group */
		if (edittree) {
			if (*ntree)
				for (node= (*ntree)->nodes.first; node; node= node->next)
					if (nodeGroupEditGet(node))
						break;
			
			if (node && node->id)
				*edittree= (bNodeTree *)node->id;
			else
				*edittree= *ntree;
		}
	}
	else {
		*ntree= NULL;
		*edittree= NULL;
		if (treetype) *treetype= 0;
	}
}

/* Here we set the active tree(s), even called for each redraw now, so keep it fast :) */
void snode_set_context(SpaceNode *snode, Scene *scene)
{
	Object *ob= OBACT;
	
	snode->id= snode->from= NULL;
	
	if (snode->treetype==NTREE_SHADER) {
		/* need active object, or we allow pinning... */
		if (snode->shaderfrom == SNODE_SHADER_OBJECT) {
			if (ob) {
				if (ob->type == OB_LAMP) {
					snode->from= &ob->id;
					snode->id= ob->data;
				}
				else {
					Material *ma= give_current_material(ob, ob->actcol);
					if (ma) {
						snode->from= &ob->id;
						snode->id= &ma->id;
					}
				}
			}
		}
		else { /* SNODE_SHADER_WORLD */
			if (scene->world) {
				snode->from= NULL;
				snode->id= &scene->world->id;
			}
		}
	}
	else if (snode->treetype==NTREE_COMPOSIT) {
		snode->id= &scene->id;
		
		/* update output sockets based on available layers */
		ntreeCompositForceHidden(scene->nodetree, scene);
	}
	else if (snode->treetype==NTREE_TEXTURE) {
		Tex *tx= NULL;

		if (snode->texfrom==SNODE_TEX_OBJECT) {
			if (ob) {
				tx= give_current_object_texture(ob);

				if (ob->type == OB_LAMP)
					snode->from= (ID*)ob->data;
				else
					snode->from= (ID*)give_current_material(ob, ob->actcol);

				/* from is not set fully for material nodes, should be ID + Node then */
				snode->id= &tx->id;
			}
		}
		else if (snode->texfrom==SNODE_TEX_WORLD) {
			tx= give_current_world_texture(scene->world);
			snode->from= (ID *)scene->world;
			snode->id= &tx->id;
		}
		else {
			struct Brush *brush= NULL;
			
			if (ob && (ob->mode & OB_MODE_SCULPT))
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
	else {
		if (snode->nodetree && snode->nodetree->type == snode->treetype)
			snode->id = &snode->nodetree->id;
		else
			snode->id = NULL;
	}

	node_tree_from_ID(snode->id, &snode->nodetree, &snode->edittree, NULL);
}

static void snode_update(SpaceNode *snode, bNode *node)
{
	bNode *gnode;
	
	if (node)
		nodeUpdate(snode->edittree, node);
	
	/* if inside group, tag entire group */
	gnode= node_tree_get_editgroup(snode->nodetree);
	if (gnode)
		nodeUpdateID(snode->nodetree, gnode->id);
}

void ED_node_set_active(Main *bmain, bNodeTree *ntree, bNode *node)
{
	int was_active_texture = (node->flag & NODE_ACTIVE_TEXTURE);

	nodeSetActive(ntree, node);
	
	if (node->type!=NODE_GROUP) {
		int was_output= (node->flag & NODE_DO_OUTPUT);
		
		/* tree specific activate calls */
		if (ntree->type==NTREE_SHADER) {
			/* when we select a material, active texture is cleared, for buttons */
			if (node->id && ELEM3(GS(node->id->name), ID_MA, ID_LA, ID_WO))
				nodeClearActiveID(ntree, ID_TE);
			
			if (node->type==SH_NODE_OUTPUT) {
				bNode *tnode;
				
				for (tnode= ntree->nodes.first; tnode; tnode= tnode->next)
					if ( tnode->type==SH_NODE_OUTPUT)
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if (was_output==0)
					ED_node_generic_update(bmain, ntree, node);
			}

			/* if active texture changed, free glsl materials */
			if ((node->flag & NODE_ACTIVE_TEXTURE) && !was_active_texture) {
				Material *ma;

				for (ma=bmain->mat.first; ma; ma=ma->id.next)
					if (ma->nodetree && ma->use_nodes && has_nodetree(ma->nodetree, ntree))
						GPU_material_free(ma);

				WM_main_add_notifier(NC_IMAGE, NULL);
			}

			WM_main_add_notifier(NC_MATERIAL|ND_NODES, node->id);
		}
		else if (ntree->type==NTREE_COMPOSIT) {
			/* make active viewer, currently only 1 supported... */
			if ( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				bNode *tnode;
				

				for (tnode= ntree->nodes.first; tnode; tnode= tnode->next)
					if ( ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if (was_output==0)
					ED_node_generic_update(bmain, ntree, node);
				
				/* addnode() doesnt link this yet... */
				node->id= (ID *)BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
			}
			else if (node->type==CMP_NODE_R_LAYERS) {
				Scene *scene;

				for (scene=bmain->scene.first; scene; scene=scene->id.next) {
					if (scene->nodetree && scene->use_nodes && has_nodetree(scene->nodetree, ntree)) {
						if (node->id==NULL || node->id==(ID *)scene) {
							scene->r.actlay= node->custom1;
						}
					}
				}
			}
			else if (node->type==CMP_NODE_COMPOSITE) {
				if (was_output==0) {
					bNode *tnode;
					
					for (tnode= ntree->nodes.first; tnode; tnode= tnode->next)
						if ( tnode->type==CMP_NODE_COMPOSITE)
							tnode->flag &= ~NODE_DO_OUTPUT;
					
					node->flag |= NODE_DO_OUTPUT;
					ED_node_generic_update(bmain, ntree, node);
				}
			}
		}
		else if (ntree->type==NTREE_TEXTURE) {
			// XXX
#if 0
			if (node->id)
				; // XXX BIF_preview_changed(-1);
			// allqueue(REDRAWBUTSSHADING, 1);
			// allqueue(REDRAWIPO, 0);
#endif
		}
	}
}

static int inside_rctf(rctf *bounds, rctf *rect)
{
	return (bounds->xmin <= rect->xmin &&
	        bounds->xmax >= rect->xmax &&
	        bounds->ymin <= rect->ymin &&
	        bounds->ymax >= rect->ymax);
}

static void node_frame_attach_nodes(bNodeTree *UNUSED(ntree), bNode *frame)
{
	bNode *node;
	
	/* only check nodes on top of the frame for attaching */
	for (node=frame->next; node; node=node->next) {
		if (node->parent==frame) {
			/* detach nodes that went outside the frame */
			if (!inside_rctf(&frame->totr, &node->totr))
				nodeDetachNode(node);
		}
		else if (node->flag & NODE_SELECT && node->parent==NULL) {
			/* attach selected, still unparented nodes */
			if (inside_rctf(&frame->totr, &node->totr))
				nodeAttachNode(node, frame);
		}
	}
}

void ED_node_update_hierarchy(bContext *UNUSED(C), bNodeTree *ntree)
{
	bNode *node;
	
	/* XXX This does not work due to layout functions relying on node->block,
	 * which only exists during actual drawing. Can we rely on valid totr rects?
	 */
	/* make sure nodes have correct bounding boxes after transform */
//	node_update_nodetree(C, ntree, 0.0f, 0.0f);
	
	/* all selected nodes are re-parented */
	for (node=ntree->nodes.last; node; node=node->prev) {
		if (node->flag & NODE_SELECT && node->parent)
			nodeDetachNode(node);
	}
	
	/* update higher Z-level nodes first */
	for (node=ntree->nodes.last; node; node=node->prev) {
		/* XXX callback? */
		if (node->type==NODE_FRAME)
			node_frame_attach_nodes(ntree, node);
	}
}

/* ***************** generic operator functions for nodes ***************** */

#if 0 /* UNUSED */

static int edit_node_poll(bContext *C)
{
	return ED_operator_node_active(C);
}

static void edit_node_properties(wmOperatorType *ot)
{
	/* XXX could node be a context pointer? */
	RNA_def_string(ot->srna, "node", "", MAX_NAME, "Node", "");
	RNA_def_int(ot->srna, "socket", 0, 0, MAX_SOCKET, "Socket", "", 0, MAX_SOCKET);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Side", "");
}

static int edit_node_invoke_properties(bContext *C, wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "node")) {
		bNode *node= CTX_data_pointer_get_type(C, "node", &RNA_Node).data;
		if (!node)
			return 0;
		else
			RNA_string_set(op->ptr, "node", node->name);
	}
	
	if (!RNA_struct_property_is_set(op->ptr, "in_out"))
		RNA_enum_set(op->ptr, "in_out", SOCK_IN);
	
	if (!RNA_struct_property_is_set(op->ptr, "socket"))
		RNA_int_set(op->ptr, "socket", 0);
	
	return 1;
}

static void edit_node_properties_get(wmOperator *op, bNodeTree *ntree, bNode **rnode, bNodeSocket **rsock, int *rin_out)
{
	bNode *node;
	bNodeSocket *sock=NULL;
	char nodename[MAX_NAME];
	int sockindex;
	int in_out;
	
	RNA_string_get(op->ptr, "node", nodename);
	node = nodeFindNodebyName(ntree, nodename);
	
	in_out = RNA_enum_get(op->ptr, "in_out");
	
	sockindex = RNA_int_get(op->ptr, "socket");
	switch (in_out) {
	case SOCK_IN:	sock = BLI_findlink(&node->inputs, sockindex);	break;
	case SOCK_OUT:	sock = BLI_findlink(&node->outputs, sockindex);	break;
	}
	
	if (rnode)
		*rnode = node;
	if (rsock)
		*rsock = sock;
	if (rin_out)
		*rin_out = in_out;
}
#endif

/* ***************** Edit Group operator ************* */

void snode_make_group_editable(SpaceNode *snode, bNode *gnode)
{
	bNode *node;
	
	/* make sure nothing has group editing on */
	for (node=snode->nodetree->nodes.first; node; node=node->next)
		nodeGroupEditClear(node);
	
	if (gnode==NULL) {
		/* with NULL argument we do a toggle */
		if (snode->edittree==snode->nodetree)
			gnode= nodeGetActive(snode->nodetree);
	}
	
	if (gnode) {
		snode->edittree = nodeGroupEditSet(gnode, 1);
		
		/* deselect all other nodes, so we can also do grabbing of entire subtree */
		for (node= snode->nodetree->nodes.first; node; node= node->next)
			node_deselect(node);
		node_select(gnode);
	}
	else 
		snode->edittree= snode->nodetree;
}

static int node_group_edit_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	ED_preview_kill_jobs(C);

	if (snode->nodetree==snode->edittree) {
		bNode *gnode = nodeGetActive(snode->edittree);
		snode_make_group_editable(snode, gnode);
	}
	else
		snode_make_group_editable(snode, NULL);

	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);

	return OPERATOR_FINISHED;
}

static int node_group_edit_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;
	
	/* XXX callback? */
	if (snode->nodetree==snode->edittree) {
		gnode = nodeGetActive(snode->edittree);
		if (gnode && gnode->id && GS(gnode->id->name)==ID_NT && gnode->id->lib) {
			uiPupMenuOkee(C, op->type->idname, "Make group local?");
			return OPERATOR_CANCELLED;
		}
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
	char name[MAX_NAME]= "";
	int type= SOCK_FLOAT;
	bNodeTree *ngroup= snode->edittree;
	/* bNodeSocket *sock; */ /* UNUSED */
	
	ED_preview_kill_jobs(C);
	
	if (RNA_struct_property_is_set(op->ptr, "name"))
		RNA_string_get(op->ptr, "name", name);
	
	if (RNA_struct_property_is_set(op->ptr, "type"))
		type = RNA_enum_get(op->ptr, "type");
	
	if (RNA_struct_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;
	
	/* using placeholder subtype first */
	/* sock = */ /* UNUSED */ node_group_add_socket(ngroup, name, type, in_out);
	
	ntreeUpdateTree(ngroup);
	
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
	RNA_def_string(ot->srna, "name", "", MAX_NAME, "Name", "Group socket name");
	RNA_def_enum(ot->srna, "type", node_socket_type_items, SOCK_FLOAT, "Type", "Type of the group socket");
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
	
	if (RNA_struct_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;
	
	if (RNA_struct_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;
	
	sock = (bNodeSocket*)BLI_findlink(in_out==SOCK_IN ? &ngroup->inputs : &ngroup->outputs, index);
	if (sock) {
		node_group_remove_socket(ngroup, sock, in_out);
		ntreeUpdateTree(ngroup);
		
		snode_notify(C, snode);
	}
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Group Socket";
	ot->description = "Remove a node group socket";
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
	
	if (RNA_struct_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;
	
	if (RNA_struct_property_is_set(op->ptr, "in_out"))
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
		
		ngroup->update |= NTREE_UPDATE_GROUP_IN;
	}
	else if (in_out==SOCK_OUT) {
		sock = (bNodeSocket*)BLI_findlink(&ngroup->outputs, index);
		prev = sock->prev;
		/* can't move up the first socket */
		if (!prev)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->outputs, sock);
		BLI_insertlinkbefore(&ngroup->outputs, prev, sock);
		
		ngroup->update |= NTREE_UPDATE_GROUP_OUT;
	}
	ntreeUpdateTree(ngroup);
	
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
	
	if (RNA_struct_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;
	
	if (RNA_struct_property_is_set(op->ptr, "in_out"))
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
		
		ngroup->update |= NTREE_UPDATE_GROUP_IN;
	}
	else if (in_out==SOCK_OUT) {
		sock = (bNodeSocket*)BLI_findlink(&ngroup->outputs, index);
		next = sock->next;
		/* can't move down the last socket */
		if (!next)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->outputs, sock);
		BLI_insertlinkafter(&ngroup->outputs, next, sock);
		
		ngroup->update |= NTREE_UPDATE_GROUP_OUT;
	}
	ntreeUpdateTree(ngroup);
	
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
	if (gnode)
		snode_make_group_editable(snode, NULL);
	
	gnode= nodeGetActive(snode->edittree);
	if (gnode==NULL)
		return OPERATOR_CANCELLED;
	
	if (gnode->type!=NODE_GROUP) {
		BKE_report(op->reports, RPT_WARNING, "Not a group");
		return OPERATOR_CANCELLED;
	}
	else if (!node_group_ungroup(snode->edittree, gnode)) {
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

/* is rct in visible part of node? */
static bNode *visible_node(SpaceNode *snode, rctf *rct)
{
	bNode *node;
	
	for (node=snode->edittree->nodes.last; node; node=node->prev) {
		if (BLI_isect_rctf(&node->totr, rct, NULL))
			break;
	}
	return node;
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
	
	if (ibuf == NULL) {
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
	ot->name = "Background Image Move";
	ot->description = "Move Node backdrop";
	ot->idname = "NODE_OT_backimage_move";
	
	/* api callbacks */
	ot->invoke = snode_bg_viewmove_invoke;
	ot->modal = snode_bg_viewmove_modal;
	ot->poll = composite_node_active;
	ot->cancel = snode_bg_viewmove_cancel;
	
	/* flags */
	ot->flag = OPTYPE_BLOCKING|OPTYPE_GRAB_POINTER;
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
	ot->name = "Background Image Zoom";
	ot->idname = "NODE_OT_backimage_zoom";
	ot->description = "Zoom in/out the brackground image";
	
	/* api callbacks */
	ot->exec = backimage_zoom;
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = OPTYPE_BLOCKING;

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

	unsigned char col[4];
	float colf[4];

	int draw;
} ImageSampleInfo;

static void sample_draw(const bContext *C, ARegion *ar, void *arg_info)
{
	Scene *scene = CTX_data_scene(C);
	ImageSampleInfo *info= arg_info;

	ED_image_draw_info(ar, (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT), info->channels,
	                   info->x, info->y, info->col, info->colf,
	                   NULL, NULL /* zbuf - unused for nodes */
	                   );
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
	if (!ibuf)
		return;
	
	if (!ibuf->rect) {
		if (info->color_manage)
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

	if (fx>=0.0f && fy>=0.0f && fx<1.0f && fy<1.0f) {
		float *fp;
		char *cp;
		int x= (int)(fx*ibuf->x), y= (int)(fy*ibuf->y);

		CLAMP(x, 0, ibuf->x-1);
		CLAMP(y, 0, ibuf->y-1);

		info->x= x;
		info->y= y;
		info->draw= 1;
		info->channels= ibuf->channels;

		if (ibuf->rect) {
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
		if (ibuf->rect_float) {
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

	if (snode->treetype!=NTREE_COMPOSIT || !(snode->flag & SNODE_BACKDRAW))
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
	switch (event->type) {
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
	ot->name = "Backimage Sample";
	ot->idname = "NODE_OT_backimage_sample";
	ot->description = "Use mouse to sample background image";
	
	/* api callbacks */
	ot->invoke = sample_invoke;
	ot->modal = sample_modal;
	ot->cancel = sample_cancel;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}

/* ********************** size widget operator ******************** */

typedef struct NodeSizeWidget {
	float mxstart, mystart;
	float oldwidth, oldheight;
	float oldminiwidth;
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
				if (node->flag & NODE_HIDDEN) {
					node->miniwidth= nsw->oldminiwidth + mx - nsw->mxstart;
					CLAMP(node->miniwidth, 0.0f, 100.0f);
				}
				else {
					node->width= nsw->oldwidth + mx - nsw->mxstart;
					CLAMP(node->width, UI_DPI_FAC*node->typeinfo->minwidth, UI_DPI_FAC*node->typeinfo->maxwidth);
				}
				/* height works the other way round ... */
				node->height= nsw->oldheight - my + nsw->mystart;
				CLAMP(node->height, node->typeinfo->minheight, node->typeinfo->maxheight);
			}
				
			ED_region_tag_redraw(ar);

			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			
			MEM_freeN(nsw);
			op->customdata= NULL;
			
			ED_node_update_hierarchy(C, snode->edittree);
			
			return OPERATOR_FINISHED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

static int node_resize_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNode *node= editnode_get_active(snode->edittree);
	
	if (node) {
		/* convert mouse coordinates to v2d space */
		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
								 &snode->mx, &snode->my);
		
		if (node->typeinfo->resize_area_func(node, snode->mx, snode->my)) {
			NodeSizeWidget *nsw= MEM_callocN(sizeof(NodeSizeWidget), "size widget op data");
			
			op->customdata= nsw;
			nsw->mxstart= snode->mx;
			nsw->mystart= snode->my;
			
			/* store old */
			nsw->oldwidth= node->width;
			nsw->oldheight= node->height;
			nsw->oldminiwidth= node->miniwidth;
			
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
	ot->name = "Resize Node";
	ot->idname = "NODE_OT_resize";
	ot->description = "Resize a node";
	
	/* api callbacks */
	ot->invoke = node_resize_invoke;
	ot->modal = node_resize_modal;
	ot->poll = ED_operator_node_active;
	ot->cancel = node_resize_cancel;
	
	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}


/* ********************** hidden sockets ******************** */

int node_has_hidden_sockets(bNode *node)
{
	bNodeSocket *sock;
	
	for (sock= node->inputs.first; sock; sock= sock->next)
		if (sock->flag & SOCK_HIDDEN)
			return 1;
	for (sock= node->outputs.first; sock; sock= sock->next)
		if (sock->flag & SOCK_HIDDEN)
			return 1;
	return 0;
}

void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set)
{	
	bNodeSocket *sock;

	if (set==0) {
		for (sock= node->inputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_HIDDEN;
		for (sock= node->outputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_HIDDEN;
	}
	else {
		/* hide unused sockets */
		for (sock= node->inputs.first; sock; sock= sock->next) {
			if (sock->link==NULL)
				sock->flag |= SOCK_HIDDEN;
		}
		for (sock= node->outputs.first; sock; sock= sock->next) {
			if (nodeCountSocketLinks(snode->edittree, sock)==0)
				sock->flag |= SOCK_HIDDEN;
		}
	}
}

static int node_link_viewer(const bContext *C, bNode *tonode)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node;
	bNodeLink *link;
	bNodeSocket *sock;

	/* context check */
	if (tonode==NULL || tonode->outputs.first==NULL)
		return OPERATOR_CANCELLED;
	if ( ELEM(tonode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
		return OPERATOR_CANCELLED;
	
	/* get viewer */
	for (node= snode->edittree->nodes.first; node; node= node->next)
		if ( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
			if (node->flag & NODE_DO_OUTPUT)
				break;
	/* no viewer, we make one active */
	if (node==NULL) {
		for (node= snode->edittree->nodes.first; node; node= node->next) {
			if ( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				node->flag |= NODE_DO_OUTPUT;
				break;
			}
		}
	}
	
	sock = NULL;
	
	/* try to find an already connected socket to cycle to the next */
	if (node) {
		link = NULL;
		for (link= snode->edittree->links.first; link; link= link->next)
			if (link->tonode==node && link->fromnode==tonode)
				if (link->tosock==node->inputs.first)
					break;
		if (link) {
			/* unlink existing connection */
			sock= link->fromsock;
			nodeRemLink(snode->edittree, link);
			
			/* find a socket after the previously connected socket */
			for (sock=sock->next; sock; sock= sock->next)
				if (!nodeSocketIsHidden(sock))
					break;
		}
	}
	
	/* find a socket starting from the first socket */
	if (!sock) {
		for (sock= tonode->outputs.first; sock; sock= sock->next)
			if (!nodeSocketIsHidden(sock))
				break;
	}
	
	if (sock) {
		/* add a new viewer if none exists yet */
		if (!node) {
			Main *bmain = CTX_data_main(C);
			Scene *scene = CTX_data_scene(C);
			bNodeTemplate ntemp;
			
			ntemp.type = CMP_NODE_VIEWER;
			/* XXX location is a quick hack, just place it next to the linked socket */
			node = node_add_node(snode, bmain, scene, &ntemp, sock->locx + 100, sock->locy);
			if (!node)
				return OPERATOR_CANCELLED;
			
			link = NULL;
		}
		else {
			/* get link to viewer */
			for (link= snode->edittree->links.first; link; link= link->next)
				if (link->tonode==node && link->tosock==node->inputs.first)
					break;
		}
		
		if (link==NULL) {
			nodeAddLink(snode->edittree, tonode, sock, node, node->inputs.first);
		}
		else {
			link->fromnode= tonode;
			link->fromsock= sock;
		}
		ntreeUpdateTree(snode->edittree);
		snode_update(snode, node);
	}
	
	return OPERATOR_FINISHED;
}


static int node_active_link_viewer(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	
	node= editnode_get_active(snode->edittree);
	
	if (!node)
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	if (node_link_viewer(C, node) == OPERATOR_CANCELLED)
		return OPERATOR_CANCELLED;

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}



void NODE_OT_link_viewer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Link to Viewer Node";
	ot->description = "Link to viewer node";
	ot->idname = "NODE_OT_link_viewer";
	
	/* api callbacks */
	ot->exec = node_active_link_viewer;
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}



/* return 0, nothing done */
static int UNUSED_FUNCTION(node_mouse_groupheader)(SpaceNode *snode)
{
	bNode *gnode;
	float mx=0, my=0;
// XXX	int mval[2];
	
	gnode= node_tree_get_editgroup(snode->nodetree);
	if (gnode==NULL) return 0;
	
// XXX	getmouseco_areawin(mval);
// XXX	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* click in header or outside? */
	if (BLI_in_rctf(&gnode->totr, mx, my)==0) {
		rctf rect= gnode->totr;
		
		rect.ymax += NODE_DY;
		if (BLI_in_rctf(&rect, mx, my)==0)
			snode_make_group_editable(snode, NULL);	/* toggles, so exits editmode */
//		else
// XXX			transform_nodes(snode->nodetree, 'g', "Move group");
		
		return 1;
	}
	return 0;
}

/* checks snode->mouse position, and returns found node/socket */
/* type is SOCK_IN and/or SOCK_OUT */
int node_find_indicated_socket(SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, int in_out)
{
	bNode *node;
	bNodeSocket *sock;
	rctf rect;
	
	*nodep= NULL;
	*sockp= NULL;
	
	/* check if we click in a socket */
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		
		rect.xmin = snode->mx - (NODE_SOCKSIZE+4);
		rect.ymin = snode->my - (NODE_SOCKSIZE+4);
		rect.xmax = snode->mx + (NODE_SOCKSIZE+4);
		rect.ymax = snode->my + (NODE_SOCKSIZE+4);
		
		if (!(node->flag & NODE_HIDDEN)) {
			/* extra padding inside and out - allow dragging on the text areas too */
			if (in_out == SOCK_IN) {
				rect.xmax += NODE_SOCKSIZE;
				rect.xmin -= NODE_SOCKSIZE*4;
			}
			else if (in_out == SOCK_OUT) {
				rect.xmax += NODE_SOCKSIZE*4;
				rect.xmin -= NODE_SOCKSIZE;
			}
		}
		
		if (in_out & SOCK_IN) {
			for (sock= node->inputs.first; sock; sock= sock->next) {
				if (!nodeSocketIsHidden(sock)) {
					if (BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if (node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
		if (in_out & SOCK_OUT) {
			for (sock= node->outputs.first; sock; sock= sock->next) {
				if (!nodeSocketIsHidden(sock)) {
					if (BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if (node == visible_node(snode, &rect)) {
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
	if (in_out & SOCK_IN) {
		for (sock= snode->edittree->outputs.first; sock; sock= sock->next) {
			if (!nodeSocketIsHidden(sock)) {
				if (BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= NULL;	/* NULL node pointer indicates group socket */
					*sockp= sock;
					return 1;
				}
			}
		}
	}
	if (in_out & SOCK_OUT) {
		for (sock= snode->edittree->inputs.first; sock; sock= sock->next) {
			if (!nodeSocketIsHidden(sock)) {
				if (BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= NULL;	/* NULL node pointer indicates group socket */
					*sockp= sock;
					return 1;
				}
			}
		}
	}
	
	return 0;
}

static int outside_group_rect(SpaceNode *snode)
{
	bNode *gnode= node_tree_get_editgroup(snode->nodetree);
	if (gnode) {
		return (snode->mx <  gnode->totr.xmin ||
		        snode->mx >= gnode->totr.xmax ||
		        snode->my <  gnode->totr.ymin ||
		        snode->my >= gnode->totr.ymax);
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

static int socket_is_available(bNodeTree *UNUSED(ntree), bNodeSocket *sock, int allow_used)
{
	if (nodeSocketIsHidden(sock))
		return 0;
	
	if (!allow_used && (sock->flag & SOCK_IN_USE))
		return 0;
	
	return 1;
}

static bNodeSocket *best_socket_output(bNodeTree *ntree, bNode *node, bNodeSocket *sock_target, int allow_multiple)
{
	bNodeSocket *sock;
	
	/* first look for selected output */
	for (sock=node->outputs.first; sock; sock=sock->next) {
		if (!socket_is_available(ntree, sock, allow_multiple))
			continue;
		
		if (sock->flag & SELECT)
			return sock;
	}
	
	/* try to find a socket with a matching name */
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

/* this is a bit complicated, but designed to prioritize finding
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

static int snode_autoconnect_input(SpaceNode *snode, bNode *node_fr, bNodeSocket *sock_fr, bNode *node_to, bNodeSocket *sock_to, int replace)
{
	bNodeTree *ntree = snode->edittree;
	bNodeLink *link;
	
	/* then we can connect */
	if (replace)
		nodeRemSocketLinks(ntree, sock_to);
	
	link = nodeAddLink(ntree, node_fr, sock_fr, node_to, sock_to);
	/* validate the new link */
	ntreeUpdateTree(ntree);
	if (!(link->flag & NODE_LINK_VALID)) {
		nodeRemLink(ntree, link);
		return 0;
	}
	
	snode_update(snode, node_to);
	return 1;
}

void snode_autoconnect(SpaceNode *snode, int allow_multiple, int replace)
{
	bNodeTree *ntree = snode->edittree;
	ListBase *nodelist = MEM_callocN(sizeof(ListBase), "items_list");
	bNodeListItem *nli;
	bNode *node;
	int i, numlinks=0;
	
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->flag & NODE_SELECT) {
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
		int has_selected_inputs = 0;
		
		if (nli->next == NULL) break;
		
		node_fr = nli->node;
		node_to = nli->next->node;
		
		/* if there are selected sockets, connect those */
		for (sock_to = node_to->inputs.first; sock_to; sock_to=sock_to->next) {
			if (sock_to->flag & SELECT) {
				has_selected_inputs = 1;
				
				if (!socket_is_available(ntree, sock_to, replace))
					continue;
				
				/* check for an appropriate output socket to connect from */
				sock_fr = best_socket_output(ntree, node_fr, sock_to, allow_multiple);
				if (!sock_fr)
					continue;
	
				if (snode_autoconnect_input(snode, node_fr, sock_fr, node_to, sock_to, replace))
					++numlinks;
			}
		}
		
		if (!has_selected_inputs) {
			/* no selected inputs, connect by finding suitable match */
			int num_inputs = BLI_countlist(&node_to->inputs);
			
			for (i=0; i<num_inputs; i++) {
				
				/* find the best guess input socket */
				sock_to = best_socket_input(ntree, node_to, i, replace);
				if (!sock_to)
					continue;
				
				/* check for an appropriate output socket to connect from */
				sock_fr = best_socket_output(ntree, node_fr, sock_to, allow_multiple);
				if (!sock_fr)
					continue;
	
				if (snode_autoconnect_input(snode, node_fr, sock_fr, node_to, sock_to, replace)) {
					++numlinks;
					break;
				}
			}
		}
	}
	
	if (numlinks > 0) {
		ntreeUpdateTree(ntree);
	}
	
	BLI_freelistN(nodelist);
	MEM_freeN(nodelist);
}

/* can be called from menus too, but they should do own undopush and redraws */
bNode *node_add_node(SpaceNode *snode, Main *bmain, Scene *scene, bNodeTemplate *ntemp, float locx, float locy)
{
	bNode *node= NULL, *gnode;
	
	node_deselect_all(snode);
	
	node = nodeAddNode(snode->edittree, ntemp);
	
	/* generics */
	if (node) {
		node->locx= locx;
		node->locy= locy + 60.0f;		// arbitrary.. so its visible, (0,0) is top of node
		node_select(node);
		
		gnode= node_tree_get_editgroup(snode->nodetree);
		if (gnode) {
			node->locx -= gnode->locx;
			node->locy -= gnode->locy;
		}

		ntreeUpdateTree(snode->edittree);
		ED_node_set_active(bmain, snode->edittree, node);
		
		if (snode->nodetree->type==NTREE_COMPOSIT) {
			if (ELEM4(node->type, CMP_NODE_R_LAYERS, CMP_NODE_COMPOSITE, CMP_NODE_DEFOCUS, CMP_NODE_OUTPUT_FILE)) {
				node->id = &scene->id;
			}
			else if (ELEM3(node->type, CMP_NODE_MOVIECLIP, CMP_NODE_MOVIEDISTORTION, CMP_NODE_STABILIZE2D)) {
				node->id = (ID *)scene->clip;
			}
			
			ntreeCompositForceHidden(snode->edittree, scene);
		}
			
		if (node->id)
			id_us_plus(node->id);
			
		snode_update(snode, node);
	}
	
	if (snode->nodetree->type==NTREE_TEXTURE) {
		ntreeTexCheckCyclics(snode->edittree);
	}
	
	return node;
}

/* ****************** Duplicate *********************** */

static int node_duplicate_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree= snode->edittree;
	bNode *node, *newnode, *lastnode;
	bNodeLink *link, *newlink, *lastlink;
	int keep_inputs = RNA_boolean_get(op->ptr, "keep_inputs");
	
	ED_preview_kill_jobs(C);
	
	lastnode = ntree->nodes.last;
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			newnode = nodeCopyNode(ntree, node);
			
			if (newnode->id) {
				/* simple id user adjustment, node internal functions don't touch this
				 * but operators and readfile.c do. */
				id_us_plus(newnode->id);
				/* to ensure redraws or rerenders happen */
				ED_node_changed_update(snode->id, newnode);
			}
		}
		
		/* make sure we don't copy new nodes again! */
		if (node==lastnode)
			break;
	}
	
	/* copy links between selected nodes
	 * NB: this depends on correct node->new_node and sock->new_sock pointers from above copy!
	 */
	lastlink = ntree->links.last;
	for (link=ntree->links.first; link; link=link->next) {
		/* This creates new links between copied nodes.
		 * If keep_inputs is set, also copies input links from unselected (when fromnode==NULL)!
		 */
		if (link->tonode && (link->tonode->flag & NODE_SELECT) &&
		    (keep_inputs || (link->fromnode && (link->fromnode->flag & NODE_SELECT))))
		{
			newlink = MEM_callocN(sizeof(bNodeLink), "bNodeLink");
			newlink->flag = link->flag;
			newlink->tonode = link->tonode->new_node;
			newlink->tosock = link->tosock->new_sock;
			if (link->fromnode && (link->fromnode->flag & NODE_SELECT)) {
				newlink->fromnode = link->fromnode->new_node;
				newlink->fromsock = link->fromsock->new_sock;
			}
			else {
				/* input node not copied, this keeps the original input linked */
				newlink->fromnode = link->fromnode;
				newlink->fromsock = link->fromsock;
			}
			
			BLI_addtail(&ntree->links, newlink);
		}
		
		/* make sure we don't copy new links again! */
		if (link==lastlink)
			break;
	}
	
	/* deselect old nodes, select the copies instead */
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			/* has been set during copy above */
			newnode = node->new_node;
			
			node_deselect(node);
			node->flag &= ~NODE_ACTIVE;
			node_select(newnode);
		}
		
		/* make sure we don't copy new nodes again! */
		if (node==lastnode)
			break;
	}
	
	ntreeUpdateTree(snode->edittree);
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Nodes";
	ot->description = "Duplicate selected nodes";
	ot->idname = "NODE_OT_duplicate";
	
	/* api callbacks */
	ot->exec = node_duplicate_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "keep_inputs", 0, "Keep Inputs", "Keep the input links to duplicated nodes");
}

/* *************************** add link op ******************** */

static void node_remove_extra_links(SpaceNode *snode, bNodeSocket *tsock, bNodeLink *link)
{
	bNodeLink *tlink;
	bNodeSocket *sock;
	
	if (tsock && nodeCountSocketLinks(snode->edittree, link->tosock) > tsock->limit) {
		
		for (tlink= snode->edittree->links.first; tlink; tlink= tlink->next) {
			if (link!=tlink && tlink->tosock==link->tosock)
				break;
		}
		if (tlink) {
			/* try to move the existing link to the next available socket */
			if (tlink->tonode) {
				/* is there a free input socket with the target type? */
				for (sock= tlink->tonode->inputs.first; sock; sock= sock->next) {
					if (sock->type==tlink->tosock->type)
						if (nodeCountSocketLinks(snode->edittree, sock) < sock->limit)
							break;
				}
				if (sock) {
					tlink->tosock= sock;
					sock->flag &= ~SOCK_HIDDEN;
				}
				else {
					nodeRemLink(snode->edittree, tlink);
				}
			}
			else
				nodeRemLink(snode->edittree, tlink);
			
			snode->edittree->update |= NTREE_UPDATE_LINKS;
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
	bNode *tnode;
	bNodeSocket *tsock= NULL;
	bNodeLink *link;
	LinkData *linkdata;
	int in_out;

	in_out = nldrag->in_out;
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
							 &snode->mx, &snode->my);

	switch (event->type) {
		case MOUSEMOVE:
			
			if (in_out==SOCK_OUT) {
				if (node_find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					for (linkdata=nldrag->links.first; linkdata; linkdata=linkdata->next) {
						link = linkdata->data;
						
						/* skip if this is already the target socket */
						if (link->tosock == tsock)
							continue;
						/* skip if socket is on the same node as the fromsock */
						if (tnode && link->fromnode == tnode)
							continue;
						
						/* attach links to the socket */
						link->tonode = tnode;
						link->tosock = tsock;
						/* add it to the node tree temporarily */
						if (link->prev==NULL && link->next==NULL)
							BLI_addtail(&snode->edittree->links, link);
						
						snode->edittree->update |= NTREE_UPDATE_LINKS;
					}
					ntreeUpdateTree(snode->edittree);
				}
				else {
					int do_update = FALSE;
					for (linkdata=nldrag->links.first; linkdata; linkdata=linkdata->next) {
						link = linkdata->data;
						
						if (link->tonode || link->tosock) {
							BLI_remlink(&snode->edittree->links, link);
							link->prev = link->next = NULL;
							link->tonode= NULL;
							link->tosock= NULL;
							
							snode->edittree->update |= NTREE_UPDATE_LINKS;
							do_update = TRUE;
						}
					}
					if (do_update) {
						ntreeUpdateTree(snode->edittree);
					}
				}
			}
			else {
				if (node_find_indicated_socket(snode, &tnode, &tsock, SOCK_OUT)) {
					for (linkdata=nldrag->links.first; linkdata; linkdata=linkdata->next) {
						link = linkdata->data;
						
						/* skip if this is already the target socket */
						if (link->fromsock == tsock)
							continue;
						/* skip if socket is on the same node as the fromsock */
						if (tnode && link->tonode == tnode)
							continue;
						
						/* attach links to the socket */
						link->fromnode = tnode;
						link->fromsock = tsock;
						/* add it to the node tree temporarily */
						if (link->prev==NULL && link->next==NULL)
							BLI_addtail(&snode->edittree->links, link);
						
						snode->edittree->update |= NTREE_UPDATE_LINKS;
					}
					ntreeUpdateTree(snode->edittree);
				}
				else {
					int do_update = FALSE;
					for (linkdata=nldrag->links.first; linkdata; linkdata=linkdata->next) {
						link = linkdata->data;
						
						if (link->fromnode || link->fromsock) {
							BLI_remlink(&snode->edittree->links, link);
							link->prev = link->next = NULL;
							link->fromnode= NULL;
							link->fromsock= NULL;
							
							snode->edittree->update |= NTREE_UPDATE_LINKS;
							do_update = TRUE;
						}
					}
					if (do_update) {
						ntreeUpdateTree(snode->edittree);
					}
				}
			}
			
			ED_region_tag_redraw(ar);
			break;
			
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE: {
			for (linkdata=nldrag->links.first; linkdata; linkdata=linkdata->next) {
				link = linkdata->data;
				
				if (link->tosock && link->fromsock) {
					/* send changed events for original tonode and new */
					if (link->tonode)
						snode_update(snode, link->tonode);
					
					/* we might need to remove a link */
					if (in_out==SOCK_OUT)
						node_remove_extra_links(snode, link->tosock, link);
					
					/* when linking to group outputs, update the socket type */
					/* XXX this should all be part of a generic update system */
					if (!link->tonode) {
						if(link->tosock->type != link->fromsock->type)
							nodeSocketSetType(link->tosock, link->fromsock->type);
					}
				}
				else if (outside_group_rect(snode) && (link->tonode || link->fromnode)) {
					/* automatically add new group socket */
					if (link->tonode && link->tosock) {
						link->fromsock = node_group_expose_socket(snode->edittree, link->tosock, SOCK_IN);
						link->fromnode = NULL;
						if (link->prev==NULL && link->next==NULL)
							BLI_addtail(&snode->edittree->links, link);
						
						snode->edittree->update |= NTREE_UPDATE_GROUP_IN | NTREE_UPDATE_LINKS;
					}
					else if (link->fromnode && link->fromsock) {
						link->tosock = node_group_expose_socket(snode->edittree, link->fromsock, SOCK_OUT);
						link->tonode = NULL;
						if (link->prev==NULL && link->next==NULL)
							BLI_addtail(&snode->edittree->links, link);
						
						snode->edittree->update |= NTREE_UPDATE_GROUP_OUT | NTREE_UPDATE_LINKS;
					}
				}
				else
					nodeRemLink(snode->edittree, link);
			}
			
			ntreeUpdateTree(snode->edittree);
			snode_notify(C, snode);
			snode_dag_update(C, snode);
			
			BLI_remlink(&snode->linkdrag, nldrag);
			/* links->data pointers are either held by the tree or freed already */
			BLI_freelistN(&nldrag->links);
			MEM_freeN(nldrag);
			
			return OPERATOR_FINISHED;
		}
	}
	
	return OPERATOR_RUNNING_MODAL;
}

/* return 1 when socket clicked */
static bNodeLinkDrag *node_link_init(SpaceNode *snode, int detach)
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link, *link_next, *oplink;
	bNodeLinkDrag *nldrag = NULL;
	LinkData *linkdata;
	int num_links;
	
	/* output indicated? */
	if (node_find_indicated_socket(snode, &node, &sock, SOCK_OUT)) {
		nldrag= MEM_callocN(sizeof(bNodeLinkDrag), "drag link op customdata");
		
		num_links = nodeCountSocketLinks(snode->edittree, sock);
		if (num_links > 0 && (num_links >= sock->limit || detach)) {
			/* dragged links are fixed on input side */
			nldrag->in_out = SOCK_IN;
			/* detach current links and store them in the operator data */
			for (link= snode->edittree->links.first; link; link= link_next) {
				link_next = link->next;
				if (link->fromsock==sock) {
					linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
					linkdata->data = oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
					*oplink = *link;
					BLI_addtail(&nldrag->links, linkdata);
					nodeRemLink(snode->edittree, link);
				}
			}
		}
		else {
			/* dragged links are fixed on output side */
			nldrag->in_out = SOCK_OUT;
			/* create a new link */
			linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
			linkdata->data = oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
			oplink->fromnode = node;
			oplink->fromsock = sock;
			BLI_addtail(&nldrag->links, linkdata);
		}
	}
	/* or an input? */
	else if (node_find_indicated_socket(snode, &node, &sock, SOCK_IN)) {
		nldrag= MEM_callocN(sizeof(bNodeLinkDrag), "drag link op customdata");
		
		num_links = nodeCountSocketLinks(snode->edittree, sock);
		if (num_links > 0 && (num_links >= sock->limit || detach)) {
			/* dragged links are fixed on output side */
			nldrag->in_out = SOCK_OUT;
			/* detach current links and store them in the operator data */
			for (link= snode->edittree->links.first; link; link= link_next) {
				link_next = link->next;
				if (link->tosock==sock) {
					linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
					linkdata->data = oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
					*oplink = *link;
					BLI_addtail(&nldrag->links, linkdata);
					nodeRemLink(snode->edittree, link);
					
					/* send changed event to original link->tonode */
					if (node)
						snode_update(snode, node);
				}
			}
		}
		else {
			/* dragged links are fixed on input side */
			nldrag->in_out = SOCK_IN;
			/* create a new link */
			linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
			linkdata->data = oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
			oplink->tonode = node;
			oplink->tosock = sock;
			BLI_addtail(&nldrag->links, linkdata);
		}
	}
	
	return nldrag;
}

static int node_link_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNodeLinkDrag *nldrag;
	int detach = RNA_boolean_get(op->ptr, "detach");
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
							 &snode->mx, &snode->my);

	ED_preview_kill_jobs(C);

	nldrag = node_link_init(snode, detach);
	
	if (nldrag) {
		op->customdata= nldrag;
		BLI_addtail(&snode->linkdrag, nldrag);
		
		/* add modal handler */
		WM_event_add_modal_handler(C, op);
		
		return OPERATOR_RUNNING_MODAL;
	}
	else
		return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
}

static int node_link_cancel(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeLinkDrag *nldrag= op->customdata;
	
	BLI_remlink(&snode->linkdrag, nldrag);
	
	BLI_freelistN(&nldrag->links);
	MEM_freeN(nldrag);
	
	return OPERATOR_CANCELLED;
}

void NODE_OT_link(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Link Nodes";
	ot->idname = "NODE_OT_link";
	ot->description = "Use the mouse to create a link between two nodes";
	
	/* api callbacks */
	ot->invoke = node_link_invoke;
	ot->modal = node_link_modal;
//	ot->exec = node_link_exec;
	ot->poll = ED_operator_node_active;
	ot->cancel = node_link_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	RNA_def_boolean(ot->srna, "detach", FALSE, "Detach", "Detach and redirect existing links");
}

/* ********************** Make Link operator ***************** */

/* makes a link between selected output and input sockets */
static int node_make_link_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	int replace = RNA_boolean_get(op->ptr, "replace");

	ED_preview_kill_jobs(C);

	snode_autoconnect(snode, 1, replace);

	/* deselect sockets after linking */
	node_deselect_all_input_sockets(snode, 0);
	node_deselect_all_output_sockets(snode, 0);

	ntreeUpdateTree(snode->edittree);
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_link_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Links";
	ot->description = "Makes a link between selected output in input sockets";
	ot->idname = "NODE_OT_link_make";
	
	/* callbacks */
	ot->exec = node_make_link_exec;
	ot->poll = ED_operator_node_active; // XXX we need a special poll which checks that there are selected input/output sockets
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "replace", 0, "Replace", "Replace socket connections with the new links");
}

/* ********************** Cut Link operator ***************** */

#define LINK_RESOL 12
static int cut_links_intersect(bNodeLink *link, float mcoords[][2], int tot)
{
	float coord_array[LINK_RESOL+1][2];
	int i, b;
	
	if (node_link_bezier_points(NULL, NULL, link, coord_array, LINK_RESOL)) {

		for (i=0; i<tot-1; i++)
			for (b=0; b<LINK_RESOL; b++)
				if (isect_line_line_v2(mcoords[i], mcoords[i+1], coord_array[b], coord_array[b+1]) > 0)
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
	
	RNA_BEGIN (op->ptr, itemptr, "path")
	{
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		UI_view2d_region_to_view(&ar->v2d, (int)loc[0], (int)loc[1],
								 &mcoords[i][0], &mcoords[i][1]);
		i++;
		if (i>= 256) break;
	}
	RNA_END;
	
	if (i>1) {
		bNodeLink *link, *next;

		ED_preview_kill_jobs(C);
		
		for (link= snode->edittree->links.first; link; link= next) {
			next= link->next;
			
			if (cut_links_intersect(link, mcoords, i)) {
				snode_update(snode, link->tonode);
				nodeRemLink(snode->edittree, link);
			}
		}
		
		ntreeUpdateTree(snode->edittree);
		snode_notify(C, snode);
		snode_dag_update(C, snode);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
}

void NODE_OT_links_cut(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name = "Cut links";
	ot->idname = "NODE_OT_links_cut";
	ot->description = "Use the mouse to cut (remove) some links";
	
	ot->invoke = WM_gesture_lines_invoke;
	ot->modal = WM_gesture_lines_modal;
	ot->exec = cut_links_exec;
	ot->cancel = WM_gesture_lines_cancel;
	
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	prop= RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_KNIFECURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/* ********************** Detach links operator ***************** */

static int detach_links_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree= snode->edittree;
	bNode *node;
	
	ED_preview_kill_jobs(C);
	
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			nodeInternalRelink(ntree, node);
		}
	}
	
	ntreeUpdateTree(ntree);
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_links_detach(wmOperatorType *ot)
{
	ot->name = "Detach Links";
	ot->idname = "NODE_OT_links_detach";
	ot->description = "Remove all links to selected nodes, and try to connect neighbor nodes together";
	
	ot->exec = detach_links_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *********************  automatic node insert on dragging ******************* */

/* assumes sockets in list */
static bNodeSocket *socket_best_match(ListBase *sockets)
{
	bNodeSocket *sock;
	int type, maxtype=0;
	
	/* find type range */
	for (sock=sockets->first; sock; sock=sock->next)
		maxtype = MAX2(sock->type, maxtype);
	
	/* try all types, starting from 'highest' (i.e. colors, vectors, values) */
	for (type=maxtype; type >= 0; --type) {
		for (sock= sockets->first; sock; sock= sock->next) {
			if (!nodeSocketIsHidden(sock) && type==sock->type) {
				return sock;
			}
		}
	}
	
	/* no visible sockets, unhide first of highest type */
	for (type=maxtype; type >= 0; --type) {
		for (sock= sockets->first; sock; sock= sock->next) {
			if (type==sock->type) {
				sock->flag &= ~SOCK_HIDDEN;
				return sock;
			}
		}
	}
	
	return NULL;
}

/* prevent duplicate testing code below */
static SpaceNode *ed_node_link_conditions(ScrArea *sa, bNode **select)
{
	SpaceNode *snode= sa?sa->spacedata.first:NULL;
	bNode *node;
	bNodeLink *link;
	
	/* no unlucky accidents */
	if (sa==NULL || sa->spacetype!=SPACE_NODE) return NULL;
	
	*select= NULL;
	
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			if (*select)
				break;
			else
				*select= node;
		}
	}
	/* only one selected */
	if (node || *select==NULL) return NULL;
	
	/* correct node */
	if ((*select)->inputs.first==NULL || (*select)->outputs.first==NULL) return NULL;
	
	/* test node for links */
	for (link= snode->edittree->links.first; link; link=link->next) {
		if (link->tonode == *select || link->fromnode == *select)
			return NULL;
	}
	
	return snode;
}

/* assumes link with NODE_LINKFLAG_HILITE set */
void ED_node_link_insert(ScrArea *sa)
{
	bNode *node, *select;
	SpaceNode *snode= ed_node_link_conditions(sa, &select);
	bNodeLink *link;
	bNodeSocket *sockto;
	
	if (snode==NULL) return;
	
	/* get the link */
	for (link= snode->edittree->links.first; link; link=link->next)
		if (link->flag & NODE_LINKFLAG_HILITE)
			break;
	
	if (link) {
		node= link->tonode;
		sockto= link->tosock;
		
		link->tonode= select;
		link->tosock= socket_best_match(&select->inputs);
		link->flag &= ~NODE_LINKFLAG_HILITE;
		
		nodeAddLink(snode->edittree, select, socket_best_match(&select->outputs), node, sockto);
		ntreeUpdateTree(snode->edittree);	/* needed for pointers */
		snode_update(snode, select);
		ED_node_changed_update(snode->id, select);
	}
}


/* test == 0, clear all intersect flags */
void ED_node_link_intersect_test(ScrArea *sa, int test)
{
	bNode *select;
	SpaceNode *snode= ed_node_link_conditions(sa, &select);
	bNodeLink *link, *selink=NULL;
	float mcoords[6][2];
	
	if (snode==NULL) return;
	
	/* clear flags */
	for (link= snode->edittree->links.first; link; link=link->next)
		link->flag &= ~NODE_LINKFLAG_HILITE;
	
	if (test==0) return;
	
	/* okay, there's 1 node, without links, now intersect */
	mcoords[0][0]= select->totr.xmin;
	mcoords[0][1]= select->totr.ymin;
	mcoords[1][0]= select->totr.xmax;
	mcoords[1][1]= select->totr.ymin;
	mcoords[2][0]= select->totr.xmax;
	mcoords[2][1]= select->totr.ymax;
	mcoords[3][0]= select->totr.xmin;
	mcoords[3][1]= select->totr.ymax;
	mcoords[4][0]= select->totr.xmin;
	mcoords[4][1]= select->totr.ymin;
	mcoords[5][0]= select->totr.xmax;
	mcoords[5][1]= select->totr.ymax;
	
	/* we only tag a single link for intersect now */
	/* idea; use header dist when more? */
	for (link= snode->edittree->links.first; link; link=link->next) {
		
		if (cut_links_intersect(link, mcoords, 5)) { /* intersect code wants edges */
			if (selink)
				break;
			selink= link;
		}
	}
		
	if (link==NULL && selink)
		selink->flag |= NODE_LINKFLAG_HILITE;
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
	for (scene= bmain->scene.first; scene; scene= scene->id.next)
		scene->id.flag |= LIB_DOIT;

	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->type==CMP_NODE_R_LAYERS) {
			ID *id= node->id;
			if (id->flag & LIB_DOIT) {
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
	
	ot->name = "Read Render Layers";
	ot->idname = "NODE_OT_read_renderlayers";
	ot->description = "Read all render layers of all used scenes";
	
	ot->exec = node_read_renderlayers_exec;
	
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = 0;
}

static int node_read_fullsamplelayers_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain= CTX_data_main(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *curscene= CTX_data_scene(C);
	Render *re= RE_NewRender(curscene->id.name);

	WM_cursor_wait(1);
	RE_MergeFullSample(re, bmain, curscene, snode->nodetree);
	WM_cursor_wait(0);

	/* note we are careful to send the right notifier, as otherwise the
	 * compositor would reexecute and overwrite the full sample result */
	WM_event_add_notifier(C, NC_SCENE|ND_COMPO_RESULT, NULL);

	return OPERATOR_FINISHED;
}


void NODE_OT_read_fullsamplelayers(wmOperatorType *ot)
{
	
	ot->name = "Read Full Sample Layers";
	ot->idname = "NODE_OT_read_fullsamplelayers";
	ot->description = "Read all render layers of current scene, in full sample";
	
	ot->exec = node_read_fullsamplelayers_exec;
	
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = 0;
}

int node_render_changed_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *sce= CTX_data_scene(C);
	bNode *node;
	
	for (node= sce->nodetree->nodes.first; node; node= node->next) {
		if (node->id==(ID *)sce && node->need_exec) {
			break;
		}
	}
	if (node) {
		SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
		
		if (srl) {
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
	
	ot->name = "Render Changed Layer";
	ot->idname = "NODE_OT_render_changed";
	ot->description = "Render current scene, when input node's layer has been changed";
	
	ot->exec = node_render_changed_exec;
	
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = 0;
}


/* ****************** Make Group operator ******************* */

static int node_group_make_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;
	
	if (snode->edittree!=snode->nodetree) {
		BKE_report(op->reports, RPT_WARNING, "Can not add a new Group in a Group");
		return OPERATOR_CANCELLED;
	}
	
	/* for time being... is too complex to handle */
	if (snode->treetype==NTREE_COMPOSIT) {
		for (gnode=snode->nodetree->nodes.first; gnode; gnode= gnode->next) {
			if (gnode->flag & SELECT)
				if (gnode->type==CMP_NODE_R_LAYERS)
					break;
		}
		
		if (gnode) {
			BKE_report(op->reports, RPT_WARNING, "Can not add RenderLayer in a Group");
			return OPERATOR_CANCELLED;
		}
	}

	ED_preview_kill_jobs(C);
	
	gnode= node_group_make_from_selected(snode->nodetree);
	if (gnode==NULL) {
		BKE_report(op->reports, RPT_WARNING, "Can not make Group");
		return OPERATOR_CANCELLED;
	}
	else {
		nodeSetActive(snode->nodetree, gnode);
		ntreeUpdateTree(snode->nodetree);
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
	bNode *node;
	int tot_eq= 0, tot_neq= 0;

	/* Toggles the flag on all selected nodes.
	 * If the flag is set on all nodes it is unset.
	 * If the flag is not set on all nodes, it is set.
	 */
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			
			if (toggle_flag== NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW)==0)
				continue;
			if (toggle_flag== NODE_OPTIONS && (node->typeinfo->flag & NODE_OPTIONS)==0)
				continue;
			
			if (node->flag & toggle_flag)
				tot_eq++;
			else
				tot_neq++;
		}
	}
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			
			if (toggle_flag== NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW)==0)
				continue;
			if (toggle_flag== NODE_OPTIONS && (node->typeinfo->flag & NODE_OPTIONS)==0)
				continue;
			
			if ( (tot_eq && tot_neq) || tot_eq==0)
				node->flag |= toggle_flag;
			else
				node->flag &= ~toggle_flag;
		}
	}
}

static int node_hide_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;
	
	node_flag_toggle_exec(snode, NODE_HIDDEN);
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_hide_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide";
	ot->description = "Toggle hiding of selected nodes";
	ot->idname = "NODE_OT_hide_toggle";
	
	/* callbacks */
	ot->exec = node_hide_toggle_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int node_preview_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);

	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	node_flag_toggle_exec(snode, NODE_PREVIEW);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_preview_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Node Preview";
	ot->description = "Toggle preview display for selected nodes";
	ot->idname = "NODE_OT_preview_toggle";

	/* callbacks */
	ot->exec = node_preview_toggle_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int node_options_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);

	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	node_flag_toggle_exec(snode, NODE_OPTIONS);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_options_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Node Options";
	ot->description = "Toggle option buttons display for selected nodes";
	ot->idname = "NODE_OT_options_toggle";

	/* callbacks */
	ot->exec = node_options_toggle_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int node_socket_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	int hidden;

	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	/* Toggle for all selected nodes */
	hidden = 0;
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			if (node_has_hidden_sockets(node)) {
				hidden= 1;
				break;
			}
		}
	}
	
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & SELECT) {
			node_set_hidden_sockets(snode, node, !hidden);
		}
	}

	ntreeUpdateTree(snode->edittree);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_hide_socket_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Hidden Node Sockets";
	ot->description = "Toggle unused node socket display";
	ot->idname = "NODE_OT_hide_socket_toggle";

	/* callbacks */
	ot->exec = node_socket_toggle_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Mute operator *********************** */

static int node_mute_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;

	ED_preview_kill_jobs(C);

	for (node= snode->edittree->nodes.first; node; node= node->next) {
		/* Only allow muting of nodes having a mute func! */
		if ((node->flag & SELECT) && node->typeinfo->internal_connect) {
			node->flag ^= NODE_MUTED;
			snode_update(snode, node);
		}
	}
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_mute_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Node Mute";
	ot->description = "Toggle muting of the nodes";
	ot->idname = "NODE_OT_mute_toggle";
	
	/* callbacks */
	ot->exec = node_mute_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Delete operator ******************* */

static int node_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node, *next;
	
	ED_preview_kill_jobs(C);

	for (node= snode->edittree->nodes.first; node; node= next) {
		next= node->next;
		if (node->flag & SELECT) {
			/* check id user here, nodeFreeNode is called for free dbase too */
			if (node->id)
				node->id->us--;
			nodeFreeNode(snode->edittree, node);
		}
	}
	
	ntreeUpdateTree(snode->edittree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected nodes";
	ot->idname = "NODE_OT_delete";
	
	/* api callbacks */
	ot->exec = node_delete_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Delete with reconnect ******************* */
static int node_delete_reconnect_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node, *next;

	ED_preview_kill_jobs(C);

	for (node= snode->edittree->nodes.first; node; node= next) {
		next= node->next;
		if (node->flag & SELECT) {
			nodeInternalRelink(snode->edittree, node);
			
			/* check id user here, nodeFreeNode is called for free dbase too */
			if (node->id)
				node->id->us--;
			nodeFreeNode(snode->edittree, node);
		}
	}

	ntreeUpdateTree(snode->edittree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_delete_reconnect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete with reconnect";
	ot->description = "Delete nodes; will reconnect nodes as if deletion was muted";
	ot->idname = "NODE_OT_delete_reconnect";

	/* api callbacks */
	ot->exec = node_delete_reconnect_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Show Cyclic Dependencies Operator  ******************* */

static int node_show_cycles_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	/* this is just a wrapper around this call... */
	ntreeUpdateTree(snode->nodetree);
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_show_cyclic_dependencies(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show Cyclic Dependencies";
	ot->description = "Sort the nodes and show the cyclic dependencies between the nodes";
	ot->idname = "NODE_OT_show_cyclic_dependencies";
	
	/* callbacks */
	ot->exec = node_show_cycles_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Add File Node Operator  ******************* */

static int node_add_file_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	Image *ima= NULL;
	bNodeTemplate ntemp;

	/* check input variables */
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		char path[FILE_MAX];
		RNA_string_get(op->ptr, "filepath", path);

		errno= 0;

		ima= BKE_image_load_exists(path);

		if (!ima) {
			BKE_reportf(op->reports, RPT_ERROR, "Can't read: \"%s\", %s", path, errno ? strerror(errno) : "Unsupported image format");
			return OPERATOR_CANCELLED;
		}
	}
	else if (RNA_struct_property_is_set(op->ptr, "name")) {
		char name[MAX_ID_NAME-2];
		RNA_string_get(op->ptr, "name", name);
		ima= (Image *)BKE_libblock_find_name(ID_IM, name);

		if (!ima) {
			BKE_reportf(op->reports, RPT_ERROR, "Image named \"%s\", not found", name);
			return OPERATOR_CANCELLED;
		}
	}
	
	node_deselect_all(snode);
	
	switch (snode->nodetree->type) {
		case NTREE_SHADER:
			ntemp.type = SH_NODE_TEX_IMAGE;
			break;
		case NTREE_TEXTURE:
			ntemp.type = TEX_NODE_IMAGE;
			break;
		case NTREE_COMPOSIT:
			ntemp.type = CMP_NODE_IMAGE;
			break;
		default:
			return OPERATOR_CANCELLED;
	}
	
	ED_preview_kill_jobs(C);
	
	node = node_add_node(snode, bmain, scene, &ntemp, snode->mx, snode->my);
	
	if (!node) {
		BKE_report(op->reports, RPT_WARNING, "Could not add an image node");
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
	
	if (RNA_struct_property_is_set(op->ptr, "filepath") || RNA_struct_property_is_set(op->ptr, "name"))
		return node_add_file_exec(C, op);
	else
		return WM_operator_filesel(C, op, event);
}

void NODE_OT_add_file(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add File Node";
	ot->description = "Add a file node to the current node editor";
	ot->idname = "NODE_OT_add_file";
	
	/* callbacks */
	ot->exec = node_add_file_exec;
	ot->invoke = node_add_file_invoke;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);  //XXX TODO, relative_path
	RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME-2, "Name", "Datablock name to assign");
}

/********************** New node tree operator *********************/

static int new_node_tree_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode;
	bNodeTree *ntree;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	int treetype;
	char treename[MAX_ID_NAME-2] = "NodeTree";
	
	/* retrieve state */
	snode= CTX_wm_space_node(C);
	
	if (RNA_struct_property_is_set(op->ptr, "type"))
		treetype = RNA_enum_get(op->ptr, "type");
	else
		treetype = snode->treetype;
	
	if (RNA_struct_property_is_set(op->ptr, "name"))
		RNA_string_get(op->ptr, "name", treename);
	
	ntree = ntreeAddTree(treename, treetype, 0);
	if (!ntree)
		return OPERATOR_CANCELLED;
	
	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if (prop) {
		RNA_id_pointer_create(&ntree->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		/* RNA_property_pointer_set increases the user count,
		 * fixed here as the editor is the initial user.
		 */
		--ntree->id.us;
		RNA_property_update(C, &ptr, prop);
	}
	else if (snode) {
		Scene *scene= CTX_data_scene(C);
		snode->nodetree = ntree;
		
		ED_node_tree_update(snode, scene);
	}
	
	return OPERATOR_FINISHED;
}

void NODE_OT_new_node_tree(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Node Tree";
	ot->idname = "NODE_OT_new_node_tree";
	ot->description = "Create a new node tree";
	
	/* api callbacks */
	ot->exec = new_node_tree_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", nodetree_type_items, NTREE_COMPOSIT, "Tree Type", "");
	RNA_def_string(ot->srna, "name", "NodeTree", MAX_ID_NAME-2, "Name", "");
}

/* ****************** File Output Add Socket  ******************* */

static int node_output_file_add_socket_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	PointerRNA ptr;
	bNodeTree *ntree;
	bNode *node;
	char file_path[MAX_NAME];
	
	ptr = CTX_data_pointer_get(C, "node");
	if (!ptr.data)
		return OPERATOR_CANCELLED;
	node = ptr.data;
	ntree = ptr.id.data;
	
	RNA_string_get(op->ptr, "file_path", file_path);
	ntreeCompositOutputFileAddSocket(ntree, node, file_path, &scene->r.im_format);
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_output_file_add_socket(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add File Node Socket";
	ot->description = "Add a new input to a file output node";
	ot->idname = "NODE_OT_output_file_add_socket";
	
	/* callbacks */
	ot->exec = node_output_file_add_socket_exec;
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_string(ot->srna, "file_path", "Image", MAX_NAME, "File Path", "Sub-path of the output file");
}

/* ****************** Multi File Output Remove Socket  ******************* */

static int node_output_file_remove_active_socket_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	PointerRNA ptr = CTX_data_pointer_get(C, "node");
	bNodeTree *ntree;
	bNode *node;
	
	if (!ptr.data)
		return OPERATOR_CANCELLED;
	node = ptr.data;
	ntree = ptr.id.data;
	
	if (!ntreeCompositOutputFileRemoveActiveSocket(ntree, node))
		return OPERATOR_CANCELLED;
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_output_file_remove_active_socket(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove File Node Socket";
	ot->description = "Remove active input from a file output node";
	ot->idname = "NODE_OT_output_file_remove_active_socket";
	
	/* callbacks */
	ot->exec = node_output_file_remove_active_socket_exec;
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** Multi File Output Move Socket  ******************* */

static int node_output_file_move_active_socket_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	PointerRNA ptr = CTX_data_pointer_get(C, "node");
	bNode *node;
	NodeImageMultiFile *nimf;
	bNodeSocket *sock;
	int direction;
	
	if (!ptr.data)
		return OPERATOR_CANCELLED;
	node = ptr.data;
	nimf = node->storage;
	
	sock = BLI_findlink(&node->inputs, nimf->active_input);
	if (!sock)
		return OPERATOR_CANCELLED;
	
	direction = RNA_enum_get(op->ptr, "direction");
	
	if (direction==1) {
		bNodeSocket *before = sock->prev;
		if (!before)
			return OPERATOR_CANCELLED;
		BLI_remlink(&node->inputs, sock);
		BLI_insertlinkbefore(&node->inputs, before, sock);
		--nimf->active_input;
	}
	else {
		bNodeSocket *after = sock->next;
		if (!after)
			return OPERATOR_CANCELLED;
		BLI_remlink(&node->inputs, sock);
		BLI_insertlinkafter(&node->inputs, after, sock);
		++nimf->active_input;
	}
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_output_file_move_active_socket(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{1, "UP", 0, "Up", ""},
		{2, "DOWN", 0, "Down", ""},
		{ 0, NULL, 0, NULL, NULL }};
	
	/* identifiers */
	ot->name = "Move File Node Socket";
	ot->description = "Move the active input of a file output node up or down the list";
	ot->idname = "NODE_OT_output_file_move_active_socket";
	
	/* callbacks */
	ot->exec = node_output_file_move_active_socket_exec;
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "direction", direction_items, 2, "Direction", "");
}
