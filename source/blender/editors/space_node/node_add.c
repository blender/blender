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

/** \file blender/editors/space_node/node_relationships.c
 *  \ingroup spnode
 */

#include <errno.h>

#include "DNA_node_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"

#include "ED_node.h"  /* own include */
#include "ED_screen.h"
#include "ED_render.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "node_intern.h"  /* own include */

/* can be called from menus too, but they should do own undopush and redraws */
bNode *node_add_node(SpaceNode *snode, Main *bmain, Scene *scene,
                     bNodeTemplate *ntemp, float locx, float locy)
{
	bNode *node = NULL, *gnode;

	node_deselect_all(snode);

	node = nodeAddNode(snode->edittree, ntemp);

	/* generics */
	if (node) {
		node_select(node);

		gnode = node_tree_get_editgroup(snode->nodetree);
		// arbitrary y offset of 60 so its visible
		if (gnode) {
			nodeFromView(gnode, locx, locy + 60.0f, &node->locx, &node->locy);
		}
		else {
			node->locx = locx;
			node->locy = locy + 60.0f;
		}

		ntreeUpdateTree(snode->edittree);
		ED_node_set_active(bmain, snode->edittree, node);

		if (snode->nodetree->type == NTREE_COMPOSIT) {
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


		if (snode->flag & SNODE_USE_HIDDEN_PREVIEW)
			node->flag &= ~NODE_PREVIEW;

		snode_update(snode, node);
	}

	if (snode->nodetree->type == NTREE_TEXTURE) {
		ntreeTexCheckCyclics(snode->edittree);
	}

	return node;
}

/* ********************** Add reroute operator ***************** */
static int add_reroute_intersect_check(bNodeLink *link, float mcoords[][2], int tot, float result[2])
{
	float coord_array[NODE_LINK_RESOL + 1][2];
	int i, b;

	if (node_link_bezier_points(NULL, NULL, link, coord_array, NODE_LINK_RESOL)) {

		for (i = 0; i < tot - 1; i++)
			for (b = 0; b < NODE_LINK_RESOL; b++)
				if (isect_line_line_v2(mcoords[i], mcoords[i + 1], coord_array[b], coord_array[b + 1]) > 0) {
					result[0] = (mcoords[i][0] + mcoords[i + 1][0]) / 2.0f;
					result[1] = (mcoords[i][1] + mcoords[i + 1][1]) / 2.0f;
					return 1;
				}
	}
	return 0;
}

static int add_reroute_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	bNode *gnode = node_tree_get_editgroup(snode->nodetree);
	float mcoords[256][2];
	int i = 0;

	RNA_BEGIN(op->ptr, itemptr, "path")
	{
		float loc[2];

		RNA_float_get_array(&itemptr, "loc", loc);
		UI_view2d_region_to_view(&ar->v2d, (short)loc[0], (short)loc[1],
		                         &mcoords[i][0], &mcoords[i][1]);
		i++;
		if (i >= 256) break;
	}
	RNA_END;

	if (i > 1) {
		bNodeLink *link;
		float insertPoint[2];

		for (link = snode->edittree->links.first; link; link = link->next) {
			if (add_reroute_intersect_check(link, mcoords, i, insertPoint)) {
				bNodeTemplate ntemp;
				bNode *rerouteNode;

				/* always first */
				ED_preview_kill_jobs(C);

				node_deselect_all(snode);

				ntemp.type = NODE_REROUTE;
				rerouteNode = nodeAddNode(snode->edittree, &ntemp);
				if (gnode) {
					nodeFromView(gnode, insertPoint[0], insertPoint[1], &rerouteNode->locx, &rerouteNode->locy);
				}
				else {
					rerouteNode->locx = insertPoint[0];
					rerouteNode->locy = insertPoint[1];
				}

				nodeAddLink(snode->edittree, link->fromnode, link->fromsock, rerouteNode, rerouteNode->inputs.first);
				link->fromnode = rerouteNode;
				link->fromsock = rerouteNode->outputs.first;

				/* always last */
				ntreeUpdateTree(snode->edittree);
				snode_notify(C, snode);
				snode_dag_update(C, snode);

				return OPERATOR_FINISHED; // add one reroute at the time.
			}
		}

		return OPERATOR_CANCELLED;

	}

	return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

void NODE_OT_add_reroute(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Add reroute";
	ot->idname = "NODE_OT_add_reroute";

	ot->invoke = WM_gesture_lines_invoke;
	ot->modal = WM_gesture_lines_modal;
	ot->exec = add_reroute_exec;
	ot->cancel = WM_gesture_lines_cancel;

	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_CROSSCURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}


/* ****************** Add File Node Operator  ******************* */

static int node_add_file_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node;
	Image *ima = NULL;
	bNodeTemplate ntemp;

	/* check input variables */
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		char path[FILE_MAX];
		RNA_string_get(op->ptr, "filepath", path);

		errno = 0;

		ima = BKE_image_load_exists(path);

		if (!ima) {
			BKE_reportf(op->reports, RPT_ERROR, "Can't read image: \"%s\", %s", path, errno ? strerror(errno) : "Unsupported format");
			return OPERATOR_CANCELLED;
		}
	}
	else if (RNA_struct_property_is_set(op->ptr, "name")) {
		char name[MAX_ID_NAME - 2];
		RNA_string_get(op->ptr, "name", name);
		ima = (Image *)BKE_libblock_find_name(ID_IM, name);

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
	id_us_plus(node->id);

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

static int node_add_file_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceNode *snode = CTX_wm_space_node(C);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_filesel(ot, FOLDERFILE | IMAGEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);  //XXX TODO, relative_path
	RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME - 2, "Name", "Datablock name to assign");
}


/********************** New node tree operator *********************/

static int new_node_tree_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode;
	bNodeTree *ntree;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	int treetype;
	char treename[MAX_ID_NAME - 2] = "NodeTree";

	/* retrieve state */
	snode = CTX_wm_space_node(C);

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
		Scene *scene = CTX_data_scene(C);
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", nodetree_type_items, NTREE_COMPOSIT, "Tree Type", "");
	RNA_def_string(ot->srna, "name", "NodeTree", MAX_ID_NAME - 2, "Name", "");
}
