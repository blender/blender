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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_intern.h
 *  \ingroup spnode
 */

#ifndef ED_NODE_INTERN_H
#define ED_NODE_INTERN_H

#include <stddef.h> /* for size_t */
#include "UI_interface.h"

/* internal exports only */

struct ARegion;
struct ARegionType;
struct View2D;
struct bContext;
struct wmWindowManager;
struct bNodeTemplate;
struct bNode;
struct bNodeSocket;
struct bNodeLink;
struct Main;

/* temp data to pass on to modal */
typedef struct bNodeLinkDrag
{
	struct bNodeLinkDrag *next, *prev;
	
	struct bNode *node;
	struct bNodeSocket *sock;
	struct bNodeLink *link;
	int in_out;
} bNodeLinkDrag;

/* space_node.c */
ARegion *node_has_buttons_region(ScrArea *sa);

/* node_header.c */
void node_header_buttons(const bContext *C, ARegion *ar);
void node_menus_register(void);

/* node_draw.c */
void node_socket_circle_draw(struct bNodeTree *ntree, struct bNodeSocket *sock, float size);
void node_draw_default(const struct bContext *C, struct ARegion *ar, struct SpaceNode *snode, struct bNodeTree *ntree, struct bNode *node);
void node_update_default(const struct bContext *C, struct bNodeTree *ntree, struct bNode *node);
void node_update_nodetree(const struct bContext *C, struct bNodeTree *ntree, float offsetx, float offsety);
void node_draw_nodetree(const struct bContext *C, struct ARegion *ar, struct SpaceNode *snode, struct bNodeTree *ntree);
void drawnodespace(const bContext *C, ARegion *ar, View2D *v2d);

/* node_buttons.c */
void node_buttons_register(struct ARegionType *art);
void NODE_OT_properties(struct wmOperatorType *ot);

/* node_ops.c */
void node_operatortypes(void);
void node_keymap(wmKeyConfig *keyconf);

/* node_select.c */
void node_deselect_all(struct SpaceNode *snode);
int node_select_same_type(struct SpaceNode *snode);
int node_select_same_type_np(struct SpaceNode *snode, int dir);
void node_select_single(struct bContext *C, struct bNode *node);

void NODE_OT_select(struct wmOperatorType *ot);
void NODE_OT_select_all(wmOperatorType *ot);
void NODE_OT_select_linked_to(wmOperatorType *ot);
void NODE_OT_select_linked_from(wmOperatorType *ot);
void NODE_OT_select_border(struct wmOperatorType *ot);
void NODE_OT_select_same_type(struct wmOperatorType *ot);
void NODE_OT_select_same_type_next(wmOperatorType *ot);
void NODE_OT_select_same_type_prev(wmOperatorType *ot);

/* node_state.c */
void NODE_OT_view_all(struct wmOperatorType *ot);

/* drawnode.c */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link);
void node_draw_link_bezier(View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int do_shaded, int th_col2, int do_triple, int th_col3 );
int node_link_bezier_points(View2D *v2d, SpaceNode *snode, bNodeLink *link, float coord_array[][2], int resol);
void node_draw_link_straight(View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int do_shaded, int th_col2, int do_triple, int th_col3 );
void draw_nodespace_back_pix(ARegion *ar, SpaceNode *snode, int color_manage);
void draw_nodespace_color_info(struct ARegion *ar, int color_manage, int channels, int x, int y, const char cp[4], const float fp[4]);

/* node_edit.c */
void node_tree_from_ID(ID *id, bNodeTree **ntree, bNodeTree **edittree, int *treetype);
void snode_notify(bContext *C, SpaceNode *snode);
void snode_dag_update(bContext *C, SpaceNode *snode);
bNode *node_add_node(struct SpaceNode *snode, struct Main *bmain, struct Scene *scene, struct bNodeTemplate *ntemp, float locx, float locy);
void snode_set_context(SpaceNode *snode, Scene *scene);
void snode_make_group_editable(SpaceNode *snode, bNode *gnode);
void snode_composite_job(const struct bContext *C, ScrArea *sa);
bNode *node_tree_get_editgroup(bNodeTree *ntree);
void node_tree_verify_groups(bNodeTree *nodetree);
void snode_autoconnect(SpaceNode *snode, int allow_multiple, int replace);
int node_has_hidden_sockets(bNode *node);
void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set);
int node_render_changed_exec(bContext *, wmOperator *);

void NODE_OT_duplicate(struct wmOperatorType *ot);
void NODE_OT_delete(struct wmOperatorType *ot);
void NODE_OT_delete_reconnect(struct wmOperatorType *ot);
void NODE_OT_resize(struct wmOperatorType *ot);

void NODE_OT_link(struct wmOperatorType *ot);
void NODE_OT_link_make(struct wmOperatorType *ot);
void NODE_OT_links_cut(struct wmOperatorType *ot);

void NODE_OT_group_make(struct wmOperatorType *ot);
void NODE_OT_group_ungroup(struct wmOperatorType *ot);
void NODE_OT_group_edit(struct wmOperatorType *ot);
void NODE_OT_group_socket_add(struct wmOperatorType *ot);
void NODE_OT_group_socket_remove(struct wmOperatorType *ot);
void NODE_OT_group_socket_move_up(struct wmOperatorType *ot);
void NODE_OT_group_socket_move_down(struct wmOperatorType *ot);

void NODE_OT_mute_toggle(struct wmOperatorType *ot);
void NODE_OT_hide_toggle(struct wmOperatorType *ot);
void NODE_OT_hide_socket_toggle(struct wmOperatorType *ot);
void NODE_OT_preview_toggle(struct wmOperatorType *ot);
void NODE_OT_options_toggle(struct wmOperatorType *ot);

void NODE_OT_show_cyclic_dependencies(struct wmOperatorType *ot);
void NODE_OT_link_viewer(struct wmOperatorType *ot);
void NODE_OT_read_fullsamplelayers(struct wmOperatorType *ot);
void NODE_OT_read_renderlayers(struct wmOperatorType *ot);
void NODE_OT_render_changed(struct wmOperatorType *ot);

void NODE_OT_backimage_move(struct wmOperatorType *ot);
void NODE_OT_backimage_zoom(struct wmOperatorType *ot);
void NODE_OT_backimage_sample(wmOperatorType *ot);

void NODE_OT_add_file(struct wmOperatorType *ot);

void NODE_OT_new_node_tree(struct wmOperatorType *ot);

extern const char *node_context_dir[];

// XXXXXX

// XXX from BSE_node.h
#define HIDDEN_RAD		15.0f
#define BASIS_RAD		8.0f
#define NODE_DYS		(U.widget_unit/2)
#define NODE_DY			U.widget_unit
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
} eNodeSpace_ButEvents;

#endif /* ED_NODE_INTERN_H */
