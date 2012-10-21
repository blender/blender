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

#include "MEM_guardedalloc.h"

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_texture.h"

#include "RE_pipeline.h"


#include "ED_node.h"  /* own include */
#include "ED_screen.h"
#include "ED_render.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "GPU_material.h"

#include "node_intern.h"  /* own include */

#define USE_ESC_COMPO

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
	CompoJob *cj = cjv;
	
	/* without G.is_break 'ESC' wont quit - which annoys users */
	return (*(cj->stop)
#ifdef USE_ESC_COMPO
	        ||
	        G.is_break
#endif
	        );
}

/* called by compo, wmJob sends notifier */
static void compo_redrawjob(void *cjv, char *UNUSED(str))
{
	CompoJob *cj = cjv;
	
	*(cj->do_update) = TRUE;
}

static void compo_freejob(void *cjv)
{
	CompoJob *cj = cjv;

	if (cj->localtree) {
		ntreeLocalMerge(cj->localtree, cj->ntree);
	}
	MEM_freeN(cj);
}

/* only now we copy the nodetree, so adding many jobs while
 * sliding buttons doesn't frustrate */
static void compo_initjob(void *cjv)
{
	CompoJob *cj = cjv;

	cj->localtree = ntreeLocalize(cj->ntree);
}

/* called before redraw notifiers, it moves finished previews over */
static void compo_updatejob(void *cjv)
{
	CompoJob *cj = cjv;
	
	ntreeLocalSync(cj->localtree, cj->ntree);
}

static void compo_progressjob(void *cjv, float progress)
{
	CompoJob *cj = cjv;
	
	*(cj->progress) = progress;
}


/* only this runs inside thread */
static void compo_startjob(void *cjv, short *stop, short *do_update, float *progress)
{
	CompoJob *cj = cjv;
	bNodeTree *ntree = cj->localtree;
	Scene *scene = cj->scene;

	if (scene->use_nodes == FALSE)
		return;
	
	cj->stop = stop;
	cj->do_update = do_update;
	cj->progress = progress;

	ntree->test_break = compo_breakjob;
	ntree->tbh = cj;
	ntree->stats_draw = compo_redrawjob;
	ntree->sdh = cj;
	ntree->progress = compo_progressjob;
	ntree->prh = cj;
	
	// XXX BIF_store_spare();
	
	ntreeCompositExecTree(ntree, &cj->scene->r, 0, 1, &scene->view_settings, &scene->display_settings);  /* 1 is do_previews */

	ntree->test_break = NULL;
	ntree->stats_draw = NULL;
	ntree->progress = NULL;

}

/**
 * \param sa_owner is the owner of the job,
 * we don't use it for anything else currently so could also be a void pointer,
 * but for now keep it an 'Scene' for consistency.
 *
 * \note only call from spaces `refresh` callbacks, not direct! - use with care.
 */
void ED_node_composite_job(const bContext *C, struct bNodeTree *nodetree, Scene *scene_owner)
{
	wmJob *wm_job;
	CompoJob *cj;

	/* to fix bug: [#32272] */
	if (G.is_rendering) {
		return;
	}

#ifdef USE_ESC_COMPO
	G.is_break = FALSE;
#endif

	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene_owner, "Compositing",
	                     WM_JOB_EXCL_RENDER | WM_JOB_PROGRESS, WM_JOB_TYPE_COMPOSITE);
	cj = MEM_callocN(sizeof(CompoJob), "compo job");

	/* customdata for preview thread */
	cj->scene = CTX_data_scene(C);
	cj->ntree = nodetree;

	/* setup job */
	WM_jobs_customdata_set(wm_job, cj, compo_freejob);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE, NC_SCENE | ND_COMPO_RESULT);
	WM_jobs_callbacks(wm_job, compo_startjob, compo_initjob, compo_updatejob, NULL);

	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/* ***************************************** */

/* operator poll callback */
int composite_node_active(bContext *C)
{
	if (ED_operator_node_active(C)) {
		SpaceNode *snode = CTX_wm_space_node(C);
		if (snode->treetype == NTREE_COMPOSIT)
			return 1;
	}
	return 0;
}

/* also checks for edited groups */
bNode *editnode_get_active(bNodeTree *ntree)
{
	bNode *node;
	
	/* check for edited group */
	for (node = ntree->nodes.first; node; node = node->next)
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
	
	for (node = ntree->nodes.first; node; node = node->next)
		if (node->type == NODE_GROUP && node->id)
			if (has_nodetree((bNodeTree *)node->id, lookup))
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
	if (snode->edittree != snode->nodetree) {
		bNodeTreeType *tti = ntreeGetType(snode->edittree->type);
		tti->foreach_nodetree(bmain, snode->edittree, snode_dag_update_group);
	}

	DAG_id_tag_update(snode->id, 0);
}

void snode_notify(bContext *C, SpaceNode *snode)
{
	WM_event_add_notifier(C, NC_NODE | NA_EDITED, NULL);

	if (snode->treetype == NTREE_SHADER)
		WM_event_add_notifier(C, NC_MATERIAL | ND_NODES, snode->id);
	else if (snode->treetype == NTREE_COMPOSIT)
		WM_event_add_notifier(C, NC_SCENE | ND_NODES, snode->id);
	else if (snode->treetype == NTREE_TEXTURE)
		WM_event_add_notifier(C, NC_TEXTURE | ND_NODES, snode->id);
}

bNode *node_tree_get_editgroup(bNodeTree *nodetree)
{
	bNode *gnode;
	
	/* get the groupnode */
	for (gnode = nodetree->nodes.first; gnode; gnode = gnode->next)
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
	
	ntree = ntreeAddTree("Shader Nodetree", NTREE_SHADER, 0);

	switch (GS(id->name)) {
		case ID_MA:
		{
			Material *ma = (Material *)id;
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
			strength = 0.0f;
			break;
		}
		case ID_WO:
		{
			World *wo = (World *)id;
			wo->nodetree = ntree;

			output_type = SH_NODE_OUTPUT_WORLD;
			shader_type = SH_NODE_BACKGROUND;

			copy_v3_v3(color, &wo->horr);
			strength = 1.0f;
			break;
		}
		case ID_LA:
		{
			Lamp *la = (Lamp *)id;
			la->nodetree = ntree;

			output_type = SH_NODE_OUTPUT_LAMP;
			shader_type = SH_NODE_EMISSION;

			copy_v3_v3(color, &la->r);
			if (la->type == LA_LOCAL || la->type == LA_SPOT || la->type == LA_AREA)
				strength = 100.0f;
			else
				strength = 1.0f;
			break;
		}
		default:
			printf("ED_node_shader_default called on wrong ID type.\n");
			return;
	}
	
	ntemp.type = output_type;
	out = nodeAddNode(ntree, &ntemp);
	out->locx = 300.0f; out->locy = 300.0f;
	
	ntemp.type = shader_type;
	in = nodeAddNode(ntree, &ntemp);
	in->locx = 10.0f; in->locy = 300.0f;
	nodeSetActive(ntree, in);
	
	/* only a link from color to color */
	fromsock = in->outputs.first;
	tosock = out->inputs.first;
	nodeAddLink(ntree, in, fromsock, out, tosock);

	/* default values */
	if (BKE_scene_use_new_shading_nodes(scene)) {
		sock = in->inputs.first;
		copy_v3_v3(((bNodeSocketValueRGBA *)sock->default_value)->value, color);

		if (strength != 0.0f) {
			sock = in->inputs.last;
			((bNodeSocketValueFloat *)sock->default_value)->value = strength;
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
	
	sce->nodetree = ntreeAddTree("Compositing Nodetree", NTREE_COMPOSIT, 0);

	sce->nodetree->chunksize = 256;
	sce->nodetree->edit_quality = NTREE_QUALITY_HIGH;
	sce->nodetree->render_quality = NTREE_QUALITY_HIGH;
	
	ntemp.type = CMP_NODE_COMPOSITE;
	out = nodeAddNode(sce->nodetree, &ntemp);
	out->locx = 300.0f; out->locy = 400.0f;
	out->id = &sce->id;
	id_us_plus(out->id);
	
	ntemp.type = CMP_NODE_R_LAYERS;
	in = nodeAddNode(sce->nodetree, &ntemp);
	in->locx = 10.0f; in->locy = 400.0f;
	in->id = &sce->id;
	id_us_plus(in->id);
	nodeSetActive(sce->nodetree, in);
	
	/* links from color to color */
	fromsock = in->outputs.first;
	tosock = out->inputs.first;
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
	
	tx->nodetree = ntreeAddTree("Texture Nodetree", NTREE_TEXTURE, 0);
	
	ntemp.type = TEX_NODE_OUTPUT;
	out = nodeAddNode(tx->nodetree, &ntemp);
	out->locx = 300.0f; out->locy = 300.0f;
	
	ntemp.type = TEX_NODE_CHECKER;
	in = nodeAddNode(tx->nodetree, &ntemp);
	in->locx = 10.0f; in->locy = 300.0f;
	nodeSetActive(tx->nodetree, in);
	
	fromsock = in->outputs.first;
	tosock = out->inputs.first;
	nodeAddLink(tx->nodetree, in, fromsock, out, tosock);
	
	ntreeUpdateTree(tx->nodetree);
}

/* id is supposed to contain a node tree */
void node_tree_from_ID(ID *id, bNodeTree **ntree, bNodeTree **edittree, int *treetype)
{
	if (id) {
		bNode *node = NULL;
		short idtype = GS(id->name);
	
		if (idtype == ID_NT) {
			*ntree = (bNodeTree *)id;
			if (treetype) *treetype = (*ntree)->type;
		}
		else if (idtype == ID_MA) {
			*ntree = ((Material *)id)->nodetree;
			if (treetype) *treetype = NTREE_SHADER;
		}
		else if (idtype == ID_LA) {
			*ntree = ((Lamp *)id)->nodetree;
			if (treetype) *treetype = NTREE_SHADER;
		}
		else if (idtype == ID_WO) {
			*ntree = ((World *)id)->nodetree;
			if (treetype) *treetype = NTREE_SHADER;
		}
		else if (idtype == ID_SCE) {
			*ntree = ((Scene *)id)->nodetree;
			if (treetype) *treetype = NTREE_COMPOSIT;
		}
		else if (idtype == ID_TE) {
			*ntree = ((Tex *)id)->nodetree;
			if (treetype) *treetype = NTREE_TEXTURE;
		}
		else {
			if (treetype) *treetype = 0;
			return;
		}
	
		/* find editable group */
		if (edittree) {
			if (*ntree)
				for (node = (*ntree)->nodes.first; node; node = node->next)
					if (nodeGroupEditGet(node))
						break;
			
			if (node && node->id)
				*edittree = (bNodeTree *)node->id;
			else
				*edittree = *ntree;
		}
	}
	else {
		*ntree = NULL;
		*edittree = NULL;
		if (treetype) *treetype = 0;
	}
}

/* Here we set the active tree(s), even called for each redraw now, so keep it fast :) */
void snode_set_context(SpaceNode *snode, Scene *scene)
{
	Object *ob = OBACT;
	
	snode->id = snode->from = NULL;
	
	if (snode->treetype == NTREE_SHADER) {
		/* need active object, or we allow pinning... */
		if (snode->shaderfrom == SNODE_SHADER_OBJECT) {
			if (ob) {
				if (ob->type == OB_LAMP) {
					snode->from = &ob->id;
					snode->id = ob->data;
				}
				else {
					Material *ma = give_current_material(ob, ob->actcol);
					if (ma) {
						snode->from = &ob->id;
						snode->id = &ma->id;
					}
				}
			}
		}
		else { /* SNODE_SHADER_WORLD */
			if (scene->world) {
				snode->from = NULL;
				snode->id = &scene->world->id;
			}
		}
	}
	else if (snode->treetype == NTREE_COMPOSIT) {
		snode->id = &scene->id;
		
		/* update output sockets based on available layers */
		ntreeCompositForceHidden(scene->nodetree, scene);
	}
	else if (snode->treetype == NTREE_TEXTURE) {
		Tex *tx = NULL;

		if (snode->texfrom == SNODE_TEX_OBJECT) {
			if (ob) {
				tx = give_current_object_texture(ob);

				if (ob->type == OB_LAMP)
					snode->from = (ID *)ob->data;
				else
					snode->from = (ID *)give_current_material(ob, ob->actcol);

				/* from is not set fully for material nodes, should be ID + Node then */
				snode->id = &tx->id;
			}
		}
		else if (snode->texfrom == SNODE_TEX_WORLD) {
			tx = give_current_world_texture(scene->world);
			snode->from = (ID *)scene->world;
			snode->id = &tx->id;
		}
		else {
			struct Brush *brush = NULL;
			
			if (ob && (ob->mode & OB_MODE_SCULPT))
				brush = paint_brush(&scene->toolsettings->sculpt->paint);
			else
				brush = paint_brush(&scene->toolsettings->imapaint.paint);

			if (brush) {
				snode->from = (ID *)brush;
				tx = give_current_brush_texture(brush);
				snode->id = &tx->id;
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

void snode_update(SpaceNode *snode, bNode *node)
{
	bNode *gnode;
	
	if (node)
		nodeUpdate(snode->edittree, node);
	
	/* if inside group, tag entire group */
	gnode = node_tree_get_editgroup(snode->nodetree);
	if (gnode)
		nodeUpdateID(snode->nodetree, gnode->id);
}

void ED_node_set_active(Main *bmain, bNodeTree *ntree, bNode *node)
{
	int was_active_texture = (node->flag & NODE_ACTIVE_TEXTURE);

	nodeSetActive(ntree, node);
	
	if (node->type != NODE_GROUP) {
		int was_output = (node->flag & NODE_DO_OUTPUT);
		
		/* tree specific activate calls */
		if (ntree->type == NTREE_SHADER) {
			/* when we select a material, active texture is cleared, for buttons */
			if (node->id && ELEM3(GS(node->id->name), ID_MA, ID_LA, ID_WO))
				nodeClearActiveID(ntree, ID_TE);
			
			if (node->type == SH_NODE_OUTPUT) {
				bNode *tnode;
				
				for (tnode = ntree->nodes.first; tnode; tnode = tnode->next)
					if (tnode->type == SH_NODE_OUTPUT)
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if (was_output == 0)
					ED_node_generic_update(bmain, ntree, node);
			}

			/* if active texture changed, free glsl materials */
			if ((node->flag & NODE_ACTIVE_TEXTURE) && !was_active_texture) {
				Material *ma;

				for (ma = bmain->mat.first; ma; ma = ma->id.next)
					if (ma->nodetree && ma->use_nodes && has_nodetree(ma->nodetree, ntree))
						GPU_material_free(ma);

				WM_main_add_notifier(NC_IMAGE, NULL);
			}

			WM_main_add_notifier(NC_MATERIAL | ND_NODES, node->id);
		}
		else if (ntree->type == NTREE_COMPOSIT) {
			/* make active viewer, currently only 1 supported... */
			if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				bNode *tnode;
				

				for (tnode = ntree->nodes.first; tnode; tnode = tnode->next)
					if (ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if (was_output == 0)
					ED_node_generic_update(bmain, ntree, node);
				
				/* addnode() doesnt link this yet... */
				node->id = (ID *)BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
			}
			else if (node->type == CMP_NODE_R_LAYERS) {
				Scene *scene;

				for (scene = bmain->scene.first; scene; scene = scene->id.next) {
					if (scene->nodetree && scene->use_nodes && has_nodetree(scene->nodetree, ntree)) {
						if (node->id == NULL || node->id == (ID *)scene) {
							scene->r.actlay = node->custom1;
						}
					}
				}
			}
			else if (node->type == CMP_NODE_COMPOSITE) {
				if (was_output == 0) {
					bNode *tnode;
					
					for (tnode = ntree->nodes.first; tnode; tnode = tnode->next)
						if (tnode->type == CMP_NODE_COMPOSITE)
							tnode->flag &= ~NODE_DO_OUTPUT;
					
					node->flag |= NODE_DO_OUTPUT;
					ED_node_generic_update(bmain, ntree, node);
				}
			}
		}
		else if (ntree->type == NTREE_TEXTURE) {
			// XXX
#if 0
			if (node->id)
				;  // XXX BIF_preview_changed(-1);
			// allqueue(REDRAWBUTSSHADING, 1);
			// allqueue(REDRAWIPO, 0);
#endif
		}
	}
}

void ED_node_post_apply_transform(bContext *UNUSED(C), bNodeTree *UNUSED(ntree))
{
	/* XXX This does not work due to layout functions relying on node->block,
	 * which only exists during actual drawing. Can we rely on valid totr rects?
	 */
	/* make sure nodes have correct bounding boxes after transform */
	/* node_update_nodetree(C, ntree, 0.0f, 0.0f); */
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
		bNode *node = CTX_data_pointer_get_type(C, "node", &RNA_Node).data;
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
	bNodeSocket *sock = NULL;
	char nodename[MAX_NAME];
	int sockindex;
	int in_out;
	
	RNA_string_get(op->ptr, "node", nodename);
	node = nodeFindNodebyName(ntree, nodename);
	
	in_out = RNA_enum_get(op->ptr, "in_out");
	
	sockindex = RNA_int_get(op->ptr, "socket");
	switch (in_out) {
		case SOCK_IN:   sock = BLI_findlink(&node->inputs, sockindex);  break;
		case SOCK_OUT:  sock = BLI_findlink(&node->outputs, sockindex); break;
	}
	
	if (rnode)
		*rnode = node;
	if (rsock)
		*rsock = sock;
	if (rin_out)
		*rin_out = in_out;
}
#endif

/* ************************** Node generic ************** */

/* is rct in visible part of node? */
static bNode *visible_node(SpaceNode *snode, const rctf *rct)
{
	bNode *node;
	
	for (node = snode->edittree->nodes.last; node; node = node->prev) {
		if (BLI_rctf_isect(&node->totr, rct, NULL))
			break;
	}
	return node;
}

/* ********************** size widget operator ******************** */

typedef struct NodeSizeWidget {
	float mxstart, mystart;
	float oldlocx, oldlocy;
	float oldoffsetx, oldoffsety;
	float oldwidth, oldheight;
	float oldminiwidth;
	int directions;
} NodeSizeWidget;

static void node_resize_init(bContext *C, wmOperator *op, wmEvent *UNUSED(event), bNode *node, int dir)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	NodeSizeWidget *nsw = MEM_callocN(sizeof(NodeSizeWidget), "size widget op data");
	
	op->customdata = nsw;
	nsw->mxstart = snode->cursor[0];
	nsw->mystart = snode->cursor[1];
	
	/* store old */
	nsw->oldlocx = node->locx;
	nsw->oldlocy = node->locy;
	nsw->oldoffsetx = node->offsetx;
	nsw->oldoffsety = node->offsety;
	nsw->oldwidth = node->width;
	nsw->oldheight = node->height;
	nsw->oldminiwidth = node->miniwidth;
	nsw->directions = dir;
	
	WM_cursor_modal(CTX_wm_window(C), node_get_resize_cursor(dir));
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
}

static void node_resize_exit(bContext *C, wmOperator *op, int UNUSED(cancel))
{
	WM_cursor_restore(CTX_wm_window(C));
	
	MEM_freeN(op->customdata);
	op->customdata = NULL;
}

static int node_resize_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	bNode *node = editnode_get_active(snode->edittree);
	NodeSizeWidget *nsw = op->customdata;
	float mx, my, dx, dy;
	
	switch (event->type) {
		case MOUSEMOVE:
			
			UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &mx, &my);
			dx = mx - nsw->mxstart;
			dy = my - nsw->mystart;
			
			if (node) {
				if (node->flag & NODE_HIDDEN) {
					float widthmin = 0.0f;
					float widthmax = 100.0f;
					if (nsw->directions & NODE_RESIZE_RIGHT) {
						node->miniwidth = nsw->oldminiwidth + dx;
						CLAMP(node->miniwidth, widthmin, widthmax);
					}
					if (nsw->directions & NODE_RESIZE_LEFT) {
						float locmax = nsw->oldlocx + nsw->oldminiwidth;
						
						node->locx = nsw->oldlocx + dx;
						CLAMP(node->locx, locmax - widthmax, locmax - widthmin);
						node->miniwidth = locmax - node->locx;
					}
				}
				else {
					float widthmin = UI_DPI_FAC * node->typeinfo->minwidth;
					float widthmax = UI_DPI_FAC * node->typeinfo->maxwidth;
					if (nsw->directions & NODE_RESIZE_RIGHT) {
						node->width = nsw->oldwidth + dx;
						CLAMP(node->width, widthmin, widthmax);
					}
					if (nsw->directions & NODE_RESIZE_LEFT) {
						float locmax = nsw->oldlocx + nsw->oldwidth;
						
						node->locx = nsw->oldlocx + dx;
						CLAMP(node->locx, locmax - widthmax, locmax - widthmin);
						node->width = locmax - node->locx;
					}
				}
			
				/* height works the other way round ... */
				{
					float heightmin = UI_DPI_FAC * node->typeinfo->minheight;
					float heightmax = UI_DPI_FAC * node->typeinfo->maxheight;
					if (nsw->directions & NODE_RESIZE_TOP) {
						float locmin = nsw->oldlocy - nsw->oldheight;
						
						node->locy = nsw->oldlocy + dy;
						CLAMP(node->locy, locmin + heightmin, locmin + heightmax);
						node->height = node->locy - locmin;
					}
					if (nsw->directions & NODE_RESIZE_BOTTOM) {
						node->height = nsw->oldheight - dy;
						CLAMP(node->height, heightmin, heightmax);
					}
				}
				
				/* XXX make callback? */
				if (node->type == NODE_FRAME) {
					/* keep the offset symmetric around center point */
					if (nsw->directions & NODE_RESIZE_LEFT) {
						node->locx = nsw->oldlocx + 0.5f * dx;
						node->offsetx = nsw->oldoffsetx + 0.5f * dx;
					}
					if (nsw->directions & NODE_RESIZE_RIGHT) {
						node->locx = nsw->oldlocx + 0.5f * dx;
						node->offsetx = nsw->oldoffsetx - 0.5f * dx;
					}
					if (nsw->directions & NODE_RESIZE_TOP) {
						node->locy = nsw->oldlocy + 0.5f * dy;
						node->offsety = nsw->oldoffsety + 0.5f * dy;
					}
					if (nsw->directions & NODE_RESIZE_BOTTOM) {
						node->locy = nsw->oldlocy + 0.5f * dy;
						node->offsety = nsw->oldoffsety - 0.5f * dy;
					}
				}
			}
				
			ED_region_tag_redraw(ar);

			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			
			node_resize_exit(C, op, 0);
			ED_node_post_apply_transform(C, snode->edittree);
			
			return OPERATOR_FINISHED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

static int node_resize_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	bNode *node = editnode_get_active(snode->edittree);
	int dir;
	
	if (node) {
		/* convert mouse coordinates to v2d space */
		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1],
		                         &snode->cursor[0], &snode->cursor[1]);
		dir = node->typeinfo->resize_area_func(node, snode->cursor[0], snode->cursor[1]);
		if (dir != 0) {
			node_resize_init(C, op, event, node, dir);
			return OPERATOR_RUNNING_MODAL;
		}
	}
	return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

static int node_resize_cancel(bContext *C, wmOperator *op)
{
	node_resize_exit(C, op, 1);

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
	
	for (sock = node->inputs.first; sock; sock = sock->next)
		if (sock->flag & SOCK_HIDDEN)
			return 1;
	for (sock = node->outputs.first; sock; sock = sock->next)
		if (sock->flag & SOCK_HIDDEN)
			return 1;
	return 0;
}

void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set)
{
	bNodeSocket *sock;

	if (set == 0) {
		for (sock = node->inputs.first; sock; sock = sock->next)
			sock->flag &= ~SOCK_HIDDEN;
		for (sock = node->outputs.first; sock; sock = sock->next)
			sock->flag &= ~SOCK_HIDDEN;
	}
	else {
		/* hide unused sockets */
		for (sock = node->inputs.first; sock; sock = sock->next) {
			if (sock->link == NULL)
				sock->flag |= SOCK_HIDDEN;
		}
		for (sock = node->outputs.first; sock; sock = sock->next) {
			if (nodeCountSocketLinks(snode->edittree, sock) == 0)
				sock->flag |= SOCK_HIDDEN;
		}
	}
}

/* return 0, nothing done */
static int UNUSED_FUNCTION(node_mouse_groupheader) (SpaceNode * snode)
{
	bNode *gnode;
	float mx = 0, my = 0;
// XXX	int mval[2];
	
	gnode = node_tree_get_editgroup(snode->nodetree);
	if (gnode == NULL) return 0;
	
// XXX	getmouseco_areawin(mval);
// XXX	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* click in header or outside? */
	if (BLI_rctf_isect_pt(&gnode->totr, mx, my) == 0) {
		rctf rect = gnode->totr;
		
		rect.ymax += NODE_DY;
		if (BLI_rctf_isect_pt(&rect, mx, my) == 0)
			snode_make_group_editable(snode, NULL);  /* toggles, so exits editmode */
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
	
	*nodep = NULL;
	*sockp = NULL;
	
	/* check if we click in a socket */
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		
		rect.xmin = snode->cursor[0] - (NODE_SOCKSIZE + 4);
		rect.ymin = snode->cursor[1] - (NODE_SOCKSIZE + 4);
		rect.xmax = snode->cursor[0] + (NODE_SOCKSIZE + 4);
		rect.ymax = snode->cursor[1] + (NODE_SOCKSIZE + 4);
		
		if (!(node->flag & NODE_HIDDEN)) {
			/* extra padding inside and out - allow dragging on the text areas too */
			if (in_out == SOCK_IN) {
				rect.xmax += NODE_SOCKSIZE;
				rect.xmin -= NODE_SOCKSIZE * 4;
			}
			else if (in_out == SOCK_OUT) {
				rect.xmax += NODE_SOCKSIZE * 4;
				rect.xmin -= NODE_SOCKSIZE;
			}
		}
		
		if (in_out & SOCK_IN) {
			for (sock = node->inputs.first; sock; sock = sock->next) {
				if (!nodeSocketIsHidden(sock)) {
					if (BLI_rctf_isect_pt(&rect, sock->locx, sock->locy)) {
						if (node == visible_node(snode, &rect)) {
							*nodep = node;
							*sockp = sock;
							return 1;
						}
					}
				}
			}
		}
		if (in_out & SOCK_OUT) {
			for (sock = node->outputs.first; sock; sock = sock->next) {
				if (!nodeSocketIsHidden(sock)) {
					if (BLI_rctf_isect_pt(&rect, sock->locx, sock->locy)) {
						if (node == visible_node(snode, &rect)) {
							*nodep = node;
							*sockp = sock;
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
		for (sock = snode->edittree->outputs.first; sock; sock = sock->next) {
			if (!nodeSocketIsHidden(sock)) {
				if (BLI_rctf_isect_pt(&rect, sock->locx, sock->locy)) {
					*nodep = NULL;   /* NULL node pointer indicates group socket */
					*sockp = sock;
					return 1;
				}
			}
		}
	}
	if (in_out & SOCK_OUT) {
		for (sock = snode->edittree->inputs.first; sock; sock = sock->next) {
			if (!nodeSocketIsHidden(sock)) {
				if (BLI_rctf_isect_pt(&rect, sock->locx, sock->locy)) {
					*nodep = NULL;   /* NULL node pointer indicates group socket */
					*sockp = sock;
					return 1;
				}
			}
		}
	}
	
	return 0;
}

/* ****************** Duplicate *********************** */

static void node_duplicate_reparent_recursive(bNode *node)
{
	bNode *parent;
	
	node->flag |= NODE_TEST;
	
	/* find first selected parent */
	for (parent = node->parent; parent; parent = parent->parent) {
		if (parent->flag & SELECT) {
			if (!(parent->flag & NODE_TEST))
				node_duplicate_reparent_recursive(parent);
			break;
		}
	}
	/* reparent node copy to parent copy */
	if (parent) {
		nodeDetachNode(node->new_node);
		nodeAttachNode(node->new_node, parent->new_node);
	}
}

static int node_duplicate_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = snode->edittree;
	bNode *node, *newnode, *lastnode;
	bNodeLink *link, *newlink, *lastlink;
	int keep_inputs = RNA_boolean_get(op->ptr, "keep_inputs");
	
	ED_preview_kill_jobs(C);
	
	lastnode = ntree->nodes.last;
	for (node = ntree->nodes.first; node; node = node->next) {
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
		if (node == lastnode)
			break;
	}
	
	/* copy links between selected nodes
	 * NB: this depends on correct node->new_node and sock->new_sock pointers from above copy!
	 */
	lastlink = ntree->links.last;
	for (link = ntree->links.first; link; link = link->next) {
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
		if (link == lastlink)
			break;
	}
	
	/* clear flags for recursive depth-first iteration */
	for (node = ntree->nodes.first; node; node = node->next)
		node->flag &= ~NODE_TEST;
	/* reparent copied nodes */
	for (node = ntree->nodes.first; node; node = node->next) {
		if ((node->flag & SELECT) && !(node->flag & NODE_TEST))
			node_duplicate_reparent_recursive(node);
		
		/* only has to check old nodes */
		if (node == lastnode)
			break;
	}
	
	/* deselect old nodes, select the copies instead */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			/* has been set during copy above */
			newnode = node->new_node;
			
			node_deselect(node);
			node->flag &= ~NODE_ACTIVE;
			node_select(newnode);
		}
		
		/* make sure we don't copy new nodes again! */
		if (node == lastnode)
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "keep_inputs", 0, "Keep Inputs", "Keep the input links to duplicated nodes");
}

int ED_node_select_check(ListBase *lb)
{
	bNode *node;

	for (node = lb->first; node; node = node->next) {
		if (node->flag & NODE_SELECT) {
			return TRUE;
		}
	}

	return FALSE;
}

/* ******************************** */
// XXX some code needing updating to operators...


/* goes over all scenes, reads render layers */
static int node_read_renderlayers_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	SpaceNode *snode = CTX_wm_space_node(C);
	Scene *curscene = CTX_data_scene(C), *scene;
	bNode *node;

	ED_preview_kill_jobs(C);

	/* first tag scenes unread */
	for (scene = bmain->scene.first; scene; scene = scene->id.next)
		scene->id.flag |= LIB_DOIT;

	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->type == CMP_NODE_R_LAYERS) {
			ID *id = node->id;
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
	Main *bmain = CTX_data_main(C);
	SpaceNode *snode = CTX_wm_space_node(C);
	Scene *curscene = CTX_data_scene(C);
	Render *re = RE_NewRender(curscene->id.name);

	WM_cursor_wait(1);
	RE_MergeFullSample(re, bmain, curscene, snode->nodetree);
	WM_cursor_wait(0);

	/* note we are careful to send the right notifier, as otherwise the
	 * compositor would reexecute and overwrite the full sample result */
	WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, NULL);

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
	Scene *sce = CTX_data_scene(C);
	bNode *node;
	
	for (node = sce->nodetree->nodes.first; node; node = node->next) {
		if (node->id == (ID *)sce && node->need_exec) {
			break;
		}
	}
	if (node) {
		SceneRenderLayer *srl = BLI_findlink(&sce->r.layers, node->custom1);
		
		if (srl) {
			PointerRNA op_ptr;
			
			WM_operator_properties_create(&op_ptr, "RENDER_OT_render");
			RNA_string_set(&op_ptr, "layer", srl->name);
			RNA_string_set(&op_ptr, "scene", sce->id.name + 2);
			
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

/* ****************** Hide operator *********************** */

static void node_flag_toggle_exec(SpaceNode *snode, int toggle_flag)
{
	bNode *node;
	int tot_eq = 0, tot_neq = 0;

	/* Toggles the flag on all selected nodes.
	 * If the flag is set on all nodes it is unset.
	 * If the flag is not set on all nodes, it is set.
	 */
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			
			if (toggle_flag == NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW) == 0)
				continue;
			if (toggle_flag == NODE_OPTIONS && (node->typeinfo->flag & NODE_OPTIONS) == 0)
				continue;
			
			if (node->flag & toggle_flag)
				tot_eq++;
			else
				tot_neq++;
		}
	}
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			
			if (toggle_flag == NODE_PREVIEW && (node->typeinfo->flag & NODE_PREVIEW) == 0)
				continue;
			if (toggle_flag == NODE_OPTIONS && (node->typeinfo->flag & NODE_OPTIONS) == 0)
				continue;
			
			if ((tot_eq && tot_neq) || tot_eq == 0)
				node->flag |= toggle_flag;
			else
				node->flag &= ~toggle_flag;
		}
	}
}

static int node_hide_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;
	
	node_flag_toggle_exec(snode, NODE_HIDDEN);

	WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_preview_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_options_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	node_flag_toggle_exec(snode, NODE_OPTIONS);

	WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_socket_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node;
	int hidden;

	/* sanity checking (poll callback checks this already) */
	if ((snode == NULL) || (snode->edittree == NULL))
		return OPERATOR_CANCELLED;

	ED_preview_kill_jobs(C);

	/* Toggle for all selected nodes */
	hidden = 0;
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			if (node_has_hidden_sockets(node)) {
				hidden = 1;
				break;
			}
		}
	}
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			node_set_hidden_sockets(snode, node, !hidden);
		}
	}

	ntreeUpdateTree(snode->edittree);

	WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Mute operator *********************** */

static int node_mute_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node;

	ED_preview_kill_jobs(C);

	for (node = snode->edittree->nodes.first; node; node = node->next) {
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Delete operator ******************* */

static int node_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node, *next;
	
	ED_preview_kill_jobs(C);

	for (node = snode->edittree->nodes.first; node; node = next) {
		next = node->next;
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Delete with reconnect ******************* */
static int node_delete_reconnect_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node, *next;

	ED_preview_kill_jobs(C);

	for (node = snode->edittree->nodes.first; node; node = next) {
		next = node->next;
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ****************** File Output Add Socket  ******************* */

static int node_output_file_add_socket_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceNode *snode = CTX_wm_space_node(C);
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_string(ot->srna, "file_path", "Image", MAX_NAME, "File Path", "Sub-path of the output file");
}

/* ****************** Multi File Output Remove Socket  ******************* */

static int node_output_file_remove_active_socket_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Multi File Output Move Socket  ******************* */

static int node_output_file_move_active_socket_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
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
	
	if (direction == 1) {
		bNodeSocket *before = sock->prev;
		if (!before)
			return OPERATOR_CANCELLED;
		BLI_remlink(&node->inputs, sock);
		BLI_insertlinkbefore(&node->inputs, before, sock);
		nimf->active_input--;
	}
	else {
		bNodeSocket *after = sock->next;
		if (!after)
			return OPERATOR_CANCELLED;
		BLI_remlink(&node->inputs, sock);
		BLI_insertlinkafter(&node->inputs, after, sock);
		nimf->active_input++;
	}
	
	snode_notify(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_output_file_move_active_socket(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{1, "UP", 0, "Up", ""},
		{2, "DOWN", 0, "Down", ""},
		{ 0, NULL, 0, NULL, NULL }
	};
	
	/* identifiers */
	ot->name = "Move File Node Socket";
	ot->description = "Move the active input of a file output node up or down the list";
	ot->idname = "NODE_OT_output_file_move_active_socket";
	
	/* callbacks */
	ot->exec = node_output_file_move_active_socket_exec;
	ot->poll = composite_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "direction", direction_items, 2, "Direction", "");
}

/* ****************** Copy Node Color ******************* */

static int node_copy_color_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = snode->edittree;
	bNode *node, *tnode;
	
	if (!ntree)
		return OPERATOR_CANCELLED;
	node = nodeGetActive(ntree);
	if (!node)
		return OPERATOR_CANCELLED;
	
	for (tnode = ntree->nodes.first; tnode; tnode = tnode->next) {
		if (tnode->flag & NODE_SELECT && tnode != node) {
			if (node->flag & NODE_CUSTOM_COLOR) {
				tnode->flag |= NODE_CUSTOM_COLOR;
				copy_v3_v3(tnode->color, node->color);
			}
			else
				tnode->flag &= ~NODE_CUSTOM_COLOR;
		}
	}

	ED_node_sort(ntree);
	WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

	return OPERATOR_FINISHED;
}

void NODE_OT_node_copy_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Color";
	ot->description = "Copy color to all selected nodes";
	ot->idname = "NODE_OT_node_copy_color";

	/* api callbacks */
	ot->exec = node_copy_color_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Copy to clipboard ******************* */

static int node_clipboard_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = snode->edittree;
	bNode *gnode = node_tree_get_editgroup(snode->nodetree);
	float gnode_x = 0.0f, gnode_y = 0.0f;
	bNode *node;
	bNodeLink *link, *newlink;

	ED_preview_kill_jobs(C);

	/* clear current clipboard */
	BKE_node_clipboard_clear();
	BKE_node_clipboard_init(ntree);

	/* get group node offset */
	if (gnode)
		nodeToView(gnode, 0.0f, 0.0f, &gnode_x, &gnode_y);
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			bNode *new_node;
			new_node = nodeCopyNode(NULL, node);
			BKE_node_clipboard_add_node(new_node);
		}
	}

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			bNode *new_node = node->new_node;
			
			/* ensure valid pointers */
			if (new_node->parent) {
				/* parent pointer must be redirected to new node or detached if parent is not copied */
				if (new_node->parent->flag & NODE_SELECT) {
					new_node->parent = new_node->parent->new_node;
				}
				else {
					nodeDetachNode(new_node);
				}
			}

			/* transform to basic view space. child node location is relative to parent */
			if (!new_node->parent) {
				new_node->locx += gnode_x;
				new_node->locy += gnode_y;
			}
		}
	}

	/* copy links between selected nodes
	 * NB: this depends on correct node->new_node and sock->new_sock pointers from above copy!
	 */
	for (link = ntree->links.first; link; link = link->next) {
		/* This creates new links between copied nodes. */
		if (link->tonode && (link->tonode->flag & NODE_SELECT) &&
		    link->fromnode && (link->fromnode->flag & NODE_SELECT))
		{
			newlink = MEM_callocN(sizeof(bNodeLink), "bNodeLink");
			newlink->flag = link->flag;
			newlink->tonode = link->tonode->new_node;
			newlink->tosock = link->tosock->new_sock;
			newlink->fromnode = link->fromnode->new_node;
			newlink->fromsock = link->fromsock->new_sock;

			BKE_node_clipboard_add_link(newlink);
		}
	}

	return OPERATOR_FINISHED;
}

void NODE_OT_clipboard_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy to clipboard";
	ot->description = "Copies selected nodes to the clipboard";
	ot->idname = "NODE_OT_clipboard_copy";

	/* api callbacks */
	ot->exec = node_clipboard_copy_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Paste from clipboard ******************* */

static int node_clipboard_paste_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = snode->edittree;
	bNode *gnode = node_tree_get_editgroup(snode->nodetree);
	float gnode_center[2];
	const ListBase *clipboard_nodes_lb;
	const ListBase *clipboard_links_lb;
	bNode *node;
	bNodeLink *link;
	int num_nodes;
	float center[2];
	int is_clipboard_valid;

	/* validate pointers in the clipboard */
	is_clipboard_valid = BKE_node_clipboard_validate();
	clipboard_nodes_lb = BKE_node_clipboard_get_nodes();
	clipboard_links_lb = BKE_node_clipboard_get_links();

	if (clipboard_nodes_lb->first == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Clipboard is empty");
		return OPERATOR_CANCELLED;
	}

	if (BKE_node_clipboard_get_type() != ntree->type) {
		BKE_report(op->reports, RPT_ERROR, "Clipboard nodes are an incompatible type");
		return OPERATOR_CANCELLED;
	}

	/* only warn */
	if (is_clipboard_valid == FALSE) {
		BKE_report(op->reports, RPT_WARNING, "Some nodes references could not be restored, will be left empty");
	}

	ED_preview_kill_jobs(C);

	/* deselect old nodes */
	node_deselect_all(snode);

	/* get group node offset */
	if (gnode) {
		nodeToView(gnode, 0.0f, 0.0f, &gnode_center[0], &gnode_center[1]);
	}
	else {
		zero_v2(gnode_center);
	}

	/* calculate "barycenter" for placing on mouse cursor */
	zero_v2(center);
	for (node = clipboard_nodes_lb->first, num_nodes = 0; node; node = node->next, num_nodes++) {
		center[0] += BLI_rctf_cent_x(&node->totr);
		center[1] += BLI_rctf_cent_y(&node->totr);
	}
	mul_v2_fl(center, 1.0 / num_nodes);

	/* copy nodes from clipboard */
	for (node = clipboard_nodes_lb->first; node; node = node->next) {
		bNode *new_node = nodeCopyNode(ntree, node);

		/* needed since nodeCopyNode() doesn't increase ID's */
		id_us_plus(node->id);

		/* pasted nodes are selected */
		node_select(new_node);
	}
	
	/* reparent copied nodes */
	for (node = clipboard_nodes_lb->first; node; node = node->next) {
		bNode *new_node = node->new_node;
		if (new_node->parent)
			new_node->parent = new_node->parent->new_node;
		
		
		/* place nodes around the mouse cursor. child nodes locations are relative to parent */
		if (!new_node->parent) {
			new_node->locx += snode->cursor[0] - center[0] - gnode_center[0];
			new_node->locy += snode->cursor[1] - center[1] - gnode_center[1];
		}
	}

	for (link = clipboard_links_lb->first; link; link = link->next) {
		nodeAddLink(ntree, link->fromnode->new_node, link->fromsock->new_sock,
		            link->tonode->new_node, link->tosock->new_sock);
	}

	ntreeUpdateTree(snode->edittree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

static int node_clipboard_paste_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceNode *snode = CTX_wm_space_node(C);

	/* convert mouse coordinates to v2d space */
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &snode->cursor[0], &snode->cursor[1]);

	return node_clipboard_paste_exec(C, op);
}

void NODE_OT_clipboard_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste from clipboard";
	ot->description = "Pastes nodes from the clipboard to the active node tree";
	ot->idname = "NODE_OT_clipboard_paste";

	/* api callbacks */
	ot->exec = node_clipboard_paste_exec;
	ot->invoke = node_clipboard_paste_invoke;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
