/**
 * $Id$
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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "mydevice.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_dynamiclist.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"
#include "BKE_verse.h"

#include "BIF_verse.h"
#include "BIF_editmesh.h"
#include "BIF_space.h"
#include "BIF_screen.h"

#include "BSE_edit.h"

#include "editmesh.h"

#include "verse.h"

/* prototypes of static functions */

static void mark_changed_face_obsolete(struct VerseFace *vface);
static void sync_verseface_with_editface(struct VLayer *vlayer, struct VerseFace *vface);

void createVerseVertNL(struct EditVert *ev, struct VNode *vnode, struct VLayer *vlayer);
static void createAllVerseVerts(struct VNode *vnode, struct VLayer *vlayer);
void createVerseFaceNL(struct EditFace *efa, struct VNode *vnode, struct VLayer *vlayer);
static void createAllVerseFaces(struct VNode *vnode, struct VLayer *vlayer);

/*=============================================================================
 *
 *                  functions handling verse/blender FACES
 *
 *===========================================================================*/

/*
 * create new VerseFace (polygon), due to information about EditFace
 * put VerseFace to queue ... send to verse host (server)
 */
void createVerseFace(EditFace *efa)
{
	if(G.editMesh->vnode) {
		struct VLayer *vlayer = find_verse_layer_type((VGeomData*)((VNode*)G.editMesh->vnode)->data, POLYGON_LAYER);
		createVerseFaceNL(efa, (VNode*)G.editMesh->vnode, vlayer);
	}
	else efa->vface = NULL;
}

/*
 * create new VerseFace (polygon), due to information about EditFace
 * put VerseFace to queue ... send to verse host (server)
 * NL version of function (infomration about verse node and
 * layer is known ... optimalisation)
 */
void createVerseFaceNL(EditFace *efa, VNode *vnode, VLayer *vlayer)
{
	struct VerseFace *vface;

	vface = (VerseFace*)create_verse_face(vlayer, vlayer->counter, -1, -1, -1, -1);
	
	vface->face = (void*)efa;
	efa->vface = (void*)vface;

	vface->flag |= FACE_SEND_READY;

	/* EditVert #1 */
	if(efa->v1) {
		if(efa->v1->vvert) {
			vface->vvert0 = (VerseVert*)efa->v1->vvert;
			if(((VerseVert*)efa->v1->vvert)->flag & VERT_RECEIVED) {
				vface->v0 = ((VerseVert*)efa->v1->vvert)->id;
				vface->counter--;
			}
			else
				vface->flag &= ~FACE_SEND_READY;
		}
	}
	else
		vface->counter--;

	/* EditVert #2 */
	if(efa->v2) {
		if(efa->v2->vvert) {
			vface->vvert1 = (VerseVert*)efa->v2->vvert;
			if(((VerseVert*)efa->v2->vvert)->flag & VERT_RECEIVED) {
				vface->v1 = ((VerseVert*)efa->v2->vvert)->id;
				vface->counter--;
			}
			else
				vface->flag &= ~FACE_SEND_READY;
		}
	}
	else
		vface->counter--;
	/* EditVert #3 */
	if(efa->v3) {
		if(efa->v3->vvert) {
			vface->vvert2 = (VerseVert*)efa->v3->vvert;
			if(((VerseVert*)efa->v3->vvert)->flag & VERT_RECEIVED) {
				vface->v2 = ((VerseVert*)efa->v3->vvert)->id;
				vface->counter--;
			}
			else
				vface->flag &= ~FACE_SEND_READY;
		}
	}
	else
		vface->counter--;
	/* EditVert #4 */
	if(efa->v4) {
		if(efa->v4->vvert) {
			vface->vvert3 = (VerseVert*)efa->v4->vvert;
			if(((VerseVert*)efa->v4->vvert)->flag & VERT_RECEIVED) {
				vface->v3 = ((VerseVert*)efa->v4->vvert)->id;
				vface->counter--;
			}
			else
				vface->flag &= ~FACE_SEND_READY;
		}
	}
	else
		vface->counter--;

	add_item_to_send_queue(&(vlayer->queue), (void*)vface, VERSE_FACE);
}

/*
 * creates new verse faces, add all of then to queue ... send to verse server
 */
static void createAllVerseFaces(VNode *vnode, VLayer *vlayer)
{
	if(G.obedit) {
		struct EditMesh *em;
		struct EditFace *efa;

		em = G.editMesh;

		efa = em->faces.first;
		/* push all EditFaces to the verse server */
		while(efa){
			createVerseFaceNL(efa, vnode, vlayer);
			efa = efa->next;
		}
	}
}

/*
 * When verse face is changed during sending/receiving from verse server, then
 * this verse face is marked as obsolete and it will be send again, when it
 * will be received from verse server 
 */
static void mark_changed_face_obsolete(VerseFace *vface)
{
	struct EditFace *efa = (EditFace*)vface->face;

	if(efa) {
		if(vface->vvert0->vertex != efa->v1) vface->flag |= FACE_OBSOLETE;
		if(vface->vvert1->vertex != efa->v2) vface->flag |= FACE_OBSOLETE;
		if(vface->vvert2->vertex != efa->v3) vface->flag |= FACE_OBSOLETE;
		if(vface->vvert3 && (vface->vvert3->vertex != efa->v4)) vface->flag |= FACE_OBSOLETE;
	}
}

/*
 * this function will sync EditFace and VerseFace and it will send changes to
 * verse server too
 */
static void sync_verseface_with_editface(VLayer *vlayer, VerseFace *vface)
{
	struct EditFace *efa = (EditFace*)vface->face;
	int dosend = 0;

	/* edit face and probably verse face was deleted */
	if(!efa || (vface->flag & FACE_DELETED)) return;

	/* blender nor verse don't support such crazy things */
	if(!(vface->vvert0) || !(vface->vvert1) || !(vface->vvert2)) {
		printf("\tERROR: vface->vvert0: %p, vface->vvert1: %p, vface->vvert2: %p\n", vface->vvert0, vface->vvert1, vface->vvert2);
		return;
	}

	/* initialize verse face flag */
	vface->flag |= FACE_SEND_READY;

	/* debug print */
#if 0
	printf("\n");
	if(efa->v4) {
		printf("\tefa->v1,v2,v3,v4->vvert->id: %d, %d, %d, %d\n",
				((VerseVert*)efa->v1->vvert)->id,
				((VerseVert*)efa->v2->vvert)->id,
				((VerseVert*)efa->v3->vvert)->id,
				((VerseVert*)efa->v4->vvert)->id);
		printf("\tefa->v1,v2,v3,v4->vvert: %p, %p, %p, %p\n",
				efa->v1->vvert,
				efa->v2->vvert,
				efa->v3->vvert,
				efa->v4->vvert);
	}
	else {
		printf("\tefa->v1,v2,v3->vvert->id: %d, %d, %d, NULL\n",
				((VerseVert*)efa->v1->vvert)->id,
				((VerseVert*)efa->v2->vvert)->id,
				((VerseVert*)efa->v3->vvert)->id);
		printf("\tefa->v1,v2,v3,v4->vvert: %p, %p, %p, NULL\n",
				efa->v1->vvert,
				efa->v2->vvert,
				efa->v3->vvert);
	}
	printf("\tvface->vvert0, vvvert1, vvvert2, vvvert3: %p, %p, %p, %p\n",
			vface->vvert0,
			vface->vvert1,
			vface->vvert2,
			vface->vvert3);

	printf("\tefa->v1, v2, v3, v4: %p, %p, %p, %p\n",
			efa->v1,
			efa->v2,
			efa->v3,
			efa->v4);
	if(vface->vvert3) {
		printf("\tvface->v0,v1,v2,v3: %d, %d, %d, %d\n",
				vface->v0,
				vface->v1,
				vface->v2,
				vface->v3);
		printf("\tvface->vvert0,vvvert1,vvvert2,vvvert3->vertex: %p, %p, %p, %p\n",
				vface->vvert0->vertex,
				vface->vvert1->vertex,
				vface->vvert2->vertex,
				vface->vvert3->vertex);
	}
	else {
		printf("\tvface->v0,v1,v2,v3: %d, %d, %d, NULL\n",
				vface->v0,
				vface->v1,
				vface->v2);
		printf("\tvface->vvert0,vvvert1,vvvert2->vertex: %p, %p, %p, NULL\n",
				vface->vvert0->vertex,
				vface->vvert1->vertex,
				vface->vvert2->vertex);
	}
#endif

	/* initialize counter of unreceived vertexes */
	vface->counter = 4;

	/* 1st vertex */
	if(vface->vvert0->vertex != efa->v1) {
		dosend = 1;
		vface->vvert0->counter--;
		vface->vvert0 = (VerseVert*)efa->v1->vvert;
		vface->v0 = vface->vvert0->id;
		if(vface->vvert0->flag & VERT_RECEIVED)
			vface->counter--;
		else
			vface->flag &= ~FACE_SEND_READY;
	}
	else
		vface->counter--;

	/* 2nd vertex */
	if(vface->vvert1->vertex != efa->v2) {
		dosend = 1;
		vface->vvert1->counter--;
		vface->vvert1 = (VerseVert*)efa->v2->vvert;
		vface->v1 = vface->vvert1->id;
		if(vface->vvert1->flag & VERT_RECEIVED)
			vface->counter--;
		else
			vface->flag &= ~FACE_SEND_READY;
	}
	else
		vface->counter--;

	/* 3th vertex */
	if(vface->vvert2->vertex != efa->v3) {
		dosend = 1;
		vface->vvert2->counter--;
		vface->vvert2 = (VerseVert*)efa->v3->vvert;
		vface->v2 = vface->vvert2->id;
		if(vface->vvert2->flag & VERT_RECEIVED)
			vface->counter--;
		else
			vface->flag &= ~FACE_SEND_READY;
	}
	else
		vface->counter--;

	/* 4th vertex */
	if(vface->vvert3 && ((vface->vvert3->vertex != efa->v4) || (vface->vvert3 && !efa->v4) || (vface->v3 != vface->vvert3->id))) {
		dosend = 1;
		if(efa->v4) {
			vface->vvert3->counter--;
			vface->vvert3 = (VerseVert*)efa->v4->vvert;
			vface->v3 = vface->vvert3->id;
			if(vface->vvert3->flag & VERT_RECEIVED)
				vface->counter--;
			else
				vface->flag &= ~FACE_SEND_READY;
		}
		else {
			vface->vvert3->counter--;
			vface->vvert3 = NULL;
			vface->v3 = -1;
			vface->counter--;
		}
	}
	/* verse face has 4 vertexes now, not 3 vertexes as in past */
	else if(!(vface->vvert3) && efa->v4) {
		dosend = 1;
		vface->vvert3 = (VerseVert*)efa->v4->vvert;
		vface->v3 = vface->vvert3->id;
		if(vface->vvert3->flag & VERT_RECEIVED)
			vface->counter--;
		else
			vface->flag &= ~FACE_SEND_READY;
	}
	else
		vface->counter--;

	if(dosend) {
		/* printf("\tsending CHANGED FACE\n");
		printf("\t\tnew: %d %d %d %d\n", vface->v0, vface->v1, vface->v2, vface->v3);*/
		vface->flag |= FACE_CHANGED;
		/* remove verse face from list of received faces */
		BLI_dlist_rem_item(&(vlayer->dl), vface->id);
		/* and add verse face again to sending queue */
		add_item_to_send_queue(&(vlayer->queue), (void*)vface, VERSE_FACE);
	}
}

/*
 * this function will sync all VerseFaces with coresponding EditFaces,
 * this is useful, when some editmesh tool has changed editface pointers at
 * vertexes (edges), parameter of this function is geometry node
 */
void sync_all_versefaces_with_editfaces(VNode *vnode)
{
	struct VLayer *vlayer;
	struct VerseFace *vface, *nvface;

	if(vnode->type != V_NT_GEOMETRY) return;

	vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	/* mark changed verse faces in sending queue as obsolete at the first time */
	vface = vlayer->queue.first;
	while(vface) {
		mark_changed_face_obsolete(vface);
		vface = vface->next;
	}
	
	/* send all received and changed verse face again to verse server */
	vface = vlayer->dl.lb.first;
	while(vface) {
		nvface = vface->next;
		sync_verseface_with_editface(vlayer, vface);
		vface = nvface;
	}
}

/*
 * send delete polygon command to verse server
 */
void b_verse_send_face_delete(EditFace *efa)
{
	((VerseFace*)efa->vface)->face = NULL;
	send_verse_face_delete((VerseFace*)efa->vface);
	efa->vface = NULL;
}

/*=============================================================================
 *
 *                   functions handling verse/blender VERTEXES
 *
 *===========================================================================*/

/*
 * this function will sync position of all VerseVerts with EditVerts
 * this function is called after actions: Smooth, Noise and To Sphere,
 * because position of vertexes isn't managed by transform system
 */
void sync_all_verseverts_with_editverts(VNode *vnode)
{
	struct VLayer *vlayer;
	struct VerseVert *vvert;

	if(vnode->type != V_NT_GEOMETRY) return;

	vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);

	vvert = vlayer->dl.lb.first;

	/* sync all received vertexes */
	while(vvert) {
		send_versevert_pos(vvert);
		vvert = vvert->next;
	}

	vvert = vlayer->queue.first;
	
	/* sync all unreceived vertexes (mar pos as obsolete, when
	 * actual position was changed) */
	while(vvert) {
		send_versevert_pos(vvert);
		vvert = vvert->next;
	}

	verse_callback_update(0);
}

/*
 * send delete vertex command to verse server
 */
void b_verse_send_vertex_delete(EditVert *eve)
{
	((VerseVert*)eve->vvert)->vertex = NULL;
	send_verse_vertex_delete((VerseVert*)eve->vvert);
	eve->vvert = NULL;
}

/*
 * send position of verse vertex to verse server
 */
void send_versevert_pos(VerseVert *vvert)
{
	/* delete command was sent to verse server ... sending one
	 * more position command would create new vertex */
	if ((vvert->flag & VERT_DELETED) | (vvert->flag & VERT_OBSOLETE)) return;
	
	/* don't send position of verse vertex to verse server, because it could create
	 * new vertex */
	if(vvert->flag & VERT_RECEIVED) {
	       	if(vvert->flag & VERT_LOCKED) {
			/* when position of verse vert was sent to verse server
			 * and it wasn't received yet, then mark sent position
			 * as obsolete ... blender will automaticaly send actual
			 * position, when old will be received */
			vvert->flag |= VERT_POS_OBSOLETE;
		}
		else {
			struct EditVert *eve = (EditVert*)vvert->vertex;
			/* send position to verse server, when it is different from actual position */
			if(eve && (eve->co[0]!=vvert->co[0] || eve->co[1]!=vvert->co[1] || eve->co[2]!=vvert->co[2])) {
				/* lock vertex and send its position to verse server,
				 * locking of vertex prevents from sending too many
				 * informations about vertex position during draging
				 * of vertex */
				vvert->flag |= VERT_LOCKED;
				VECCOPY(vvert->co, eve->co);
				send_verse_vertex(vvert);
			}
		}
	}
	else {
		/* we created this vertex and we sent new position to verse server, but "confirmation" command about
		 * position of vertex didn't arrived yet, then we can't send new position of vertex ... we only mark
		 * position of vertex as obsolete and new position will be sent to verse server, when confirmation
		 * command will arive */
		struct EditVert *eve = (EditVert*)vvert->vertex;
		if(eve && (eve->co[0]!=vvert->co[0] || eve->co[1]!=vvert->co[1] || eve->co[2]!=vvert->co[2])) {
			vvert->flag |= VERT_POS_OBSOLETE;
		}
	}
	
	verse_callback_update(0);
}

/*
 * create new VerseVert due to information about EditVert,
 * put VerseVert to queue ... send to verse host (server)
 */
void createVerseVert(EditVert *eve)
{
	if(G.editMesh->vnode) {
		struct VLayer *vlayer = find_verse_layer_type((VGeomData*)((VNode*)G.editMesh->vnode)->data, VERTEX_LAYER);
		createVerseVertNL(eve, (VNode*)G.editMesh->vnode, vlayer);
	}
	else eve->vvert = NULL;
}

/*
 * create new VerseVert due to information about EditVert,
 * put VerseVert to queue ... send to verse host (server)
 * NL version of function (infomration about verse node and
 * layer is known ... optimalisation)
 */
void createVerseVertNL(EditVert *eve, VNode *vnode, VLayer *vlayer)
{
	struct VerseVert *vvert;

	vvert = create_verse_vertex(vlayer, vlayer->counter, eve->co[0], eve->co[1], eve->co[2]);

	vvert->vertex = (void*)eve;
	eve->vvert = (void*)vvert;

	vvert->flag |= VERT_LOCKED;

/*	printf("\tsend_versevert_pos: %d create and LOCK \n", vvert->id);*/

	/* add vvert to sending queue */
	add_item_to_send_queue(&(vlayer->queue), (void*)vvert, VERSE_VERT);
}

/*
 * create new verse vertexes due to all vertexes and send all of them to verse server 
 */
static void createAllVerseVerts(VNode *vnode, VLayer *vlayer)
{
	if(G.obedit) {
		struct EditMesh *em;
		struct EditVert *eve;

		em = G.editMesh;
		eve = em->verts.first;

		/* push all EditVertexes to the verse server */
		while(eve){
			createVerseVertNL(eve, vnode, vlayer);
			eve = eve->next;
		}
	}
}

/*
 * unsubscribe from verse geometry layers of verse geometry node
 * and clear bindings between verse node and mesh
 */
void unsubscribe_from_geom_node(VNode *vnode)
{
	struct VerseSession *session = vnode->session;
	
	struct VLayer *vlayer = ((VGeomData*)vnode->data)->layers.lb.first;
	
	if(vnode->type != V_NT_GEOMETRY) return;
	
	/* free bindings between verse node and blender mesh*/	
	if(((VGeomData*)vnode->data)->mesh) {
		((Mesh*)((VGeomData*)vnode->data)->mesh)->vnode = NULL;
		((VGeomData*)vnode->data)->mesh = NULL;
	}

	/* free bindings between verse node and blender editmesh */
	if(((VGeomData*)vnode->data)->editmesh) {
		((EditMesh*)((VGeomData*)vnode->data)->editmesh)->vnode = NULL;
		((VGeomData*)vnode->data)->editmesh = NULL;
	}
	
	/* free all verse layer data and unsubscribe from all layers */
	while(vlayer) {
		BLI_dlist_reinit(&(vlayer->dl));
		BLI_freelistN(&(vlayer->queue));
		BLI_freelistN(&(vlayer->orphans));

		if(session->flag & VERSE_CONNECTED)
			verse_send_g_layer_unsubscribe(vnode->id, vlayer->id);

		vlayer = vlayer->next;
	}

}

/* ===================================================================================
 *
 *             Function executed after execution of callback functions
 *
 * ===================================================================================*/

/*
 * Actions executed after new VerseLayer is created
 */
void post_layer_create(struct VLayer *vlayer)
{
	/* if we are owners of VerseNode, then push geometry to verse server */
	if(vlayer->vnode->owner_id == VN_OWNER_MINE) {
		switch(vlayer->type){
			case VN_G_LAYER_VERTEX_XYZ:
/*				if(vlayer->id==0) createAllVerseVerts(vlayer->vnode, vlayer);
				break;*/
			case VN_G_LAYER_POLYGON_CORNER_UINT32:
/*				if(vlayer->id==1) createAllVerseFaces(vlayer->vnode, vlayer);
				break;*/
			case VN_G_LAYER_VERTEX_UINT32:
			case VN_G_LAYER_VERTEX_REAL:
			case VN_G_LAYER_POLYGON_CORNER_REAL:
			case VN_G_LAYER_POLYGON_FACE_UINT8:
			case VN_G_LAYER_POLYGON_FACE_UINT32:
			case VN_G_LAYER_POLYGON_FACE_REAL:
				break;
		}
	}
	else {
		switch(vlayer->type) {
			case VN_G_LAYER_VERTEX_XYZ:
			case VN_G_LAYER_POLYGON_CORNER_UINT32:
			case VN_G_LAYER_VERTEX_UINT32:
			case VN_G_LAYER_VERTEX_REAL:
			case VN_G_LAYER_POLYGON_CORNER_REAL:
			case VN_G_LAYER_POLYGON_FACE_UINT8:
			case VN_G_LAYER_POLYGON_FACE_UINT32:
			case VN_G_LAYER_POLYGON_FACE_REAL:
				break;
		}
	}
}

/*
 * Actions after destroying of VerseLayer
 */
void post_layer_destroy(struct VLayer *vlayer)
{
}

/*
 * Actions executed after creating of new VerseVert, when object is in edit
 * mode, and this client didn't create this VerseVert (vvert->vertex is NULL),
 * then new editvert is created
 */
void post_vertex_create(VerseVert *vvert)
{
	struct VNode *obj_vnode;
	struct VNode *geom_vnode = vvert->vlayer->vnode;
	struct EditMesh *em=NULL;

	if(G.obedit && (((Mesh*)G.obedit->data)->vnode==geom_vnode)) {
		em = (EditMesh*)((VGeomData*)geom_vnode->data)->editmesh;
	}

	/* when vert was changed during sending to verse server, then
	 * we have to send it to verse server again */
	if(vvert->flag & VERT_POS_OBSOLETE) {
		vvert->flag &= ~VERT_POS_OBSOLETE;
		
		if(em && (vvert->vertex)) {
			struct EditVert *eve = (EditVert*)vvert->vertex;
			VECCOPY(vvert->co, eve->co);
			send_verse_vertex(vvert);
			verse_callback_update(0);

			return;
		}
	}
	
	if(em && !(vvert->vertex)) {
		struct EditVert *eve;

		/* to prevent never ending loop of sending and receiving
		 * vertexes, because addvertlist() sends new vertex to verse
		 * server if em->vnode isn't NULL */
		em->vnode = NULL;
		eve = addvertlist(vvert->co, NULL);
		em->vnode = (void*)geom_vnode;

		eve->vvert = (void*)vvert;
		vvert->vertex = (void*)eve;
	
		countall();

		recalc_editnormals();
	}

	if(((VGeomData*)geom_vnode->data)->vlink) {
		obj_vnode = ((VGeomData*)geom_vnode->data)->vlink->source;
		DAG_object_flush_update(G.scene, (Object*)((VObjectData*)obj_vnode->data)->object, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 1);
	}
}

/*
 * Actions executed, when position of VerseVert was changed
 * position of EditVert is changed in edit mode
 */
void post_vertex_set_xyz(VerseVert *vvert)
{
	struct VNode *obj_vnode;
	struct VNode *geom_vnode = vvert->vlayer->vnode;
	struct EditVert *eve = NULL;

	/* when vert was changed during sending to verse server, then
	 * we have to send it to verse server again */
	if(vvert->flag & VERT_POS_OBSOLETE) {
		if(vvert->vertex) {
			vvert->flag &= ~VERT_POS_OBSOLETE;
			vvert->flag |= VERT_LOCKED;

			eve = (EditVert*)vvert->vertex;
			VECCOPY(vvert->co, eve->co);
			send_verse_vertex(vvert);
			verse_callback_update(0);
		}
		else {
			printf("\terror: vvert->vertex shouldn't be NULL\n");
		}

		return;
	}
	
	/* when shared object is in edit mode, then update editmesh */	
	if(G.obedit && (((Mesh*)G.obedit->data)->vnode==geom_vnode)) {
		if(vvert->vertex) {
			eve = (EditVert*)vvert->vertex;
			VECCOPY(eve->co, vvert->co);
			recalc_editnormals();
		}
		else {
			printf("\terror: vvert->vertex shouldn't be NULL\n");
		}
	}

	if(((VGeomData*)geom_vnode->data)->vlink) {
		obj_vnode = ((VGeomData*)geom_vnode->data)->vlink->source;
		DAG_object_flush_update(G.scene, (Object*)((VObjectData*)obj_vnode->data)->object, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 1);
	}
}

/*
 * Actions executed after deleting of VerseVert
 */
void post_vertex_delete(VerseVert *vvert)
{
	struct VNode *obj_vnode;
	struct VNode *geom_vnode = vvert->vlayer->vnode;
	struct EditMesh *em = NULL;
	struct EditEdge *ed, *edn;
	struct EditVert *eve = NULL;

	if(G.obedit && (((Mesh*)G.obedit->data)->vnode==geom_vnode)) {
		em = (EditMesh*)((VGeomData*)geom_vnode->data)->editmesh;
		eve = (EditVert*)vvert->vertex;
	}

	if(em && eve) {
		/*printf("\tPOST_VERTEX_DELETE()\n");*/

		/* delete all edges needing eve vertex */
		ed = em->edges.first;
		while(ed) {
			edn = ed->next;
			if(ed->v1==eve || ed->v2==eve) {
				remedge(ed);
				free_editedge(ed);
			}
			ed = edn;
		}

		eve->vvert = NULL;
		BLI_remlink(&em->verts, eve);
		free_editvert(eve);
		vvert->vertex = NULL;
	
		countall();

		recalc_editnormals();
	}
	
	if(((VGeomData*)geom_vnode->data)->vlink) {
		obj_vnode = ((VGeomData*)geom_vnode->data)->vlink->source;
		DAG_object_flush_update(G.scene, (Object*)((VObjectData*)obj_vnode->data)->object, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 1);
	}
}

/*
 * free constraint between VerseVert and EditVert
 */
void post_vertex_free_constraint(VerseVert *vvert)
{
	if(vvert->vertex) {
		((EditVert*)vvert->vertex)->vvert=NULL;
		vvert->vertex=NULL;
	}
}

/*
 * Action executed after setting up uint8 value of polygon
 */
void post_polygon_set_uint8(VerseFace *vface)
{
}

/*
 * Action executed after creating of new VerseFace
 */
void post_polygon_create(VerseFace *vface)
{
	struct VNode *obj_vnode;
	struct VNode *geom_vnode = vface->vlayer->vnode;
	struct EditMesh *em = NULL;

	/* if verse face was set as deleted during sending to verse server, then send
	 * delete command to verse server now ... we know verse face id */
/*	if(vface->flag & FACE_DELETED) {
		send_verse_face_delete(vface);
		return;
	}*/
	
	if(G.obedit && (((Mesh*)G.obedit->data)->vnode==geom_vnode)) {
		em = (EditMesh*)((VGeomData*)geom_vnode->data)->editmesh;
	}

	/* when face was changed during sending to verse server, then
	 * we have to send it to verse server again */
	if(vface->flag & FACE_OBSOLETE) {
		vface->flag &= ~FACE_OBSOLETE;
		sync_verseface_with_editface(vface->vlayer, vface);
		return;
	}

	if(em && !(vface->face) && (vface->counter==0)) {
		struct VLayer *vlayer;
		struct VerseVert *vvert;
		struct EditFace *efa;
		struct EditVert *eves[4]={NULL, NULL, NULL, NULL};
		uint32 vert_ids[4]={vface->v0, vface->v1, vface->v2, vface->v3};
		int i;

		/*printf("\tPOST_POLYGON_CREATE()\n");*/

		vlayer = find_verse_layer_type((VGeomData*)geom_vnode->data, VERTEX_LAYER);

		for(i=0; i<4; i++) {
			if(vert_ids[i] != -1) {
				vvert = BLI_dlist_find_link(&(vlayer->dl), vert_ids[i]);
				if(vvert) eves[i] = (EditVert*)vvert->vertex;
			}
		}

		/* to prevent never ending loop of sending and receiving
		 * faces, because addfacelist() sends new face to verse
		 * server if em->vnode isn't NULL */
		em->vnode = NULL;
		efa = addfacelist(eves[0], eves[1], eves[2], eves[3], NULL, NULL);
		em->vnode = geom_vnode;

		if(efa) {
			efa->vface = vface;
			vface->face = efa;
		}
	
		countall();

		recalc_editnormals();
	}

	if(((VGeomData*)geom_vnode->data)->vlink) {
		obj_vnode = ((VGeomData*)geom_vnode->data)->vlink->source;
		DAG_object_flush_update(G.scene, (Object*)((VObjectData*)obj_vnode->data)->object, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 1);
	}
}

/*
 * Action executed after changes of VerseFace
 * ... order of vertexes was fliped, etc.
 */
void post_polygon_set_corner(VerseFace *vface)
{
	struct VNode *obj_vnode;
	struct VNode *geom_vnode = vface->vlayer->vnode;
	struct EditMesh *em = NULL;
	struct EditFace *efa = NULL;
	struct EditEdge *eed, *eedn;

	if(G.obedit && (((Mesh*)G.obedit->data)->vnode==geom_vnode)) {
		em = (EditMesh*)((VGeomData*)geom_vnode->data)->editmesh;
		efa = (EditFace*)vface->face;
	}

	if(em && efa) {

		/* when face was changed during sending to verse server, then
		 * we have to send it to verse server again */
		if(vface->flag & FACE_OBSOLETE) {
			vface->flag &= ~FACE_OBSOLETE;
			sync_verseface_with_editface(vface->vlayer, vface);
			return;
		}

		/* mark all edges, which are part of face efa */
		efa->e1->f2 = 1;
		efa->e2->f2 = 1;
		efa->e3->f2 = 1;
		if(efa->e4) efa->e4->f2 = 1;

		/* change pointers at EdtitVerts and decrease counters of "old"
		 * VerseVertexes reference ... less VerseFaces will need them */
		if(vface->vvert0 != efa->v1->vvert)
			efa->v1 = (EditVert*)vface->vvert0->vertex;
		if(vface->vvert1 != efa->v2->vvert)
			efa->v2 = (EditVert*)vface->vvert1->vertex;
		if(vface->vvert2 != efa->v3->vvert)
			efa->v3 = (EditVert*)vface->vvert2->vertex;
		if(efa->v4) {
			if(!vface->vvert3)
				efa->v4 = NULL;
			else if(vface->vvert3 != efa->v4->vvert)
				efa->v4 = (EditVert*)vface->vvert3->vertex;
		}

		/* change pointers at EditEdges */

		/* 1st edge */
		eed = findedgelist(efa->v1, efa->v2);
		if(eed) efa->e1 = eed;
		else efa->e1 = addedgelist(efa->v1, efa->v2, NULL);

		/* 2nd edge */
		eed = findedgelist(efa->v2, efa->v3);
		if(eed) efa->e2 = eed;
		else efa->e2 = addedgelist(efa->v2, efa->v3, NULL);

		if(efa->v4) {
		/* 3th edge */
			eed = findedgelist(efa->v2, efa->v3);
			if(eed) efa->e3 = eed;
			else efa->e3 = addedgelist(efa->v2, efa->v3, NULL);
			/* 4th edge */
			eed = findedgelist(efa->v4, efa->v1);
			if(eed) efa->e4 = eed;
			else efa->e4 = addedgelist(efa->v4, efa->v1, NULL);
		}
		else {
		/* 3th edge */
			eed = findedgelist(efa->v3, efa->v1);
			if(eed) efa->e3 = eed;
			else efa->e3 = addedgelist(efa->v3, efa->v1, NULL);
			/* 4th edge */
			efa->e4 = NULL;
		}

		/* unmark needed edges */
		efa = em->faces.first;
		while(efa) {
			efa->e1->f2 = 0;
			efa->e2->f2 = 0;
			efa->e3->f2 = 0;
			if(efa->e4) efa->e4->f2 = 0;
			efa = efa->next;
		}

		/* delete all unneeded edges */
		eed = em->edges.first;
		while(eed) {
			eedn = eed->next;
			if(eed->f2) {
				remedge(eed);
				free_editedge(eed);
			}
			eed = eedn;
		}

		countall();

		recalc_editnormals();
	}

	if(((VGeomData*)geom_vnode->data)->vlink) {
		obj_vnode = ((VGeomData*)geom_vnode->data)->vlink->source;
		DAG_object_flush_update(G.scene, (Object*)((VObjectData*)obj_vnode->data)->object, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 1);
	}
}

/*
 * Action executed after deleting of VerseFace
 */
void post_polygon_delete(VerseFace *vface)
{
	struct VNode *obj_vnode;
	struct VNode *geom_vnode = vface->vlayer->vnode;
	struct EditMesh *em = NULL;
	struct EditFace *efa = NULL;
	struct EditEdge *eed, *eedn;

	if(G.obedit && (((Mesh*)G.obedit->data)->vnode==geom_vnode)) {
		em = (EditMesh*)((VGeomData*)geom_vnode->data)->editmesh;
		efa = (EditFace*)vface->face;
	}

	if(em && efa) {
		/*printf("\tPOST_POLYGON_DELETE()\n");*/

		/* mark all edges, which are part of face efa */
		efa->e1->f2 = 1;
		efa->e2->f2 = 1;
		efa->e3->f2 = 1;
		if(efa->e4) efa->e4->f2 = 1;

		efa->vface = NULL;
		BLI_remlink(&em->faces, efa);
		free_editface(efa);
		vface->face = NULL;

		/* following two crazy loops wouldn't be neccessary if verse spec
		 * would support edges */

		/* unmark needed edges */
		efa = em->faces.first;
		while(efa) {
			efa->e1->f2 = 0;
			efa->e2->f2 = 0;
			efa->e3->f2 = 0;
			if(efa->e4) efa->e4->f2 = 0;
			efa = efa->next;
		}

		/* delete all unneeded edges */
		eed = em->edges.first;
		while(eed) {
			eedn = eed->next;
			if(eed->f2) {
				remedge(eed);
				free_editedge(eed);
			}
			eed = eedn;
		}
	
		countall();
	}

	if(((VGeomData*)geom_vnode->data)->vlink) {
		obj_vnode = ((VGeomData*)geom_vnode->data)->vlink->source;
		DAG_object_flush_update(G.scene, (Object*)((VObjectData*)obj_vnode->data)->object, OB_RECALC_DATA);

		allqueue(REDRAWVIEW3D, 1);
	}
}

/*
 * free constraint between VerseFace and EditFace
 */
void post_polygon_free_constraint(VerseFace *vface)
{
	if(vface->face) {
		((EditFace*)vface->face)->vface = NULL;
		vface->face = NULL;
	}
}

/*
 * free constraint between VGeomData, EditMesh and Mesh
 */
void post_geometry_free_constraint(VNode *vnode)
{
	if(((VGeomData*)vnode->data)->editmesh) {
		G.editMesh->vnode = NULL;
		((VGeomData*)vnode->data)->editmesh = NULL;
	}
	if(((VGeomData*)vnode->data)->mesh) {
		((Mesh*)((VGeomData*)vnode->data)->mesh)->vnode = NULL;
		((VGeomData*)vnode->data)->mesh = NULL;
	}
}

/* =========================================================================
 *
 *              Functions influencing whole EditMesh or VerseMesh
 *
 * ========================================================================= */

/*
 * free all bindings between EditMesh and "verse mesh" ... it is called between
 * restorng editmesh from undo stack
 */
void destroy_versemesh(VNode *vnode)
{
	struct VLayer *vert_vlayer, *face_vlayer;
	struct VerseVert *vvert;
	struct VerseFace *vface;

	if(vnode->type != V_NT_GEOMETRY) return;

	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	face_vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);


	/* send delete command to all received verse faces */
	vface = face_vlayer->dl.lb.first;
	while(vface) {
		if(vface->face) ((EditFace*)vface->face)->vface = NULL;
		vface->face = NULL;
		send_verse_face_delete(vface);
		vface = vface->next;
	}
	/* send delete command to all verse faces waiting in orphan list */
	vface = face_vlayer->orphans.first;
	while(vface) {
		if(vface->face) ((EditFace*)vface->face)->vface = NULL;
		vface->face = NULL;
		send_verse_face_delete(vface);
		vface = vface->next;
	}
	/* mark all verse faces waiting in sending queue as deleted,
	 * send delete command, when this verse face was changed */
	vface = face_vlayer->queue.first;
	while(vface) {
		if(vface->face) ((EditFace*)vface->face)->vface = NULL;
		vface->face = NULL;
		if(vface->flag & FACE_CHANGED)
			send_verse_face_delete(vface);
		else {
			vface->flag |= FACE_DELETED;
		}
		vface = vface->next;
	}
	
	/* send delete command to all received verse vertexes */
	vvert = vert_vlayer->dl.lb.first;
	while(vvert) {
		if(vvert->vertex) ((EditVert*)vvert->vertex)->vvert = NULL;
		vvert->vertex = NULL;
		send_verse_vertex_delete(vvert);
		vvert = vvert->next;
	}
	/* mark all verse vertexes waiting in sending queue as deleted
	 * ... verse vertexes will be deleted, when received */
	vvert = vert_vlayer->queue.first;
	while(vvert) {
		if(vvert->vertex) ((EditVert*)vvert->vertex)->vvert = NULL;
		vvert->vertex = NULL;
		vvert = vvert->next;
	}
}

/*
 * duplicate geometry verse node, this can be handy, when you duplicate some
 * object or make object single user
 */
VNode *create_geom_vnode_from_geom_vnode(VNode *vnode)
{
	struct VNode *n_vnode;				/* new verse geometry node */
	struct VGeomData *geom_data;			/* new geometry data */
	struct VLayer *n_vert_vlayer, *n_face_vlayer;	/* new vertex and polygon layer */
       	struct VLayer *vert_vlayer, *face_vlayer;
	struct VerseVert *n_vvert, *vvert;
	struct VerseFace *n_vface, *vface;
	int i;

	if(!vnode) return NULL;

	if(vnode->type != V_NT_GEOMETRY) return NULL;
	
	/* create new verse node, when no one exist */
	n_vnode= create_verse_node(vnode->session, -1 , V_NT_GEOMETRY, VN_OWNER_MINE);
	/* create new geometry data */
	geom_data = create_geometry_data();
	n_vnode->data = (void*)geom_data;
	
	/* set up name of VerseNode */
	n_vnode->name = (char*)MEM_mallocN(sizeof(char*)*(strlen(vnode->name)-1), "new geom node name");
	n_vnode->name[0] = '\0';
	strcat(n_vnode->name, vnode->name);
	
	/* add node to sending queue */
	add_item_to_send_queue(&(vnode->session->queue), n_vnode, VERSE_NODE);
	
	/* create vertex verse layer */
	n_vert_vlayer = create_verse_layer(n_vnode, 0, "vertex", VN_G_LAYER_VERTEX_XYZ, 0, 0);
	add_item_to_send_queue(&(geom_data->queue), n_vert_vlayer, VERSE_LAYER);

	/* create polygon verse layer */
	n_face_vlayer = create_verse_layer(n_vnode, 1, "polygon", VN_G_LAYER_POLYGON_CORNER_UINT32, 0, 0);
	add_item_to_send_queue(&(geom_data->queue), n_face_vlayer, VERSE_LAYER);

	/* find vertex layer of old verse node */
	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	/* find polygon layer of old verse node */
	face_vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	/* duplicate verse vertexes */
	for(i=0, vvert = (VerseVert*)vert_vlayer->dl.lb.first; vvert; vvert = vvert->next, i++) {
		n_vvert = create_verse_vertex(n_vert_vlayer, i, vvert->co[0], vvert->co[1], vvert->co[2]);
		vvert->tmp.vvert = n_vvert;
		add_item_to_send_queue(&(n_vert_vlayer->queue), n_vvert, VERSE_VERT);
	}

	/* duplicate verse faces (polyons) */
	for(i=0, vface = (VerseFace*)face_vlayer->dl.lb.first; vface; vface = vface->next, i++) {
		n_vface = create_verse_face(n_face_vlayer, i, -1, -1, -1, -1);
		n_vface->vvert0 = vface->vvert0->tmp.vvert;
		n_vface->vvert1 = vface->vvert1->tmp.vvert;
		n_vface->vvert2 = vface->vvert2->tmp.vvert;
		if(vface->vvert3)
			n_vface->vvert3 = vface->vvert3->tmp.vvert;
		else
			n_vface->vvert3 = NULL;
		add_item_to_send_queue(&(n_face_vlayer->queue), n_vface, VERSE_FACE);
	}

	return n_vnode;
}

/*
 * create new geometry node, make bindings between geometry node and editmesh,
 * make bindings between editverts and verseverts, make bindings between editfaces
 * and versefaces
 */
VNode *create_geom_vnode_data_from_editmesh(VerseSession *session, EditMesh *em)
{
	struct VNode *vnode;
	struct Mesh *me;
	struct VGeomData *geom_data;
	struct VLayer *vert_vlayer, *face_vlayer;
	
	if(!session) return NULL;
	
	if(!em) return NULL;

	/* some verse geometry node already exists */
	if(em->vnode) return NULL;

	/* we will need mesh too (mesh name, creating bindings between verse node, etc.) */
	me = get_mesh(G.obedit);
	
	/* create new verse node, when no one exist */
	vnode = create_verse_node(session, -1 , V_NT_GEOMETRY, VN_OWNER_MINE);
	/* create new geometry data */
	geom_data = create_geometry_data();
	vnode->data = (void*)geom_data;
	
	/* set up name of VerseNode */
	vnode->name = (char*)MEM_mallocN(sizeof(char*)*(strlen(me->id.name)-1), "geom node name");
	vnode->name[0] = '\0';
	strcat(vnode->name, me->id.name+2);

	/* set up bindings */
	me->vnode = (void*)vnode;
	em->vnode = (void*)vnode;
	geom_data->mesh = (void*)me;
	geom_data->editmesh = (void*)em;

	/* add node to sending queue */
	add_item_to_send_queue(&(session->queue), vnode, VERSE_NODE);

	/* create vertex verse layer */
	vert_vlayer = create_verse_layer(vnode, 0, "vertex", VN_G_LAYER_VERTEX_XYZ, 0, 0);
	add_item_to_send_queue(&(geom_data->queue), vert_vlayer, VERSE_LAYER);
	
	/* create polygon verse layer */
	face_vlayer = create_verse_layer(vnode, 1, "polygon", VN_G_LAYER_POLYGON_CORNER_UINT32, 0, 0);
	add_item_to_send_queue(&(geom_data->queue), face_vlayer, VERSE_LAYER);

	/* create all verse verts and add them to sending queue */
	createAllVerseVerts(vnode, vert_vlayer);

	/* create all verse faces and add tehm to sending queue */
	createAllVerseFaces(vnode, face_vlayer);

	return vnode;
}

/*
 * create new geometry node, make bindings between geometry node and mesh and 
 * fill geometry node with new data based at mesh data
 */
VNode *create_geom_vnode_data_from_mesh(VerseSession *session, Mesh *me)
{
	struct VNode *vnode;
	struct VGeomData *geom_data;
	struct VLayer *vert_vlayer, *face_vlayer;
	struct VerseVert *vvert, **vverts;
	struct VerseFace *vface;
	struct MVert *mvert;
	struct MFace *mface;
	int i;

	if(!session) return NULL;
	
	if(!me) return NULL;

	/* some verse geometry node already exists */
	if(me->vnode) return NULL;
	
	/* create new verse node, when no one exist */
	vnode = create_verse_node(session, -1 , V_NT_GEOMETRY, VN_OWNER_MINE);
	/* create new geometry data */
	geom_data = create_geometry_data();
	vnode->data = (void*)geom_data;
	
	/* set up name of VerseNode */
	vnode->name = (char*)MEM_mallocN(sizeof(char*)*(strlen(me->id.name)-1), "geom node name");
	vnode->name[0] = '\0';
	strcat(vnode->name, me->id.name+2);

	/* set up bindings */
	me->vnode = (void*)vnode;
	geom_data->mesh = (void*)me;
	
	/* add node to sending queue */
	add_item_to_send_queue(&(session->queue), vnode, VERSE_NODE);

	/* create vertex verse layer */
	vert_vlayer = create_verse_layer(vnode, 0, "vertex", VN_G_LAYER_VERTEX_XYZ, 0, 0);
	add_item_to_send_queue(&(geom_data->queue), vert_vlayer, VERSE_LAYER);
	
	/* create polygon verse layer */
	face_vlayer = create_verse_layer(vnode, 1, "polygon", VN_G_LAYER_POLYGON_CORNER_UINT32, 0, 0);
	add_item_to_send_queue(&(geom_data->queue), face_vlayer, VERSE_LAYER);

	/* temporary array of VerseVerts */
	vverts = (VerseVert**)MEM_mallocN(sizeof(VerseVert*)*me->totvert,"temp array of vverts");
	
	/* "fill vertex layer with vertexes" and add them to sending queue (send them to verse server) */
	for(i=0, mvert=me->mvert; i<me->totvert; i++, mvert++) {
		vverts[i] = vvert = create_verse_vertex(vert_vlayer, i, mvert->co[0], mvert->co[1], mvert->co[2]);
		add_item_to_send_queue(&(vert_vlayer->queue), vvert, VERSE_VERT);
	}

	/* "fill face/polygon layer with faces" and them to sending queue */
	for(i=0, mface = me->mface; i<me->totface; i++, mface++) {
		if(mface->v4) {
			vface = create_verse_face(face_vlayer, i, mface->v1, mface->v2, mface->v3, mface->v4);
			vface->vvert0 = vverts[mface->v1];
			vface->vvert1 = vverts[mface->v2];
			vface->vvert2 = vverts[mface->v3];
			vface->vvert3 = vverts[mface->v4];
			vface->counter = 4;
		}
		else {
			vface = create_verse_face(face_vlayer, i, mface->v1, mface->v2, mface->v3, -1);
			vface->vvert0 = vverts[mface->v1];
			vface->vvert1 = vverts[mface->v2];
			vface->vvert2 = vverts[mface->v3];
			vface->counter = 3;
		}
		add_item_to_send_queue(&(face_vlayer->queue), vface, VERSE_FACE);
	}

	MEM_freeN(vverts);

	return vnode;
}

/*
 * creates Mesh from verse geometry node and create bindings
 * between them
 */
Mesh *create_mesh_from_geom_node(VNode *vnode)
{
	struct Mesh *me;

	if(vnode->type != V_NT_GEOMETRY) return NULL;

	/* add new empty mesh*/
	me = add_mesh("Mesh");
	/* set up bindings between mesh and verse node */
	me->vnode = (void*)vnode;
	((VGeomData*)vnode->data)->mesh = (void*)me;

	return me;
}

/*
 * create mesh from verse mesh
 */
void create_meshdata_from_geom_node(Mesh *me, VNode *vnode)
{
	struct VLayer *vert_vlayer, *face_vlayer;
	struct VerseVert *vvert;
	struct VerseFace *vface;
	struct MVert *mvert;
	struct MFace *mface;
	struct EdgeHash *edges;
	int index;

	if(!me || !vnode) return;

	if(vnode->type != V_NT_GEOMETRY) return;

	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	face_vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	mesh_update_customdata_pointers(me);

	if(me->mselect) {
		MEM_freeN(me->mselect);
		me->mselect = NULL;
	}

	me->totvert = vert_vlayer ? vert_vlayer->dl.da.count : 0;
	me->totface = face_vlayer ? face_vlayer->dl.da.count : 0;
	me->totselect = 0;

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, me->totvert);
	CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, me->totface);
	mesh_update_customdata_pointers(me);

	mvert = me->mvert;
	mface = me->mface;

	index = 0;
	vvert = vert_vlayer ? vert_vlayer->dl.lb.first : NULL;
	while(vvert) {
		VECCOPY(mvert->co, vvert->co);
		VECCOPY(mvert->no, vvert->no);
		mvert->flag = 0;
		mvert->mat_nr = 0;
		vvert->tmp.index = index++;
		vvert = vvert->next;
		mvert++;
	}

	edges = BLI_edgehash_new();
	vface = face_vlayer ? face_vlayer->dl.lb.first : NULL;
	while(vface) {
		mface->v1 = vface->vvert0->tmp.index;
		mface->v2 = vface->vvert1->tmp.index;
		mface->v3 = vface->vvert2->tmp.index;

		if(!BLI_edgehash_haskey(edges, mface->v1, mface->v2))
			BLI_edgehash_insert(edges, mface->v1, mface->v2, NULL);
		if(!BLI_edgehash_haskey(edges, mface->v2, mface->v3))
			BLI_edgehash_insert(edges, mface->v2, mface->v3, NULL);
		if(vface->vvert3) {
			mface->v4 = vface->vvert3->tmp.index;
			if(!BLI_edgehash_haskey(edges, mface->v3, mface->v4))
				BLI_edgehash_insert(edges, mface->v3, mface->v4, NULL);
			if(!BLI_edgehash_haskey(edges, mface->v4, mface->v1))
				BLI_edgehash_insert(edges, mface->v4, mface->v1, NULL);
		} else {
			mface->v4 = 0;
			if(!BLI_edgehash_haskey(edges, mface->v3, mface->v1))
				BLI_edgehash_insert(edges, mface->v3, mface->v1, NULL);
		}

		mface->flag = 0;
		mface->pad = 0;
		mface->mat_nr = 0;
		mface->edcode = 0;

		/* index 0 isn't allowed at location 3 or 4 */
		test_index_face(mface, NULL, 0, vface->vvert3?4:3);
/*		printf("\t mface: %d, %d, %d, %d\n", mface->v1, mface->v2, mface->v3, mface->v4);*/
		
		vface = vface->next;
		mface++;
	}
	
	me->totedge = BLI_edgehash_size(edges);

	if(me->totedge) {
		EdgeHashIterator *i;
		MEdge *medge = me->medge = CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, me->totedge);

		for(i = BLI_edgehashIterator_new(edges); !BLI_edgehashIterator_isDone(i); BLI_edgehashIterator_step(i), ++medge) {
			memset(medge, 0, sizeof(struct MEdge));
			BLI_edgehashIterator_getKey(i, (int*)&medge->v1, (int*)&medge->v2);
		}
		BLI_edgehashIterator_free(i);
	}

	BLI_edgehash_free(edges, NULL);

	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

/*
 * Create EditMesh from VerseMesh and keep system in consitant state, this
 * function is called, when edit mode is entered ... edit mesh is generated
 * from verse mesh (not from Mesh: (Mesh*)ob->data)
 */
void create_edit_mesh_from_geom_node(VNode *vnode)
{
	struct VLayer *vert_layer, *face_layer;
	struct VerseVert *vvert;
	struct VerseFace *vface;
	struct Mesh *me;
	struct EditVert *eve, *eve0, *eve1, *eve2, *eve3;
	struct EditFace *efa;
	unsigned int keyindex;

	if(!(G.obedit && G.obedit->type==OB_MESH)) return;
	me = (Mesh*)G.obedit->data;
	if(vnode!=(VNode*)me->vnode || vnode->type!=V_NT_GEOMETRY) return;

	vert_layer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	face_layer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

	if(!(vert_layer && face_layer)) return;

	waitcursor(1);

	/* free old editMesh */
	free_editMesh(G.editMesh);
	
	G.editMesh->vnode = NULL;

	vvert = vert_layer->dl.lb.first;

	keyindex = 0;

	/* create all EditVerts */
	while(vvert) {
		eve = addvertlist(vvert->co, NULL);

		eve->f = 0;
		eve->h = 0;
		eve->data = NULL;
		eve->keyindex = keyindex;
		eve->vvert = (void*)vvert;

		vvert->vertex = (void*)eve;

		keyindex++;
		vvert = vvert->next;
	}

	vface = face_layer->dl.lb.first;

	/* create all EditFaces and EditEdges */
	while(vface) {
		if(vface->vvert0) eve0= vface->vvert0->vertex;
		else eve0 = NULL;
		if(vface->vvert1) eve1= vface->vvert1->vertex;
		else eve1 = NULL;
		if(vface->vvert2) eve2= vface->vvert2->vertex;
		else eve2 = NULL;
		if(vface->vvert3) eve3= vface->vvert3->vertex;
		else eve3 = NULL;

		efa= addfacelist(eve0, eve1, eve2, eve3, NULL, NULL);
		if(efa) {
			efa->f = 0;
			efa->h = 0;
			efa->vface = (void*)vface;
			vface->face = (void*)efa;
		}
		vface = vface->next;
	}
	
	countall();

	recalc_editnormals();

	G.editMesh->vnode = (void*)vnode;
	((VGeomData*)vnode->data)->editmesh = (void*)G.editMesh;

	waitcursor(0);
}

/*
 * destroy bindings between EditMesh and VerseMesh and send delete commands
 * for all VerseVerts and VerseFaces to verse server, Verse Node has to be
 * geometry node
 */

void destroy_verse_mesh(VNode *vnode)
{
	struct VLayer *vert_vlayer, *face_vlayer;
	struct VerseFace *vface;
	struct VerseVert *vvert;

	if(vnode->type != V_NT_GEOMETRY) return;

	face_vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);
	vface = face_vlayer->dl.lb.first;

	while(vface) {
		((EditFace*)vface->face)->vface = NULL;
		vface->face = NULL;	
		vface = vface->next;
	}

	vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	vvert = vert_vlayer->dl.lb.first;

	while(vvert) {
		((EditVert*)vvert->vertex)->vvert = NULL;
		vvert->vertex = NULL;
		vvert = vvert->next;
	}

	destroy_geometry(vnode);
}

#endif

