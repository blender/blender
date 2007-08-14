/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your opt ion) any later version. 
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL BLOCK *****
 */

#ifndef BSE_NODE_H
#define BSE_NODE_H

/* ********** drawing sizes *********** */
#define NODE_DY		20
#define NODE_DYS	10
#define NODE_SOCKSIZE	5
#define BASIS_RAD	8.0f
#define HIDDEN_RAD	15.0f


struct SpaceNode;
struct bNode;
struct bNodeTree;
struct Material;
struct ID;
struct Scene;
struct Image;
struct ImageUser;

/* ************* API for editnode.c *********** */

			/* helper calls to retreive active context for buttons, does groups */
struct Material *editnode_get_active_material(struct Material *ma);
struct bNode *editnode_get_active_idnode(struct bNodeTree *ntree, short id_code);
struct bNode *editnode_get_active(struct bNodeTree *ntree);

void snode_tag_dirty(struct SpaceNode *snode);

void snode_set_context(struct SpaceNode *snode);

void snode_home(struct ScrArea *sa, struct SpaceNode *snode);
void snode_zoom_in(struct ScrArea *sa);
void snode_zoom_out(struct ScrArea *sa);

void node_deselectall(struct SpaceNode *snode, int swap);
void node_border_select(struct SpaceNode *snode);

void node_delete(struct SpaceNode *snode);
void node_make_group(struct SpaceNode *snode);
void node_ungroup(struct SpaceNode *snode);
void snode_make_group_editable(struct SpaceNode *snode, struct bNode *gnode);
void node_hide(struct SpaceNode *snode);
void node_read_renderlayers(struct SpaceNode *snode);
void clear_scene_in_nodes(struct Scene *sce);

void node_transform_ext(int mode, int unused);
void node_shader_default(struct Material *ma);
void node_composit_default(struct Scene *scene);

int node_has_hidden_sockets(struct bNode *node);

struct bNode *node_add_node(struct SpaceNode *snode, int type, float locx, float locy);
void node_adduplicate(struct SpaceNode *snode);

void snode_autoconnect(struct SpaceNode *snode, struct bNode *node_to, int flag);
void node_select_linked(struct SpaceNode *snode, int out);

struct ImageUser *ntree_get_active_iuser(struct bNodeTree *ntree);

void imagepaint_composite_tags(struct bNodeTree *ntree, struct Image *image, struct ImageUser *iuser);



/* ************* drawnode.c *************** */
struct SpaceNode;
struct bNodeLink;
void node_draw_link(struct SpaceNode *snode, struct bNodeLink *link);

void init_node_butfuncs(void);

/* exported to CMP and SHD nodes */
//void node_ID_title_cb(void *node_v, void *unused_v);
//void node_but_title_cb(void *node_v, void *but_v);
//void node_texmap_cb(void *texmap_v, void *unused_v);
//void node_new_mat_cb(void *ntree_v, void *node_v);
//void node_browse_mat_cb(void *ntree_v, void *node_v);
//void node_mat_alone_cb(void *node_v, void *unused);


//void node_browse_image_cb(void *ntree_v, void *node_v);
//void node_active_cb(void *ntree_v, void *node_v);
//void node_image_type_cb(void *node_v, void *unused);
//char *node_image_type_pup(void);
//char *layer_menu(struct RenderResult *rr);
//void image_layer_cb(void *ima_v, void *iuser_v);
//void set_render_layers_title(void *node_v, void *unused);
//char *scene_layer_menu(struct Scene *sce);
//void node_browse_scene_cb(void *ntree_v, void *node_v);


//int node_buts_curvevec(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_curvecol(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_rgb(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_texture(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_valtorgb(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_value(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_mix_rgb(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_group(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_normal(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr);
//int node_buts_math(struct uiBlock *block, struct bNodeTree *ntree, struct bNode *node, struct rctf *butr) ;


/* ************* Shader nodes ***************** */


#endif

