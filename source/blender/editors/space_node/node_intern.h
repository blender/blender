/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_NODE_INTERN_H
#define ED_NODE_INTERN_H

/* internal exports only */

struct ARegion;
struct ARegionType;
struct View2D;
struct bContext;
struct wmWindowManager;

/* defines */

#define NODE_SELECT_MOUSE		1

/* border select defines XXX these seem to be hardcode values still in border select invoke, check */
#define NODE_EXTEND				1
#define NODE_EXCLUSIVE			3

/* node_header.c */
void node_header_buttons(const bContext *C, ARegion *ar);
void node_menus_register(struct ARegionType *art);

/* node_draw.c */
void drawnodespace(const bContext *C, ARegion *ar, View2D *v2d);

/* node_ops.c */
void node_operatortypes(void);
void node_keymap(wmWindowManager *wm);

/* node_select.c */
void NODE_OT_select(struct wmOperatorType *ot);
void NODE_OT_select_extend(struct wmOperatorType *ot);
void NODE_OT_select_all(wmOperatorType *ot);
void NODE_OT_select_linked_to(wmOperatorType *ot);
void NODE_OT_select_linked_from(wmOperatorType *ot);
void NODE_OT_visibility_toggle(struct wmOperatorType *ot);
void NODE_OT_view_all(struct wmOperatorType *ot);
void NODE_OT_select_border(struct wmOperatorType *ot);

/* drawnode.c */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link);
void node_draw_link_bezier(View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int th_col2, int do_shaded);
int node_link_bezier_points(View2D *v2d, SpaceNode *snode, bNodeLink *link, float coord_array[][2], int resol);
void draw_nodespace_back_pix(ARegion *ar, SpaceNode *snode);

/* node_edit.c */
void snode_handle_recalc(bContext *C, SpaceNode *snode);
bNode *next_node(bNodeTree *ntree);
bNode *node_add_node(SpaceNode *snode, Scene *scene, int type, float locx, float locy);
void snode_set_context(SpaceNode *snode, Scene *scene);
void snode_make_group_editable(SpaceNode *snode, bNode *gnode);
void snode_home(ScrArea *sa, ARegion *ar, SpaceNode *snode);
void node_set_active(SpaceNode *snode, bNode *node);
void node_deselectall(SpaceNode *snode);
void snode_composite_job(const struct bContext *C, ScrArea *sa);
bNode *snode_get_editgroup(SpaceNode *snode);
void snode_autoconnect(SpaceNode *snode, bNode *node_to, int flag);

void NODE_OT_duplicate(struct wmOperatorType *ot);
void NODE_OT_link(struct wmOperatorType *ot);
void NODE_OT_delete(struct wmOperatorType *ot);
void NODE_OT_resize(struct wmOperatorType *ot);
void NODE_OT_links_cut(struct wmOperatorType *ot);

// XXXXXX

// XXX from BSE_node.h
#define HIDDEN_RAD		15.0f
#define BASIS_RAD		8.0f
#define NODE_DYS		10
#define NODE_DY			20
#define NODE_SOCKSIZE	5

// XXX button events (butspace)
enum {
	B_NOP = 0,
	B_REDR 	= 1,
	B_NODE_USEMAT,
	B_NODE_USESCENE,
	B_NODE_USETEX,
	B_TEXBROWSE,
	B_TEXALONE,
	B_TEXLOCAL,
	B_TEXDELETE,
	B_TEXPRV,
	B_AUTOTEXNAME,
	B_KEEPDATA,
	B_NODE_EXEC,
	B_MATPRV,
	B_NODE_LOADIMAGE,
	B_NODE_SETIMAGE,
} eActHeader_ButEvents;

#endif /* ED_NODE_INTERN_H */
