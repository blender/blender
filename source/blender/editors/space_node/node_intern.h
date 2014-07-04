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

#ifndef __NODE_INTERN_H__
#define __NODE_INTERN_H__

#include <stddef.h> /* for size_t */
#include "BKE_node.h"
#include "UI_interface.h"

/* internal exports only */

struct ARegion;
struct ARegionType;
struct View2D;
struct bContext;
struct wmWindow;
struct wmWindowManager;
struct wmEvent;
struct bNode;
struct bNodeSocket;
struct bNodeLink;
struct Main;
struct wmKeyConfig;

/* temp data to pass on to modal */
typedef struct bNodeLinkDrag {
	struct bNodeLinkDrag *next, *prev;
	
	/* List of links dragged by the operator.
	 * Note: This is a list of LinkData structs on top of the actual bNodeLinks.
	 * This way the links can be added to the node tree while being stored in this list.
	 */
	ListBase links;
	int in_out;
} bNodeLinkDrag;

/* space_node.c */
ARegion *node_has_buttons_region(ScrArea *sa);
ARegion *node_has_tools_region(ScrArea *sa);

void snode_group_offset(struct SpaceNode *snode, float *x, float *y);	/* transform between View2Ds in the tree path */

/* node_draw.c */
int node_get_colorid(struct bNode *node);
void node_socket_circle_draw(const struct bContext *C, struct bNodeTree *ntree, struct bNode *node,
                             struct bNodeSocket *sock, float size, int highlight);
int node_get_resize_cursor(int directions);
void node_draw_shadow(struct SpaceNode *snode, struct bNode *node, float radius, float alpha);
void node_draw_default(const struct bContext *C, struct ARegion *ar, struct SpaceNode *snode,
                       struct bNodeTree *ntree, struct bNode *node, bNodeInstanceKey key);
void node_update_default(const struct bContext *C, struct bNodeTree *ntree, struct bNode *node);
int node_select_area_default(struct bNode *node, int x, int y);
int node_tweak_area_default(struct bNode *node, int x, int y);
void node_update_nodetree(const struct bContext *C, struct bNodeTree *ntree);
void node_draw_nodetree(const struct bContext *C, struct ARegion *ar, struct SpaceNode *snode,
                        struct bNodeTree *ntree, bNodeInstanceKey parent_key);
void drawnodespace(const bContext *C, ARegion *ar);

void node_set_cursor(struct wmWindow *win, struct SpaceNode *snode, float cursor[2]);
	/* DPI scaled coords */
void node_to_view(struct bNode *node, float x, float y, float *rx, float *ry);
void node_from_view(struct bNode *node, float x, float y, float *rx, float *ry);

/* node_buttons.c */
void node_buttons_register(struct ARegionType *art);
void NODE_OT_properties(struct wmOperatorType *ot);

/* node_toolbar.c */
void node_toolbar_register(struct ARegionType *art);
void NODE_OT_toolbar(struct wmOperatorType *ot);

/* node_ops.c */
void node_operatortypes(void);
void node_keymap(struct wmKeyConfig *keyconf);

/* node_select.c */
void node_deselect_all(struct SpaceNode *snode);
void node_socket_select(struct bNode *node, struct bNodeSocket *sock);
void node_socket_deselect(struct bNode *node, struct bNodeSocket *sock, const bool deselect_node);
void node_deselect_all_input_sockets(struct SpaceNode *snode, const bool deselect_nodes);
void node_deselect_all_output_sockets(struct SpaceNode *snode, const bool deselect_nodes);
void node_select_single(struct bContext *C, struct bNode *node);

void NODE_OT_select(struct wmOperatorType *ot);
void NODE_OT_select_all(struct wmOperatorType *ot);
void NODE_OT_select_linked_to(struct wmOperatorType *ot);
void NODE_OT_select_linked_from(struct wmOperatorType *ot);
void NODE_OT_select_border(struct wmOperatorType *ot);
void NODE_OT_select_circle(struct wmOperatorType *ot);
void NODE_OT_select_lasso(struct wmOperatorType *ot);
void NODE_OT_select_grouped(struct wmOperatorType *ot);
void NODE_OT_select_same_type_step(struct wmOperatorType *ot);
void NODE_OT_find_node(struct wmOperatorType *ot);

/* node_view.c */
int space_node_view_flag(struct bContext *C, SpaceNode *snode, ARegion *ar,
                         const int node_flag, const int smooth_viewtx);

void NODE_OT_view_all(struct wmOperatorType *ot);
void NODE_OT_view_selected(struct wmOperatorType *ot);

void NODE_OT_backimage_move(struct wmOperatorType *ot);
void NODE_OT_backimage_zoom(struct wmOperatorType *ot);
void NODE_OT_backimage_fit(struct wmOperatorType *ot);
void NODE_OT_backimage_sample(struct wmOperatorType *ot);

/* drawnode.c */
void node_draw_link(struct View2D *v2d, struct SpaceNode *snode, struct bNodeLink *link);
void node_draw_link_bezier(struct View2D *v2d, struct SpaceNode *snode, struct bNodeLink *link, int th_col1, bool do_shaded, int th_col2, bool do_triple, int th_col3);
int  node_link_bezier_points(struct View2D *v2d, struct SpaceNode *snode, struct bNodeLink *link, float coord_array[][2], int resol);
// void node_draw_link_straight(View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int do_shaded, int th_col2, int do_triple, int th_col3 );
void draw_nodespace_back_pix(const struct bContext *C, struct ARegion *ar, struct SpaceNode *snode, bNodeInstanceKey parent_key);


/* node_add.c */
bNode *node_add_node(const struct bContext *C, const char *idname, int type, float locx, float locy);
void NODE_OT_add_reroute(struct wmOperatorType *ot);
void NODE_OT_add_file(struct wmOperatorType *ot);
void NODE_OT_add_mask(struct wmOperatorType *ot);
void NODE_OT_new_node_tree(struct wmOperatorType *ot);


/* node_group.c */
void NODE_OT_group_make(struct wmOperatorType *ot);
void NODE_OT_group_insert(struct wmOperatorType *ot);
void NODE_OT_group_ungroup(struct wmOperatorType *ot);
void NODE_OT_group_separate(struct wmOperatorType *ot);
void NODE_OT_group_edit(struct wmOperatorType *ot);


/* node_relationships.c */
void NODE_OT_link(struct wmOperatorType *ot);
void NODE_OT_link_make(struct wmOperatorType *ot);
void NODE_OT_links_cut(struct wmOperatorType *ot);
void NODE_OT_links_detach(struct wmOperatorType *ot);

void NODE_OT_parent_set(struct wmOperatorType *ot);
void NODE_OT_parent_clear(struct wmOperatorType *ot);
void NODE_OT_join(struct wmOperatorType *ot);
void NODE_OT_attach(struct wmOperatorType *ot);
void NODE_OT_detach(struct wmOperatorType *ot);

void NODE_OT_link_viewer(struct wmOperatorType *ot);

/* node_edit.c */
void snode_notify(struct bContext *C, struct SpaceNode *snode);
void snode_dag_update(struct bContext *C, struct SpaceNode *snode);
void snode_set_context(const struct bContext *C);

void snode_update(struct SpaceNode *snode, struct bNode *node);
int composite_node_active(struct bContext *C);
int composite_node_editable(struct bContext *C);

int node_has_hidden_sockets(bNode *node);
void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set);
int node_render_changed_exec(bContext *, struct wmOperator *);
int node_find_indicated_socket(struct SpaceNode *snode, struct bNode **nodep, struct bNodeSocket **sockp, float cursor[2], int in_out);

void NODE_OT_duplicate(struct wmOperatorType *ot);
void NODE_OT_delete(struct wmOperatorType *ot);
void NODE_OT_delete_reconnect(struct wmOperatorType *ot);
void NODE_OT_resize(struct wmOperatorType *ot);

void NODE_OT_mute_toggle(struct wmOperatorType *ot);
void NODE_OT_hide_toggle(struct wmOperatorType *ot);
void NODE_OT_hide_socket_toggle(struct wmOperatorType *ot);
void NODE_OT_preview_toggle(struct wmOperatorType *ot);
void NODE_OT_options_toggle(struct wmOperatorType *ot);
void NODE_OT_node_copy_color(struct wmOperatorType *ot);

void NODE_OT_read_fullsamplelayers(struct wmOperatorType *ot);
void NODE_OT_read_renderlayers(struct wmOperatorType *ot);
void NODE_OT_render_changed(struct wmOperatorType *ot);

void NODE_OT_output_file_add_socket(struct wmOperatorType *ot);
void NODE_OT_output_file_remove_active_socket(struct wmOperatorType *ot);
void NODE_OT_output_file_move_active_socket(struct wmOperatorType *ot);

/* Note: clipboard_cut is a simple macro of copy + delete */
void NODE_OT_clipboard_copy(struct wmOperatorType *ot);
void NODE_OT_clipboard_paste(struct wmOperatorType *ot);

void NODE_OT_tree_socket_add(struct wmOperatorType *ot);
void NODE_OT_tree_socket_remove(struct wmOperatorType *ot);
void NODE_OT_tree_socket_move(struct wmOperatorType *ot);

void NODE_OT_shader_script_update(struct wmOperatorType *ot);

void NODE_OT_viewer_border(struct wmOperatorType *ot);
void NODE_OT_clear_viewer_border(struct wmOperatorType *ot);

extern const char *node_context_dir[];

// XXXXXX

// nodes draw without dpi - the view zoom is flexible
#define HIDDEN_RAD      (0.75f * U.widget_unit)
#define BASIS_RAD       (0.4f * U.widget_unit)
#define NODE_DYS        (U.widget_unit / 2)
#define NODE_DY         U.widget_unit
#define NODE_SOCKDY     (0.08f * U.widget_unit)
#define NODE_WIDTH(node)	(node->width * UI_DPI_FAC)
#define NODE_MARGIN_X   (0.75f * U.widget_unit)
#define NODE_SOCKSIZE   (0.25f * U.widget_unit)
#define NODE_LINK_RESOL 12

// XXX button events (butspace)
enum eNodeSpace_ButEvents {
	B_NOP = 0,
	B_REDR = 1,
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
};

#endif /* __NODE_INTERN_H__ */
