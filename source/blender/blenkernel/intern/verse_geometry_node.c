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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Jiri Hnidek.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef WITH_VERSE

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_dynamiclist.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_verse.h"
#include "BKE_utildefines.h"

//XXX #include "BIF_verse.h"

#include "verse.h"

/* function prototypes of static functions */

/* test functions for callback functions */
static char test_polygon_set_corner_uint32(uint32 v0, uint32 v1, uint32 v2, uint32 v3);

/* callback functions */
static void cb_g_layer_create(void *user_data, VNodeID node_id, VLayerID layer_id, const char *name, VNGLayerType type, uint32 def_integer, real64 def_real);
static void cb_g_layer_destroy(void *user_data, VNodeID node_id, VLayerID layer_id);
static void cb_g_vertex_set_xyz_real32(void *user_data, VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real32 x, 	real32 y, real32 z);
static void cb_g_polygon_set_corner_uint32(void *user_data, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint32 v0, uint32 v1, uint32 v2, uint32 v3);
static void cb_g_vertex_delete_real32(void *user_data, VNodeID node_id, uint32 vertex_id);
static void cb_g_polygon_delete(void *user_data, VNodeID node_id, uint32 polygon_id);
static void cb_g_crease_set_edge(void *user_data, VNodeID node_id, const char *layer, uint32 def_crease);
static void cb_g_crease_set_vertex(void *user_data, VNodeID node_id, const char *layer, uint32 def_crease);

/* other static functions */

static void free_unneeded_verseverts_of_verseface(struct VNode *vnode, struct VerseFace *vface);
static void free_verse_vertex(struct VLayer *vlayer, struct VerseVert *vvert);
static void free_verse_face(struct VLayer *vlayer, struct VerseFace *vface);
static void free_verse_layer_data(struct VNode *vnode, struct VLayer *vlayer);

static void send_verse_face(struct VerseFace *vface);

static VerseVert* find_verse_vert_in_queue(struct VLayer *vlayer, VNodeID node_id, uint32 vertex_id, real32 x, real32 y, real32 z);
static VerseFace* find_verse_face_in_queue(struct VLayer *vlayer, VNodeID node_id, uint32 polygon_id, uint32 v0, uint32 v1, uint32 v2, uint32 v3);

static unsigned short test_incoming_verseface(struct VGeomData *geom, struct VerseFace *vface);
static void find_unsent_faces(struct VNode *vnode, struct VerseVert *vvert);
static void find_vlayer_orphans(struct VNode *vnode, struct VerseVert *vvert);
static void move_face_orphan_to_dlist(struct VNode *vnode, struct VLayer *vlayer, struct VerseFace *vface);
static void increase_verse_verts_references(struct VerseFace *vface);
static void recalculate_verseface_normals(struct VNode *vnode);

/* verse edge functions */
static VerseEdge* find_verse_edge(struct VNode *vnode, uint32 v0, uint32 v1);
static void insert_verse_edgehash(struct VNode *vnode, struct VerseEdge *vedge);
static void remove_verse_edgehash(struct VNode *vnode, struct VerseEdge *vedge);
static void remove_verse_edge(struct VNode *vnode, uint32 v0, uint32 v1);
static void add_verse_edge(struct VNode *vnode, uint32 v0, uint32 v1);
static void update_edgehash_of_deleted_verseface(struct VNode *vnode, struct VerseFace *vface);
static void update_edgehash_of_changed_verseface(struct VNode *vnode, struct VerseFace *vface, uint32 v0, uint32 v1, uint32 v2, uint32 v3);
static void update_edgehash_of_new_verseface(struct VNode *vnode, uint32 v0, uint32 v1, uint32 v2, uint32 v3);

/*
 * recalcute normals of all VerseFaces
 */
static void recalculate_verseface_normals(VNode *vnode)
{
	struct VLayer *vert_layer, *face_layer;
	struct VerseFace *vface;
	struct VerseVert *vvert;

	if(vnode->type != V_NT_GEOMETRY) return;

	vert_layer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	face_layer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	vvert = vert_layer->dl.lb.first;
	while(vvert) {
		vvert->no[0] = vvert->no[1] = vvert->no[2] = 0.0;
		vvert = vvert->next;
	}

	vface = face_layer->dl.lb.first;
	while(vface) {
		/* calculate face normals */
		if(vface->vvert3) {
			CalcNormFloat4(vface->vvert0->co, vface->vvert1->co,
					vface->vvert2->co, vface->vvert3->co, vface->no);
			VecAddf(vface->vvert3->no, vface->vvert3->no, vface->no);
		}
		else
			CalcNormFloat(vface->vvert0->co, vface->vvert1->co,
					vface->vvert2->co, vface->no);

		/* calculate vertex normals ... it is averadge of all face normals using the vertex */
		VecAddf(vface->vvert0->no, vface->vvert0->no, vface->no);
		VecAddf(vface->vvert1->no, vface->vvert1->no, vface->no);
		VecAddf(vface->vvert2->no, vface->vvert2->no, vface->no);

		vface = vface->next;
	}

	/* we have to normalize all vertex normals */
	vvert = vert_layer->dl.lb.first;
	while(vvert) {
		Normalize(vvert->no);
		vvert = vvert->next;
	}
}

/*
 * add created item to the queue and send it if possible
 */
void add_item_to_send_queue(ListBase *lb, void *item, short type)
{
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct VerseVert *vvert;
	struct VerseFace *vface;

	/* this prevent from adding duplicated faces */
	if(type==VERSE_FACE) {
		struct Link *link = (Link*)lb->first;
		while(link) {
			if(link==item) {
				if(((VerseFace*)item)->flag & FACE_SENT) {
/*					printf("\tverse face %d marked as OBSOLETE\n", ((VerseFace*)item)->id);*/
					((VerseFace*)item)->flag |= FACE_OBSOLETE;
				}
				return;
			}
			link = link->next;
		}
	}

	/* add item to sending queue (two way dynamic list) */
	BLI_addtail(lb, item);

	/* send item, when it is possible */
	switch (type) {
		case VERSE_NODE:	/* only first node in queue can be sent */
			if(lb->first==lb->last)	
				send_verse_node((VNode*)item);
			break;
		case VERSE_LINK:	/* both object between have to exist */
			if(((VLink*)item)->flag & LINK_SEND_READY)
				send_verse_link((VLink*)item);
			break;
		case VERSE_LAYER:
			if(((VLayer*)item)->vnode->flag & NODE_RECEIVED)
				send_verse_layer((VLayer*)item);
			break;
		case VERSE_VERT:
			if(((VerseVert*)item)->vlayer->flag & LAYER_RECEIVED)
				send_verse_vertex((VerseVert*)item);
			break;
		case VERSE_FACE:	/* all vertexes of face have to be received */
			if(((VerseFace*)item)->flag & FACE_SEND_READY)
				send_verse_face((VerseFace*)item);
			break;
		case VERSE_TAG:
			send_verse_tag((VTag*)item);
			break;
		case VERSE_TAG_GROUP:
			send_verse_taggroup((VTagGroup*)item);
			break;
		case VERSE_VERT_UINT32:	/* parent item has to exist */
			vnode = (((uint32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 0 );
			vvert = (VerseVert*)BLI_dlist_find_link(&(vlayer->dl), ((uint32_item*)item)->id );
			if(vvert != NULL)
				send_verse_vert_uint32((uint32_item*)item, type);
			break;
		case VERSE_VERT_REAL32:	/* parent item has to exist */
			vnode = (((real32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 0 );
			vvert = (VerseVert*)BLI_dlist_find_link(&(vlayer->dl), ((real32_item*)item)->id );
			if( vvert != NULL)
				send_verse_vert_real32((real32_item*)item, type);
			break;
		case VERSE_VERT_VEC_REAL32:	/* parent item has to exist */
			vnode = (((vec_real32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 0 );
			vvert = (VerseVert*)BLI_dlist_find_link(&(vlayer->dl), ((vec_real32_item*)item)->id );
			if(vvert != NULL)
				send_verse_vert_vec_real32((vec_real32_item*)item, type);
			break;
		case VERSE_FACE_UINT8:	/* parent item has to exist */
			vnode = (((uint8_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 1 );
			vface = (VerseFace*)BLI_dlist_find_link(&(vlayer->dl), ((uint8_item*)item)->id );
			if(vface != NULL)
				send_verse_face_uint8((uint8_item*)item, type);
			break;
		case VERSE_FACE_UINT32:	/* parent item has to exist */
			vnode = (((uint32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 1 );
			vface = (VerseFace*)BLI_dlist_find_link(&(vlayer->dl), ((uint32_item*)item)->id );
			if(vface != NULL)
				send_verse_face_uint32((uint32_item*)item, type);
			break;
		case VERSE_FACE_REAL32:	/* parent item has to exist */
			vnode = (((real32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 1 );
			vface = (VerseFace*)BLI_dlist_find_link(&(vlayer->dl), ((real32_item*)item)->id );
			if(vface != NULL)
				send_verse_face_real32((real32_item*)item, type);
			break;
		case VERSE_FACE_QUAT_UINT32:	/* parent item has to exist */
			vnode = (((quat_uint32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 1 );
			vface = (VerseFace*)BLI_dlist_find_link(&(vlayer->dl), ((quat_uint32_item*)item)->id );
			if(vface != NULL)
				send_verse_face_corner_quat_uint32((quat_uint32_item*)item, type);
			break;
		case VERSE_FACE_QUAT_REAL32:	/* parent item has to exist */
			vnode = (((quat_real32_item*)item)->vlayer)->vnode;
			vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), 1 );
			vface = (VerseFace*)BLI_dlist_find_link(&(vlayer->dl), ((quat_real32_item*)item)->id );
			if(vface != NULL)
				send_verse_face_corner_quat_real32((quat_real32_item*)item, type);
			break;
	}
}

/*
 * return VerseLayer with certain content (vertexes, polygons, in the
 * future: weight, red color, etc.)
 */
VLayer* find_verse_layer_type(VGeomData *geom, short content)
{
	struct VLayer *vlayer = NULL;
	
	switch(content) {
		case VERTEX_LAYER:
			/* VERTEX_LAYER equals 0 and vertex layer is
			 * always in 1st layer */
			vlayer = geom->layers.da.items[VERTEX_LAYER];
			break;
		case POLYGON_LAYER:
			/* POLYGON_LAYER equals 1 and vertex layer is
			 * always in 2nd layer */
			vlayer = geom->layers.da.items[POLYGON_LAYER];
			break;
	}

	return vlayer;
}

/*
 * increase references of VerseVerts of new VerseFace
 */
static void increase_verse_verts_references(VerseFace *vface)
{
	if(vface->vvert0) vface->vvert0->counter++;
	if(vface->vvert1) vface->vvert1->counter++;
	if(vface->vvert2) vface->vvert2->counter++;
	if(vface->vvert3) vface->vvert3->counter++;
}

/*
 * move VerseFace from list of orphans to dlist of VerseFaces (if VerseFace was only changed
 * then this VerseFace is only removed from list of orphans)
 */
static void move_face_orphan_to_dlist(VNode *vnode, VLayer *vlayer, VerseFace *vface)
{
	/* remove vface from list of orphans */
	BLI_remlink(&(vlayer->orphans), vface);
	/* increase references of all vertexes beying part of this face*/
	increase_verse_verts_references(vface);

	if(vface->flag & FACE_RECEIVED) {
		/* set up vface flag */
		vface->flag &= ~FACE_RECEIVED;
		/* move vface to dynamic list of faces */
		BLI_dlist_add_item_index(&(vlayer->dl), (void*)vface, vface->id);
		/* recalculate all vertex and faces normals */
		recalculate_verseface_normals(vnode);
		/* post create action (change local data) */
		((VGeomData*)vnode->data)->post_polygon_create(vface);
	}
	else if(vface->flag & FACE_CHANGED) {
		/* set up vface flag */
		vface->flag &= ~FACE_CHANGED;
		/* move vface to dynamic list of faces */
		BLI_dlist_add_item_index(&(vlayer->dl), (void*)vface, vface->id);
		/* recalculate all vertex and faces normals */
		recalculate_verseface_normals(vnode);
		/* post create action (change local data) */
		((VGeomData*)vnode->data)->post_polygon_set_corner(vface);
	}
}

/*
 * find all VerseFaces waiting in queue, which needs id of new VerseVert
 */
static void find_unsent_faces(VNode *vnode, VerseVert *vvert)
{
	VLayer *vlayer;
	VerseFace *vface, *next_vface;

	vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	if(vlayer) {
		vface = vlayer->queue.first;
		while(vface) {
			next_vface = vface->next;
			if(vface->vvert0==vvert) {
				vface->v0 = vvert->id;
				vface->counter--;
			}
			else if(vface->vvert1==vvert) {
				vface->v1 = vvert->id;
				vface->counter--;
			}
			else if(vface->vvert2==vvert) {
				vface->v2 = vvert->id;
				vface->counter--;
			}
			else if(vface->vvert3==vvert){
				vface->v3 = vvert->id;
				vface->counter--;
			}

			if(vface->counter<1 && !(vface->flag & FACE_SENT))
				send_verse_face(vface);

			vface = next_vface;
		}
	}
}

/*
 * find all VerseFace orphans, which needs incoming VerseVert
 */
static void find_vlayer_orphans(VNode *vnode, VerseVert *vvert)
{
	VLayer *vlayer;
	VerseFace *vface, *next_vface;
	unsigned int vertex_id = vvert->id;

	vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	if(vlayer) {
		vface = vlayer->orphans.first;
		while(vface){
			next_vface = vface->next;
			if(vface->v0 == vertex_id) {
				vface->vvert0 = vvert;
				vface->counter--;
			}
			else if(vface->v1 == vertex_id) {
				vface->vvert1 = vvert;
				vface->counter--;
			}
			else if(vface->v2 == vertex_id) {
				vface->vvert2 = vvert;
				vface->counter--;
			}
			else if(vface->v3 == vertex_id) {
				vface->vvert3 = vvert;
				vface->counter--;
			}
			if(vface->counter<1) {
				/* moving VerseFace orphan to dlist */
				move_face_orphan_to_dlist(vnode, vlayer, vface);
			}
			vface = next_vface;
		}
	}
}

/*
 * return number of VerseVerts missing to incoming VerseFace, set up pointers
 * at VerseVerts
 */
static unsigned short test_incoming_verseface(VGeomData *geom, VerseFace *vface)
{
	struct VLayer *vert_layer;
	struct VerseVert *vvert; 
	int counter=0;

	vert_layer = find_verse_layer_type(geom, VERTEX_LAYER);

	if(vface->v0 != -1){
		vvert = BLI_dlist_find_link(&(vert_layer->dl), vface->v0);
		if(vvert==NULL) counter++;
		else vface->vvert0 = vvert;
	}
	if(vface->v1 != -1){
		vvert = BLI_dlist_find_link(&(vert_layer->dl), vface->v1);
		if(vvert==NULL) counter++;
		else vface->vvert1 = vvert;
	}
	if(vface->v2 != -1){
		vvert = BLI_dlist_find_link(&(vert_layer->dl), vface->v2);
		if(vvert==NULL) counter++;
		else vface->vvert2 = vvert;
	}
	if(vface->v3 != -1){
		vvert = BLI_dlist_find_link(&(vert_layer->dl), vface->v3);
		if(vvert==NULL) counter++;
		else vface->vvert3 = vvert;
	}
	
	return counter;
}

/*
 * try to find changed VerseFace in sending queue
 */
static VerseFace* find_changed_verse_face_in_queue(VLayer *vlayer, uint32 polygon_id)
{
	struct VerseFace *vface = vlayer->queue.first;

	while(vface){
		if(vface->id == polygon_id && vface->flag & FACE_CHANGED) {
			return vface;
		}
		vface = vface->next;
	}
	return NULL;
}

/*
 * try to find VerseFace in queue
 */
static VerseFace* find_verse_face_in_queue(
		VLayer *vlayer,
		VNodeID node_id,
		uint32 polygon_id,
		uint32 v0,
		uint32 v1,
		uint32 v2,
		uint32 v3)
{
	struct VerseFace *vface = vlayer->queue.first;

	while(vface){
		if((vface->v0==v0) && (vface->v1==v1) && (vface->v2==v2) && (vface->v3==v3)){
			vface->id = polygon_id;
			vface->vlayer = vlayer;
			return vface;
		}
		vface = vface->next;
	}
	return NULL;
}

/*
 * try to find VerseVert in queue
 */
static VerseVert* find_verse_vert_in_queue(
		VLayer *vlayer,
		VNodeID node_id,
		uint32 vertex_id,
		real32 x,
		real32 y,
		real32 z)
{
	struct VerseVert *vvert = vlayer->queue.first;

	while(vvert){
		if((vvert->vlayer->vnode->id == node_id) && (vvert->co[0] == x) && (vvert->co[1] == y) && (vvert->co[2] == z))
		{
			vvert->id = vertex_id;
			vvert->vlayer = vlayer;

			return vvert;
		}
		vvert = vvert->next;
	}

	return NULL;
}


/*
 * send quat of float values to verse server (4x32 bits)
 */
void send_verse_face_corner_quat_real32(quat_real32_item *item, short type)
{
	verse_send_g_polygon_set_corner_real32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value[0],
			item->value[1],
			item->value[2],
			item->value[3]);
}

/*
 * send quat of unsigned int values to verse server (4x32 bits)
 */
void send_verse_face_corner_quat_uint32(quat_uint32_item *item, short type)
{
	verse_send_g_polygon_set_corner_uint32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value[0],
			item->value[1],
			item->value[2],
			item->value[3]);
}

/*
 * send float value (32 bits) to verse server
 */
void send_verse_face_real32(real32_item *item, short type)
{
	verse_send_g_polygon_set_face_real32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value);
}

/*
 * send unsigned integer (32 bits) to verse server
 */
void send_verse_face_uint32(uint32_item *item, short type)
{
	verse_send_g_polygon_set_face_uint32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value);
}

/*
 * send unsigned char (8 bits) to verse server
 */
void send_verse_face_uint8(uint8_item *item, short type)
{
	verse_send_g_polygon_set_face_uint8(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value);
}

/*
 * send vector of float values to verse server (3x32 bits)
 */
void send_verse_vert_vec_real32(vec_real32_item *item, short type)
{
	verse_send_g_vertex_set_xyz_real32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value[0],
			item->value[1],
			item->value[2]);
}

/*
 * send float value (32 bits) to verse server
 */
void send_verse_vert_real32(real32_item *item, short type)
{
	verse_send_g_vertex_set_real32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value);
}

/*
 * send unsigned integer (32 bits) to verse server
 */
void send_verse_vert_uint32(uint32_item *item, short type)
{
	verse_send_g_vertex_set_uint32(
			item->vlayer->vnode->id,
			item->vlayer->id,
			item->id,
			item->value);
}

/*
 * send delete command to verse server
 */
void send_verse_vertex_delete(VerseVert *vvert)
{
	verse_session_set(vvert->vlayer->vnode->session->vsession);

	vvert->flag |= VERT_OBSOLETE;
	
	verse_send_g_vertex_delete_real32(vvert->vlayer->vnode->id, vvert->id);
}

/*
 * send VerseLayer to verse server
 */
void send_verse_layer(VLayer *vlayer)
{
	verse_session_set(vlayer->vnode->session->vsession);

	verse_send_g_layer_create(
			vlayer->vnode->id,
			vlayer->id,
			vlayer->name,
			vlayer->type,
			vlayer->def_int,
			vlayer->def_real);
}

/* 
 * send VerseVert to verse server
 */
void send_verse_vertex(VerseVert *vvert)
{
	/* new vertex position will not be sent, when vertex was deleted */
	if(vvert->flag & VERT_OBSOLETE) return;
	
	verse_session_set(vvert->vlayer->vnode->session->vsession);

	verse_send_g_vertex_set_xyz_real32(
			vvert->vlayer->vnode->id,
			vvert->vlayer->id,
			vvert->id,
			vvert->co[0],
			vvert->co[2],
			-vvert->co[1]);
}

/*
 * send delete command to verse server
 */
void send_verse_face_delete(VerseFace *vface)
{
	verse_session_set(vface->vlayer->vnode->session->vsession);

	vface->flag |= FACE_DELETED;

	verse_send_g_polygon_delete(vface->vlayer->vnode->id, vface->id);
}

/*
 * send VerseFace to verse server
 */
static void send_verse_face(VerseFace *vface)
{
	verse_session_set(vface->vlayer->vnode->session->vsession);

	vface->flag |= FACE_SENT;

	if(vface->v3 != -1) {
		verse_send_g_polygon_set_corner_uint32(
				vface->vlayer->vnode->id,
				vface->vlayer->id,
				vface->id,
				vface->v0,
				vface->v3,	/* verse use clock-wise winding */
				vface->v2,
				vface->v1);	/* verse use clock-wise winding */
	}
	else {
		verse_send_g_polygon_set_corner_uint32(
				vface->vlayer->vnode->id,
				vface->vlayer->id,
				vface->id,
				vface->v0,
				vface->v2,	/* verse use clock-wise winding */
				vface->v1,	/* verse use clock-wise winding */
				vface->v3);
	}
}

/*
 * free VerseVert
 */
static void free_verse_vertex(VLayer *vlayer, VerseVert *vvert)
{
	/* free VerseVert */
	BLI_freelinkN(&(vlayer->orphans), vvert);
}

/*
 * free VerseFace (and blender face)
 */
static void free_verse_face(VLayer *vlayer, VerseFace *vface)
{
	/* free VerseFace */
	BLI_dlist_free_item(&(vlayer->dl), (unsigned int)vface->id);
}

/*
 * free VerseLayer data
 */
static void free_verse_layer_data(VNode *vnode, VLayer *vlayer)
{
	struct VerseFace *vface;
	struct VerseVert *vvert;

	/* set up EditVert->vvert and EditFace->vface pointers to NULL */
	switch(vlayer->content) {
		case VERTEX_LAYER:
			vvert = (VerseVert*)vlayer->dl.lb.first;
			while(vvert) {
				((VGeomData*)vnode->data)->post_vertex_free_constraint(vvert);
				vvert = vvert->next;
			}
			break;
		case POLYGON_LAYER:
			vface = (VerseFace*)vlayer->dl.lb.first;
			while(vface) {
				((VGeomData*)vnode->data)->post_polygon_free_constraint(vface);
				vface = vface->next;
			}
			break;
		default:
			break;
	}
	/* free Verse Layer name */
	MEM_freeN(vlayer->name);
	/* destroy VerseLayer data (vertexes, polygons, etc.) */
	BLI_dlist_destroy(&(vlayer->dl));
	/* free unsent data */
	BLI_freelistN(&(vlayer->queue));
	/* free orphans */
	BLI_freelistN(&(vlayer->orphans));
}

/*
 * free all unneeded VerseVerts waiting for deleting
 */
static void free_unneeded_verseverts_of_verseface(VNode *vnode, VerseFace *vface)
{
	struct VLayer *vert_vlayer;

	/* find layer containing vertexes */
	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);

	/* free all "deleted" VerseVert waiting for deleting this VerseFace */
	
	if((vface->vvert0->counter < 1) && (vface->vvert0->flag & VERT_DELETED)) {
		((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert0);
		free_verse_vertex(vert_vlayer, vface->vvert0);
		vface->vvert0 = NULL;
	}
	if((vface->vvert1->counter < 1) && (vface->vvert1->flag & VERT_DELETED)) {
		((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert1);
		free_verse_vertex(vert_vlayer, vface->vvert1);
		vface->vvert1 = NULL;
	}
	if((vface->vvert2->counter < 1) && (vface->vvert2->flag & VERT_DELETED)) {
		((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert2);
		free_verse_vertex(vert_vlayer, vface->vvert2);
		vface->vvert2 = NULL;
	}
	if((vface->vvert3) && (vface->vvert3->counter < 1) && (vface->vvert3->flag & VERT_DELETED)) {
		((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert3);
		free_verse_vertex(vert_vlayer, vface->vvert3);
		vface->vvert3 = NULL;
	}
}

/*
 * This function create VerseVert and returns pointer on this vertex
 */
VerseVert* create_verse_vertex(
		VLayer *vlayer,
		uint32 vertex_id,
		real32 x,
		real32 y,
		real32 z)
{
	struct VerseVert *vvert;

	vvert = (VerseVert*)MEM_mallocN(sizeof(VerseVert), "VerseVert");
	
	/* set up pointer on parent node */
	vvert->vlayer = vlayer;
	vvert->id = vertex_id;
	/* position */
	vvert->co[0] = x;
	vvert->co[1] = y;
	vvert->co[2] = z;
	/* normal */
	vvert->no[0] = vvert->no[1] = vvert->no[2] = 0.0;
	/* blender internals */
	vvert->flag = 0;
	vvert->counter = 0;
	vvert->vertex = NULL;

	/* increase layer counter of vertexes */
	vlayer->counter++;

	return vvert;
}

/*
 * this function creates fake VerseEdge and returns pointer at this edge
 */
VerseEdge *create_verse_edge(uint32 v0, uint32 v1)
{
	struct VerseEdge *vedge;

	vedge = (VerseEdge*)MEM_mallocN(sizeof(VerseEdge), "VerseEdge");

	vedge->v0 = v0;
	vedge->v1 = v1;
	vedge->counter = 0;

	return vedge;
}

/*
 * this function will create new VerseFace and will return pointer on such Face
 */
VerseFace* create_verse_face(
		VLayer *vlayer,
		uint32 polygon_id,
		uint32 v0,
		uint32 v1,
		uint32 v2,
		uint32 v3)
{
	struct VerseFace *vface;

	vface = (VerseFace*)MEM_mallocN(sizeof(VerseFace), "VerseFace");

	/* verse data */
	vface->vlayer = vlayer;
	vface->id = polygon_id;

	vface->vvert0 = NULL;
	vface->vvert1 = NULL;
	vface->vvert2 = NULL;
	vface->vvert3 = NULL;

	vface->v0 = v0;
	vface->v1 = v1;
	vface->v2 = v2;
	vface->v3 = v3;

	/* blender data */
	vface->face = NULL;
	vface->flag = 0;
	vface->counter = 4;

	/* increase layer counter of faces */
	vlayer->counter++;
	
	return vface;
}

/*
 * create and return VerseLayer
 */
VLayer *create_verse_layer(
		VNode *vnode,
		VLayerID layer_id,
		const char *name,
		VNGLayerType type,
		uint32 def_integer,
		real64 def_real)
{
	struct VLayer *vlayer;

	/* add layer to the DynamicList */
	vlayer = (VLayer*)MEM_mallocN(sizeof(VLayer), "VerseLayer");

	/* store all relevant info to the vlayer and set up vlayer */
	vlayer->vnode = vnode;
	vlayer->id = layer_id;
	vlayer->name = (char*)MEM_mallocN(sizeof(char)*(sizeof(name)+1),"Verse Layer name");
	strcpy(vlayer->name, name);
	vlayer->type = type;
	vlayer->def_int = def_integer;
	vlayer->def_real = def_real;

	if((type == VN_G_LAYER_VERTEX_XYZ) && (layer_id == 0))
		vlayer->content = VERTEX_LAYER;
	else if((type == VN_G_LAYER_POLYGON_CORNER_UINT32) && (layer_id == 1))
		vlayer->content = POLYGON_LAYER;
	else
		vlayer->content = -1;

	/* initialize DynamicList in the vlayer (vertexes, polygons, etc.)*/
	BLI_dlist_init(&(vlayer->dl));
	/* initialization of queue of layer */
	vlayer->queue.first = vlayer->queue.last = NULL;
	/* initialization of list of orphans */
	vlayer->orphans.first = vlayer->orphans.last = NULL;
	/* initialize number of sent items (vertexes, faces, etc) */
	vlayer->counter = 0;
	/* initialize flag */
	vlayer->flag = 0;

	/* set up methods */
	//XXX vlayer->post_layer_create = post_layer_create;
	//XXX vlayer->post_layer_destroy = post_layer_destroy;

	return vlayer;
}

/*
 * create geometry data
 */
VGeomData *create_geometry_data(void)
{
	struct VGeomData *geom;

	geom = (VGeomData*)MEM_mallocN(sizeof(VGeomData), "VerseGeometryData");
	BLI_dlist_init(&(geom->layers));
	geom->vlink = NULL;
	geom->queue.first = geom->queue.last = NULL;
	geom->mesh = NULL;
	geom->editmesh = NULL;

	/* initialize list of fake verse edges and initialize verse edge hash */
	geom->edges.first = geom->edges.last = NULL;
	geom->hash = MEM_callocN(VEDHASHSIZE*sizeof(HashVerseEdge), "verse hashedge tab");

	/* set up methods */
	//XXX geom->post_vertex_create = post_vertex_create;
	//XXX geom->post_vertex_set_xyz = post_vertex_set_xyz;
	//XXX geom->post_vertex_delete = post_vertex_delete;
	//XXX geom->post_vertex_free_constraint = post_vertex_free_constraint;
	//XXX geom->post_polygon_create = post_polygon_create;
	//XXX geom->post_polygon_set_corner = post_polygon_set_corner;
	//XXX geom->post_polygon_delete = post_polygon_delete;
	//XXX geom->post_polygon_free_constraint = post_polygon_free_constraint;
	//XXX geom->post_geometry_free_constraint = post_geometry_free_constraint;
	//XXX geom->post_polygon_set_uint8 = post_polygon_set_uint8;

	return geom;
}

/* Create item containing 4 floats */
static quat_real32_item *create_quat_real32_item(
		VLayer *vlayer,
		uint32 item_id,
		real32 v0,
		real32 v1,
		real32 v2,
		real32 v3)
{
	struct quat_real32_item *item;

	item = (quat_real32_item*)MEM_mallocN(sizeof(quat_real32_item), "quat_real32_item");

	item->vlayer = vlayer;
	item->id = item_id;
	item->value[0] = v0;
	item->value[1] = v1;
	item->value[2] = v2;
	item->value[3] = v3;

	return item;
}

/* Create item containing 1 float */
static real32_item *create_real32_item(VLayer *vlayer, uint32 item_id, real32 value)
{
	struct real32_item *item;

	item = (real32_item*)MEM_mallocN(sizeof(real32_item), "real32_item");

	item->vlayer = vlayer;
	item->id = item_id;
	item->value = value;

	return item;
}

/* Create item containing 1 integer */
static uint32_item *create_uint32_item(VLayer *vlayer, uint32 item_id, uint32 value)
{
	struct uint32_item *item;

	item = (uint32_item*)MEM_mallocN(sizeof(uint32_item), "uint32_item");

	item->vlayer = vlayer;
	item->id = item_id;
	item->value = value;

	return item;
}

/* Create item containing 1 byte */
static uint8_item *create_uint8_item(VLayer *vlayer, uint32 item_id, uint8 value)
{
	struct uint8_item *item;

	item = (uint8_item*)MEM_mallocN(sizeof(uint8_item), "uint8_item");

	item->vlayer = vlayer;
	item->id = item_id;
	item->value = value;

	return item;
}

/*
 * callback function: vertex crease was set
 */
static void cb_g_crease_set_vertex(
		void *user_data,
		VNodeID node_id,
		const char *layer,
		uint32 def_crease)
{
}

/*
 * we have to test corretness of incoming data from verse server
 * no two vertexes can have the same index
 */
static char test_polygon_set_corner_uint32(
		uint32 v0,
		uint32 v1,
		uint32 v2,
		uint32 v3)
{
	if((v0==v1) || (v0==v2) || (v0==v3) || (v1==v2) || (v1==v3) || (v2==v3))
		return 0;
	else
		return 1;
}

/*
 * try to find verse layer in sending queue of verse geometry node
 */
static VLayer *find_vlayer_in_sending_queue(VNode *vnode, VLayerID layer_id)
{
	struct VLayer *vlayer;
	
	/* try to find verse layyer in sending queue */
	vlayer = ((VGeomData*)vnode->data)->queue.first;
	while(vlayer) {
		if(vlayer->id==layer_id) return vlayer;
		vlayer = vlayer->next;
	}

	return NULL;
}

/*
 * this function will find edge in hash table, hash function isn't too optimal (it needs
 * lot of memory for every verse node), but it works without any bug
 */
static VerseEdge* find_verse_edge(VNode *vnode, uint32 v0, uint32 v1)
{
	struct HashVerseEdge *hve;

	if(((VGeomData*)vnode->data)->hash==NULL)
		((VGeomData*)vnode->data)->hash = MEM_callocN(VEDHASHSIZE*sizeof(HashVerseEdge), "verse hashedge tab");

	hve = ((VGeomData*)vnode->data)->hash + VEDHASH(v0, v1);;
	while(hve) {
		/* edge v0---v1 is the same edge as v1---v0 */
		if(hve->vedge && ((hve->vedge->v0==v0 && hve->vedge->v1==v1) || (hve->vedge->v0==v1 && hve->vedge->v1==v0))) return hve->vedge;
		hve = hve->next;
	}

	return NULL;
}

/*
 * insert hash of verse edge to hash table
 */
static void insert_verse_edgehash(VNode *vnode, VerseEdge *vedge)
{
	struct HashVerseEdge *first, *hve;

	if(((VGeomData*)vnode->data)->hash==NULL)
		((VGeomData*)vnode->data)->hash = MEM_callocN(VEDHASHSIZE*sizeof(HashVerseEdge), "verse hashedge tab");

	first = ((VGeomData*)vnode->data)->hash + VEDHASH(vedge->v0, vedge->v1);

	if(first->vedge==NULL) {
		first->vedge = vedge;
	}
	else {
		hve = &(vedge->hash);
		hve->vedge = vedge;
		hve->next = first->next;
		first->next = hve;
	}
}

/*
 * remove hash of verse edge from hash table
 */
static void remove_verse_edgehash(VNode *vnode, VerseEdge *vedge)
{
	struct HashVerseEdge *first, *hve, *prev;

	hve = first = ((VGeomData*)vnode->data)->hash + VEDHASH(vedge->v0, vedge->v1);

	while(hve) {
		if(hve->vedge == vedge) {
			if(hve==first) {
				if(first->next) {
					hve = first->next;
					first->vedge = hve->vedge;
					first->next = hve->next;
				}
				else {
					hve->vedge = NULL;
				}
			}
			else {
				prev->next = hve->next;
			}
			return;
		}
		prev = hve;
		hve = hve->next;
	}
}

/*
 * this function will try to remove existing fake verse edge, when this verse
 * edge is still used by some faces, then counter will be only decremented
 */
static void remove_verse_edge(VNode *vnode, uint32 v0, uint32 v1)
{
	struct VerseEdge *vedge;

	vedge = find_verse_edge(vnode, v0, v1);
	if(vedge) {
		vedge->counter--;
		if(vedge->counter==0) {
			remove_verse_edgehash(vnode, vedge);
			BLI_freelinkN(&(((VGeomData*)vnode->data)->edges), vedge);
		}
	}
	else {
		printf("error: remove_verse_edge %d, %d\n", v0, v1);
	}
}

/*
 * this function will try to add new fake verse edge, when no such edge exist,
 * when such edge exist, then only counter of edge will be incremented
 */
static void add_verse_edge(VNode *vnode, uint32 v0, uint32 v1)
{
	struct VerseEdge *vedge;

	vedge = find_verse_edge(vnode, v0, v1);
	if(!vedge) {
		if(v0!=v1) {
			vedge = create_verse_edge(v0, v1);
			BLI_addtail(&(((VGeomData*)vnode->data)->edges), vedge);
			insert_verse_edgehash(vnode, vedge);
		}
		else {
			printf("error:add_verse_edge: %d, %d\n", v0, v1);
			return;
		}
	}
	vedge->counter++;
}

/*
 * verse face was deleted ... update edge hash
 */
static void update_edgehash_of_deleted_verseface(VNode *vnode, VerseFace *vface)
{
	uint32 v0, v1, v2, v3;		/* verse vertex indexes of deleted verse face */
	
	v0 = vface->vvert0->id;
	v1 = vface->vvert1->id;
	v2 = vface->vvert2->id;
	v3 = vface->vvert3 ? vface->vvert3->id : -1;

	remove_verse_edge(vnode, v0, v1);
	remove_verse_edge(vnode, v1, v2);
	if(v3!=-1) {
		remove_verse_edge(vnode, v2, v3);
		remove_verse_edge(vnode, v3, v0);
	}
	else {
		remove_verse_edge(vnode, v2, v0);
	}
}

/*
 * existing verse face was changed ... update edge hash
 */
static void update_edgehash_of_changed_verseface(
		VNode *vnode,
		VerseFace *vface,
		uint32 v0,
		uint32 v1,
		uint32 v2,
		uint32 v3)
{
	uint32 ov0, ov1, ov2, ov3;	/* old indexes at verse vertexes*/
	
	ov0 = vface->vvert0->id;
	ov1 = vface->vvert1->id;
	ov2 = vface->vvert2->id;
	ov3 = vface->vvert3 ? vface->vvert3->id : -1;

	/* 1st edge */
	if(v0!=ov0 || v1!=ov1) {
		remove_verse_edge(vnode, ov0, ov1);
		add_verse_edge(vnode, v0, v1);
	}
	
	/* 2nd edge */
	if(v1!=ov1 || v2!=ov2) {
		remove_verse_edge(vnode, ov1, ov2);
		add_verse_edge(vnode, v1, v2);
	}

	/* 3rd edge */
	if(v2!=ov2 || v3!=ov3 || v0!=ov0) {
		if(ov3!=-1) {
			remove_verse_edge(vnode, ov2, ov3);
			if(v3!=-1) {
				add_verse_edge(vnode, v2, v3);		/* new 3rd edge (quat->quat) */
			}
			else {
				remove_verse_edge(vnode, ov3, ov0);	/* old edge v3,v0 of quat have to be removed */
				add_verse_edge(vnode, v2, v0);		/* new 3rd edge (quat->triangle) */	
			}
		}
		else {
			remove_verse_edge(vnode, ov2, ov0);
			if(v3!=-1) {
				add_verse_edge(vnode, v2, v3);		/* new 3rd edge (triangle->quat) */
			}
			else {
				add_verse_edge(vnode, v2, v0);		/* new 3rd edge (triangle->triangle) */
			}
		}
	}

	/* 4th edge */
	if(v3!=-1 && (v3!=ov3 || v0!=ov0)) {
		remove_verse_edge(vnode, ov3, ov0);
		add_verse_edge(vnode, v3, v0);
	}
}

/*
 * new verse face was created ... update list of edges and edge has
 */
static void update_edgehash_of_new_verseface(
		VNode *vnode,
		uint32 v0,
		uint32 v1,
		uint32 v2,
		uint32 v3)
{
	/* when edge already exists, then only its counter is incremented,
	 * look at commentary of add_verse_edge() function */
	add_verse_edge(vnode, v0, v1);
	add_verse_edge(vnode, v1, v2);
	if(v3!=-1) {
		add_verse_edge(vnode, v2, v3);
		add_verse_edge(vnode, v3, v0);
	}
	else {
		add_verse_edge(vnode, v2, v0);
	}
}

/*
 * callback function: edge crease was set
 */
static void cb_g_crease_set_edge(
		void *user_data,
		VNodeID node_id,
		const char *layer,
		uint32 def_crease)
{
}

/*
 * callback function: float value for polygon was set up
 */
static void cb_g_polygon_set_face_real32(
		void *user_def,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 polygon_id,
		real32 value)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct real32_item *item;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	/* find layer containing uint_8 data */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);

	/* try to find item*/
	item = BLI_dlist_find_link(&(vlayer->dl), polygon_id);

	if(item) {
		item->value = value;
	}
	else {
		item = create_real32_item(vlayer, polygon_id, value);
		BLI_dlist_add_item_index(&(vlayer->dl), item, item->id);
	}
}

/*
 * callback function: int values for polygon was set up
 */
static void cb_g_polygon_set_face_uint32(
		void *user_def,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 polygon_id,
		uint32 value)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct uint32_item *item;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	/* find layer containing uint_8 data */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);

	/* try to find item*/
	item = BLI_dlist_find_link(&(vlayer->dl), polygon_id);

	if(item) {
		item->value = value;
	}
	else {
		item = create_uint32_item(vlayer, polygon_id, value);
		BLI_dlist_add_item_index(&(vlayer->dl), item, item->id);
	}
}

/*
 * callback function: uint8 value for polygon was set up
 */
static void cb_g_polygon_set_face_uint8(
		void *user_def,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 polygon_id,
		uint8 value)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct uint8_item *item;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	/* find layer containing uint_8 data */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);

	/* try to find item*/
	item = BLI_dlist_find_link(&(vlayer->dl), polygon_id);

	if(item) {
		item->value = value;
	}
	else {
		item = create_uint8_item(vlayer, polygon_id, value);
		BLI_dlist_add_item_index(&(vlayer->dl), item, item->id);
	}
}

/*
 * callback function: float value for polygon corner was set up
 */
static void cb_g_polygon_set_corner_real32(
		void *user_def,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 polygon_id,
		real32 v0,
		real32 v1,
		real32 v2,
		real32 v3)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct quat_real32_item *item;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	/* find layer containing uint_8 data */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);

	/* try to find item*/
	item = BLI_dlist_find_link(&(vlayer->dl), polygon_id);

	if(item) {
		item->value[0] = v0;
		item->value[1] = v1;
		item->value[2] = v2;
		item->value[3] = v3;
	}
	else {
		item = create_quat_real32_item(vlayer, polygon_id, v0, v1, v2, v3);
		BLI_dlist_add_item_index(&(vlayer->dl), item, item->id);
	}
}

/*
 * callback function: polygon is deleted
 */
static void cb_g_polygon_delete(
		void *user_data,
		VNodeID node_id,
		uint32 polygon_id)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	VNode *vnode;
	VLayer *vlayer;
	VerseFace *vface;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = BLI_dlist_find_link(&(session->nodes), node_id);

	/* find layer containing faces */
	vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	/* find wanted VerseFace */
	vface = BLI_dlist_find_link(&(vlayer->dl), polygon_id);

	if(!vface) return;

	/* update edge hash */
	update_edgehash_of_deleted_verseface(vnode, vface);
	
	((VGeomData*)vnode->data)->post_polygon_delete(vface);

	/* decrease references at coresponding VerseVertexes */
	vface->vvert0->counter--;
	vface->vvert1->counter--;
	vface->vvert2->counter--;
	if(vface->vvert3) vface->vvert3->counter--;

	/* delete unneeded VerseVertexes */
	free_unneeded_verseverts_of_verseface(vnode, vface);
	
	free_verse_face(vlayer, vface);
}

/*
 * callback function: new polygon (face) created or existing polygon was changed
 */
static void cb_g_polygon_set_corner_uint32(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 polygon_id,
		uint32 v0,
		uint32 v1,
		uint32 v2,
		uint32 v3)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct VerseFace *vface=NULL;

	if(!session) return;

	/* try to find VerseNode */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	if(!vnode) return;

	/* try to find VerseLayer */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);
	if(!vlayer) return;

	/* we have to test coretness of incoming data */
	if(!test_polygon_set_corner_uint32(v0, v1, v2, v3)) return;
	
	/* Blender uses different order of vertexes */
	if(v3!=-1) { /* quat swap */
		unsigned int v; v = v1; v1 = v3; v3 = v;
	}
	else { /* triangle swap */
		unsigned int v; v = v1; v1 = v2; v2 = v;
	}

	/* try to find VerseFace */
	vface = (VerseFace*)BLI_dlist_find_link(&(vlayer->dl), (unsigned int)polygon_id);

	/* try to find modified VerseFace */
	if(!vface) {
		vface = find_changed_verse_face_in_queue(vlayer, polygon_id);
		if(vface) {
			BLI_remlink(&(vlayer->queue), (void*)vface);
			BLI_dlist_add_item_index(&(vlayer->dl), (void*)vface, (unsigned int)polygon_id);
		}
	}

	if(!vface) {
		/* try to find VerseFace in list of VerseVaces created by me and set up polygon and
		 * layer ids */
		vface = find_verse_face_in_queue(vlayer, node_id, polygon_id, v0, v1, v2, v3);
		
		/* update edge hash */
		update_edgehash_of_new_verseface(vnode, v0, v1, v2, v3);
		
		if(vface){
			/* I creeated this face ... remove VerseFace from queue */
			BLI_remlink(&(vlayer->queue), (void*)vface);
		}
		else {
			/* some other client created this face*/
			vface = create_verse_face(vlayer, polygon_id, v0, v1, v2, v3);
		}

		vface->flag &= ~FACE_SENT;

		/* return number of missing verse vertexes */
		vface->counter = test_incoming_verseface((VGeomData*)vnode->data, vface);

		if(vface->counter < 1) {
			/* when VerseFace received all needed VerseFaces, then it is moved
			 * to list of VerseFaces */
			BLI_dlist_add_item_index(&(vlayer->dl), (void*)vface, (unsigned int)polygon_id);
			increase_verse_verts_references(vface);
			recalculate_verseface_normals(vnode);
			((VGeomData*)vnode->data)->post_polygon_create(vface);
		}
		else {
			/* when all needed VerseVertexes weren't received, then VerseFace is moved to
			 * the list of orphans waiting on needed vertexes */
			vface->flag |= FACE_RECEIVED;
			BLI_addtail(&(vlayer->orphans), (void*)vface);
		}
	}
	else {
		VLayer *vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
		/* VerseVertexes of existing VerseFace were changed (VerseFace will use some different
		 * VerseVertexes or it will use them in different order) */

		/* update fake verse edges */
		update_edgehash_of_changed_verseface(vnode, vface, v0, v1, v2, v3);
		
		/* initialize count of unreceived vertexes needed for this face */
		vface->counter = 4;

		/* 1st corner */
		if(vface->vvert0->id != v0) {
			/* decrease references of obsolete vertexes*/
			vface->vvert0->counter--;
			/* delete this vertex, when it isn't used by any face and it was marked as deleted */
			if((vface->vvert0->counter < 1) && (vface->vvert0->flag & VERT_DELETED)) {
				((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert0);
				free_verse_vertex(vert_vlayer, vface->vvert0);
			}
			/* try to set up new pointer at verse vertex */
			vface->v0 = v0;
			vface->vvert0 = BLI_dlist_find_link(&(vert_vlayer->dl), vface->v0);
			if(vface->vvert0) {
				/* increase references at new vertex */
				vface->vvert0->counter++;
				/* decrease count of needed vertex to receive */
				vface->counter--;
			}
			
		}
		else
			/* this corner wasn't changed */
			vface->counter--;

		/* 2nd corner */
		if(vface->vvert1->id != v1) {
			vface->vvert1->counter--;
			if((vface->vvert1->counter < 1) && (vface->vvert1->flag & VERT_DELETED)) {
				((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert1);
				free_verse_vertex(vert_vlayer, vface->vvert1);
			}
			vface->v1 = v1;
			vface->vvert1 = BLI_dlist_find_link(&(vert_vlayer->dl), vface->v1);
			if(vface->vvert1) {
				vface->vvert1->counter++;
				vface->counter--;
			}
		}
		else
			vface->counter--;

		/* 3rd corner */
		if(vface->vvert2->id != v2) {
			vface->vvert2->counter--;
			if((vface->vvert2->counter < 1) && (vface->vvert2->flag & VERT_DELETED)) {
				((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert2);
				free_verse_vertex(vert_vlayer, vface->vvert2);
			}
			vface->v2 = v2;
			vface->vvert2 = BLI_dlist_find_link(&(vert_vlayer->dl), vface->v2);
			if(vface->vvert2) {
				vface->vvert2->counter++;
				vface->counter--;
			}
		}
		else
			vface->counter--;
	
		/* 4th corner */	
		if(vface->vvert3) {
			if(vface->vvert3->id != v3) {
				vface->vvert3->counter--;
				if((vface->vvert3->counter < 1) && (vface->vvert3->flag & VERT_DELETED)) {
					((VGeomData*)vnode->data)->post_vertex_delete(vface->vvert3);
					free_verse_vertex(vert_vlayer, vface->vvert3);
				}
				vface->v3 = v3;
				if(v3 != -1) {
					vface->vvert3 = BLI_dlist_find_link(&(vert_vlayer->dl), vface->v3);
					if(vface->vvert3) {
						vface->vvert3->counter++;
						vface->counter--;
					}
				}
				else {
					/* this is some special case, this face hase now only 3 corners
					 * quat -> triangle */
					vface->vvert3 = NULL;
					vface->counter--;
				}
			}
		}
		else if(v3 != -1)
			/* this is some special case, 4th corner of this polygon was created
			 * triangle -> quat */
			vface->v3 = v3;
			vface->vvert3 = BLI_dlist_find_link(&(vert_vlayer->dl), vface->v3);
			if(vface->vvert3) {
				vface->vvert3->counter++;
				vface->counter--;
			}
		else {
			vface->v3 = -1;
			vface->counter--;
		}
		
		vface->flag &= ~FACE_SENT;
		vface->flag |= FACE_CHANGED;

		if(vface->counter<1) {
			vface->flag &= ~FACE_CHANGED;
			recalculate_verseface_normals(vnode);
			((VGeomData*)vnode->data)->post_polygon_set_corner(vface);
		}
		else {
			/* when all needed VerseVertexes weren't received, then VerseFace is added to
			 * the list of orphans waiting on needed vertexes */
			BLI_dlist_rem_item(&(vlayer->dl), vface->id);
			BLI_addtail(&(vlayer->orphans), (void*)vface);
		}
	}
}

/*
 * callback function: float value was set up for VerseVert with vertex_id
 */
static void cb_g_vertex_set_real32(
		void *user_def,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 vertex_id,
		real32 value)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct real32_item *item;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	/* find layer containing uint_8 data */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);

	/* try to find item*/
	item = BLI_dlist_find_link(&(vlayer->dl), vertex_id);

	if(item) {
		item->value = value;
	}
	else {
		item = create_real32_item(vlayer, vertex_id, value);
		BLI_dlist_add_item_index(&(vlayer->dl), item, item->id);
	}
}

/*
 * callback function: int value was set up for VerseVert with vertex_id
 */
static void cb_g_vertex_set_uint32(
		void *user_def,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 vertex_id,
		uint32 value)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;
	struct uint32_item *item;

	if(!session) return;

	/* find needed node (we can be sure, that it is geometry node) */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	/* find layer containing uint_8 data */
	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);

	/* try to find item*/
	item = BLI_dlist_find_link(&(vlayer->dl), vertex_id);

	if(item) {
		item->value = value;
	}
	else {
		item = create_uint32_item(vlayer, vertex_id, value);
		BLI_dlist_add_item_index(&(vlayer->dl), item, item->id);
	}
}

/*
 * callback function: polygon was deleted
 */
static void cb_g_vertex_delete_real32(
		void *user_data,
		VNodeID node_id,
		uint32 vertex_id)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	VNode *vnode=NULL;
	VLayer *vert_vlayer=NULL;
	VerseVert *vvert=NULL;

	if(!session) return;

	vnode = BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);

	vvert = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vertex_id);

	if(!vvert) return;

	if(vvert->counter < 1) {
		((VGeomData*)vnode->data)->post_vertex_delete(vvert);
		BLI_dlist_free_item(&(vert_vlayer->dl), (unsigned int)vertex_id);
	}
	else {
		/* some VerseFace(s) still need VerseVert, remove verse vert from
		 * list verse vertexes and put it to list of orphans */
		vvert->flag |= VERT_DELETED;
		BLI_dlist_rem_item(&(vert_vlayer->dl), (unsigned int)vertex_id);
		BLI_addtail(&(vert_vlayer->orphans), vvert);
	}
}

/*
 * callback function: position of one vertex was changed or new vertex was created
 */
static void cb_g_vertex_set_xyz_real32(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id,
		uint32 vertex_id,
		real32 x,
		real32 y,
		real32 z)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode = NULL;
	struct VLayer *vlayer = NULL;
	struct VerseVert *vvert = NULL;
	real32 tmp;

	if(!session) return;

	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	if(!vnode)return;

	vlayer = (VLayer*)BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), (unsigned int)layer_id);
	if(!vlayer) return;

	/* switch axis orientation */
	tmp = y;
	y = -z;
	z = tmp;
	
	if(vlayer->id == 0) {
		/* try to pick up verse vert from DynamicList */
		vvert = (VerseVert*)BLI_dlist_find_link(&(vlayer->dl), (unsigned int)vertex_id);

		if(vvert) {
			if(vvert->flag & VERT_OBSOLETE) return;

			if (vvert->flag & VERT_LOCKED) {
				/* this application changed position of this vertex */
				if((vvert->co[0]==x) && (vvert->co[1]==y) && (vvert->co[2]==z)) {
					/* unlock vertex position */
					vvert->flag &= ~VERT_LOCKED;
					/* call post_vertex_set_xyz only, when position of vertex is
					 * obsolete ... the new vertex position will be sent to
					 * verse server */
					if (vvert->flag & VERT_POS_OBSOLETE) {
						((VGeomData*)vnode->data)->post_vertex_set_xyz(vvert);
					}
				}
			}
			else {
				/* somebody else changed position of this vertex*/
				if((vvert->co[0]!=x) || (vvert->co[1]!=y) || (vvert->co[2]!=z)) {
					vvert->co[0] = x;
					vvert->co[1] = y;
					vvert->co[2] = z;
					recalculate_verseface_normals(vnode);
					((VGeomData*)vnode->data)->post_vertex_set_xyz(vvert);
				}
			}
		}
		else {
			/* create new verse vert */

			/* test if we are authors of this vertex :-) */
			vvert = find_verse_vert_in_queue(vlayer, node_id, vertex_id, x, y, z);

			if(vvert) {
				/* remove vert from queue */
				BLI_remlink(&(vlayer->queue), (void*)vvert);
				/* add vvert to the dynamic list */
				BLI_dlist_add_item_index(&(vlayer->dl), (void*)vvert, (unsigned int)vertex_id);
				/* set VerseVert flags */
				vvert->flag |= VERT_RECEIVED;
				if(!(vvert->flag & VERT_POS_OBSOLETE))
					vvert->flag &= ~VERT_LOCKED;
				/* find VerseFaces orphans */
				find_vlayer_orphans(vnode, vvert);
				/* find unsent VerseFaces */
				find_unsent_faces(vnode, vvert);
			}
			else {
				/* create new VerseVert */
				vvert = create_verse_vertex(vlayer, vertex_id, x, y, z);
				/* add VerseVert to list of VerseVerts */
				BLI_dlist_add_item_index(&(vlayer->dl), (void*)vvert, (unsigned int)vertex_id);
				/* set VerseVert flags */
				vvert->flag |= VERT_RECEIVED;
				/* find VerseFaces orphans */
				find_vlayer_orphans(vnode, vvert);
			}

			((VGeomData*)vnode->data)->post_vertex_create(vvert);
		}
	}
}

/*
 * callback function for destroyng of verse layer
 */
static void cb_g_layer_destroy(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VLayer *vlayer;

	if(!session) return;

	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), node_id);
	if(!vnode) return;

	vlayer = (VLayer*) BLI_dlist_find_link(&(((VGeomData*)vnode->data)->layers), layer_id);

	if(vlayer){
		/* free VerseLayer data */
		free_verse_layer_data(vnode, vlayer);
		/* remove VerseLayer from list of verse layers */
		BLI_dlist_rem_item(&(((VGeomData*)vnode->data)->layers), layer_id);
		/* do client dependent actions */
		vlayer->post_layer_destroy(vlayer);
		/* free vlayer itself */
		MEM_freeN(vlayer);
	}

}

/*
 * callback function: new layer was created
 */
static void cb_g_layer_create(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id,
		const char *name,
		VNGLayerType type,
		uint32 def_integer,
		real64 def_real)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode=NULL;
	struct VLayer *vlayer=NULL;

	if(!session) return;

	/* find node of this layer*/
	vnode = BLI_dlist_find_link(&(session->nodes), node_id);
	if(!vnode) return;

	/* when we created this layer, then subscribe to this layer */
	if(vnode->owner_id == VN_OWNER_MINE || session->flag & VERSE_AUTOSUBSCRIBE)
		verse_send_g_layer_subscribe(node_id, layer_id, 0);

	/* try to find */
	if(vnode->owner_id == VN_OWNER_MINE)
		vlayer = find_vlayer_in_sending_queue(vnode, layer_id);

	if(vlayer) {
		/* remove vlayer form sending queue add verse layer to list of verse layers */
		BLI_remlink(&((VGeomData*)vnode->data)->queue, vlayer);
		BLI_dlist_add_item_index(&((VGeomData*)vnode->data)->layers, (void*)vlayer, (unsigned int)vlayer->id);
		/* send all not sent vertexes to verse server
		 * other items waiting in sending queue will be automaticaly sent to verse server,
		 * when verse vertexes will be received from verse server */
		if((vlayer->type == VN_G_LAYER_VERTEX_XYZ) && (layer_id==0)) {
			struct VerseVert *vvert = (VerseVert*)vlayer->queue.first;
			while(vvert) {
				send_verse_vertex(vvert);
				vvert = vvert->next;
			}
		}
	}
	else {
		/* create new VerseLayer */
		vlayer = create_verse_layer(vnode, layer_id, name, type, def_integer, def_real);
		/* add layer to the list of VerseLayers */
		BLI_dlist_add_item_index(&(((VGeomData*)vnode->data)->layers), (void*)vlayer, (unsigned int)layer_id);
	}

	vlayer->flag |= LAYER_RECEIVED;

	/* post callback function */
	vlayer->post_layer_create(vlayer);
}

/*
 * this function will send destroy commands for all VerseVertexes and
 * VerseFaces to verse server, but it will not send destroy commands
 * for VerseLayers or geometry node, it can be used in other functions
 * (undo, destroy geom node, some edit mesh commands, ... ), parameter of
 * this function has to be geometry verse node
 */
void destroy_geometry(VNode *vnode)
{
	struct VLayer *vert_vlayer, *face_vlayer;
	struct VerseFace *vface;
	struct VerseVert *vvert;

	if(vnode->type != V_NT_GEOMETRY) return;

	face_vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);
	vface = face_vlayer->dl.lb.first;

	while(vface) {
		send_verse_face_delete(vface);
		vface = vface->next;
	}

	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	vvert = vert_vlayer->dl.lb.first;

	while(vvert) {
		send_verse_vertex_delete(vvert);
		vvert = vvert->next;
	}

	/* own destruction of local verse date will be executed, when client will
	 * receive apropriate callback commands from verse server */
}

/*
 * free VGeomData
 */
void free_geom_data(VNode *vnode)
{
	struct VerseSession *session = vnode->session;
	struct VLayer *vlayer;

	if(vnode->data){
		vlayer = (VLayer*)((VGeomData*)vnode->data)->layers.lb.first;
		while(vlayer){
			/* unsubscribe from layer */
			if(session->flag & VERSE_CONNECTED)
				verse_send_g_layer_unsubscribe(vnode->id, vlayer->id);
			/* free VerseLayer data */
			free_verse_layer_data(vnode, vlayer);
			/* next layer */
			vlayer = vlayer->next;
		}
		/* free constraint between vnode and mesh */
		((VGeomData*)vnode->data)->post_geometry_free_constraint(vnode);
		/* free all VerseLayers */
		BLI_dlist_destroy(&(((VGeomData*)vnode->data)->layers));
		/* free fake verse edges */
		BLI_freelistN(&((VGeomData*)vnode->data)->edges);
		/* free edge hash */
		MEM_freeN(((VGeomData*)vnode->data)->hash);
	}
}

void set_geometry_callbacks(void)
{
	/* new layer created */
	verse_callback_set(verse_send_g_layer_create, cb_g_layer_create, NULL);
	/* layer was destroyed */
	verse_callback_set(verse_send_g_layer_destroy, cb_g_layer_destroy, NULL);

	/* position of vertex was changed */
	verse_callback_set(verse_send_g_vertex_set_xyz_real32, cb_g_vertex_set_xyz_real32, NULL);
	/* vertex was deleted */
	verse_callback_set(verse_send_g_vertex_delete_real32, cb_g_vertex_delete_real32, NULL);

	/* callback functions for values being associated with vertexes */
	verse_callback_set(verse_send_g_vertex_set_uint32, cb_g_vertex_set_uint32, NULL);
	verse_callback_set(verse_send_g_vertex_set_real32, cb_g_vertex_set_real32, NULL);

	/* new polygon was created / vertex(es) of polygon was set */
	verse_callback_set(verse_send_g_polygon_set_corner_uint32, cb_g_polygon_set_corner_uint32, NULL);
	/* polygon was deleted */
	verse_callback_set(verse_send_g_polygon_delete, cb_g_polygon_delete, NULL);

	/* callback functions for values being associated with polygon corners */
	verse_callback_set(verse_send_g_polygon_set_corner_real32, cb_g_polygon_set_corner_real32, NULL);
	/* callback functions for values being associated with faces */
	verse_callback_set(verse_send_g_polygon_set_face_uint8, cb_g_polygon_set_face_uint8, NULL);
	verse_callback_set(verse_send_g_polygon_set_face_uint32, cb_g_polygon_set_face_uint32, NULL);
	verse_callback_set(verse_send_g_polygon_set_face_real32, cb_g_polygon_set_face_real32, NULL);

	/* crease of vertex was set */
	verse_callback_set(verse_send_g_crease_set_vertex, cb_g_crease_set_vertex, NULL);
	/* crease of edge was set */
	verse_callback_set(verse_send_g_crease_set_edge, cb_g_crease_set_edge, NULL);
}

#endif
