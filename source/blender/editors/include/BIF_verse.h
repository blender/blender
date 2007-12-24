/**
 * $Id: BIF_verse.h 9683 2007-01-09 11:50:45Z jiri $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): Jiri Hnidek.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef WITH_VERSE

#ifndef BIF_VERSE_H
#define BIF_VERSE_H

#include "BKE_verse.h"
#include "DNA_meshdata_types.h"

struct Object;

struct EditMesh;
struct EditVert;
struct EditFace;
struct Mesh;
struct MVert;
struct Mface;

/* verse_object.c */
void unsubscribe_from_obj_node(struct VNode *vnode);
void unsubscribe_from_geom_node(struct VNode *vnode);
void unsubscribe_from_bitmap_node(struct VNode *vnode);

void test_and_send_idbutton_cb(void *obj, void *ob_name);

struct Object *create_object_from_verse_node(struct VNode *vnode);

void b_verse_pop_node(struct VNode *vnode);
void b_verse_unsubscribe(VNode *vnode);
void b_verse_push_object(struct VerseSession *session, struct Object *ob);
void b_verse_delete_object(struct Object *ob);
void b_verse_ms_get(void);

void post_transform_pos(struct VNode *vnode);
void post_transform_rot(struct VNode *vnode);
void post_transform_scale(struct VNode *vnode);

/*void post_transform(struct VNode *vnode);*/
void post_link_set(struct VLink *vlink);
void post_link_destroy(struct VLink *vlink);
void post_object_free_constraint(struct VNode *vnode);

void b_verse_send_transformation(struct Object *ob);

/* verse_mesh.c */
void b_verse_send_vertex_delete(struct EditVert *eve);
void send_versevert_pos(struct VerseVert *vvert);

void b_verse_send_face_delete(struct EditFace *efa);

void sync_all_versefaces_with_editfaces(struct VNode *vnode);
void sync_all_verseverts_with_editverts(struct VNode *vnode);

void createVerseVert(struct EditVert *ev);
void createVerseFace(struct EditFace *efa);

void b_verse_duplicate_object(struct VerseSession *session, struct Object *ob, struct Object *n_ob);
struct VNode *create_geom_vnode_from_geom_vnode(struct VNode *vnode);
struct VNode *create_geom_vnode_data_from_editmesh(struct VerseSession *session, struct EditMesh *em);
struct VNode *create_geom_vnode_data_from_mesh(struct VerseSession *session, struct Mesh *me);

void destroy_unused_geometry(struct VNode *vnode);
void destroy_binding_between_versemesh_and_editmesh(struct VNode *vnode);

void destroy_versemesh(struct VNode *vnode);

void unsubscribe_from_geom_node(struct VNode *vnode);

void create_edit_mesh_from_geom_node(struct VNode *vnode);
struct Mesh *create_mesh_from_geom_node(struct VNode *vnode);
void create_meshdata_from_geom_node(struct Mesh *me, struct VNode *vnode);

/* geometry post callback functions */
void post_layer_create(struct VLayer *vlayer);
void post_layer_destroy(struct VLayer *vlayer);

void post_vertex_create(struct VerseVert *vvert);
void post_vertex_set_xyz(struct VerseVert *vvert);
void post_vertex_delete(struct VerseVert *vvert);
void post_vertex_free_constraint(struct VerseVert *vvert);

void post_polygon_set_uint8(struct VerseFace *vface);
void post_polygon_create(struct VerseFace *vface);
void post_polygon_set_corner(struct VerseFace *vface);
void post_polygon_delete(struct VerseFace *vface);
void post_polygon_free_constraint(struct VerseFace *vface);

void post_geometry_free_constraint(struct VNode *vnode);

/* verse_common.c */
struct VerseSession *session_menu(void);
char *verse_client_name(void);

void post_tag_change(struct VTag *vtag);
void post_taggroup_create(struct VTagGroup *vtaggroup);

void post_node_create(struct VNode *vnode);
void post_node_destroy(struct VNode *vnode);
void post_node_name_set(struct VNode *vnode);

void post_connect_accept(struct VerseSession *session);
void post_connect_terminated(struct VerseSession *session);
void post_connect_update(struct VerseSession *session);
void post_server_add(void);

/* verse_image.c */

void sync_blender_image_with_verse_bitmap_node(struct VNode *vnode);
void post_bitmap_dimension_set(struct VNode *vnode);
void post_bitmap_layer_create(struct VBitmapLayer *vblayer);
void post_bitmap_layer_destroy(struct VBitmapLayer *vblayer);
void post_bitmap_tile_set(struct VBitmapLayer *vblayer, unsigned int xs, unsigned int ys);

#endif

#endif
