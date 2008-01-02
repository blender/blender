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
#include "DNA_image_types.h"
#include "DNA_listBase.h"

#include "BLI_dynamiclist.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"

#include "BIF_verse.h"
#include "BIF_space.h"
#include "BIF_editmesh.h"
#include "BIF_drawimage.h"
#include "BIF_editmode_undo.h"
#include "BIF_toolbox.h"

#include "BKE_verse.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"

#include "BDR_editobject.h"

#include "verse.h"

/* button callback function, it test object name and send new name to verse server */
void test_and_send_idbutton_cb(void *obj, void *ob_name)
{
	struct Object *ob = (Object*)obj;
	char *name= (char*)ob_name;

	test_idbutton(name+2);

	if(ob->vnode) verse_send_node_name_set(((VNode*)ob->vnode)->id, name+2);
}

/*
 * duplicate verse object nodes
 */
void b_verse_duplicate_object(VerseSession *session, Object *ob, Object *n_ob)
{
	struct VNode *obj_vnode;

	if(!session) return;

	if(!(session->flag & VERSE_CONNECTED)) return;

	/* create "my" new object VerseNode */
	obj_vnode= create_verse_node(session, -1 , V_NT_OBJECT, VN_OWNER_MINE);
	/* create object data */
	obj_vnode->data = create_object_data();

	/* set up name of VerseNode */
	obj_vnode->name = (char*)MEM_mallocN(sizeof(char*)*(strlen(n_ob->id.name)-1), "object node name");
	obj_vnode->name[0] = '\0';
	strcat(obj_vnode->name, n_ob->id.name+2);

	/* set up object node transformation */
	VECCOPY(((VObjectData*)obj_vnode->data)->pos, n_ob->loc);
	EulToQuat(n_ob->rot, ((VObjectData*)obj_vnode->data)->quat);
	VECCOPY(((VObjectData*)obj_vnode->data)->scale, n_ob->size);

	/* set up pointers between Object and VerseNode */
	((VObjectData*)obj_vnode->data)->object = (void*)n_ob;
	n_ob->vnode = (void*)obj_vnode;

	/* add node to sending queue */
	add_item_to_send_queue(&(session->queue), obj_vnode, VERSE_NODE);

	if(ob->type==OB_MESH) {
		struct Mesh *me;
		struct VNode *geom_vnode;
		struct VLink *vlink; 
		
		/* when current mesh already shared at verse server, then only set up link
		 * between object node and geometry node */
		if(ob->data == n_ob->data) {
			geom_vnode = (VNode*)((Mesh*)ob->data)->vnode;
		}
		else {
			geom_vnode = create_geom_vnode_from_geom_vnode((VNode*)((Mesh*)ob->data)->vnode);
			me = (Mesh*)n_ob->data;
			me->vnode = (void*)geom_vnode;
			((VGeomData*)geom_vnode->data)->mesh = (void*)me;
			
		}
		/* create new link between VereseNodes */
		vlink = create_verse_link(session, obj_vnode, geom_vnode, -1, -1, "geometry");
		/* "send" link to verse server */
		add_item_to_send_queue(&(((VObjectData*)obj_vnode->data)->queue), vlink, VERSE_LINK);
	}
}

/*
 * temp hack: this function push mesh objects (edit mode only) to verse server
 */
void b_verse_push_object(VerseSession *session, Object *ob)
{
	struct VNode *obj_vnode;

	if(!session) return;

	if(!(session->flag & VERSE_CONNECTED)) return;

	/* create "my" new object VerseNode */
	obj_vnode= create_verse_node(session, -1 , V_NT_OBJECT, VN_OWNER_MINE);
	/* create object data */
	obj_vnode->data = create_object_data();

	/* set up name of VerseNode */
	obj_vnode->name = (char*)MEM_mallocN(sizeof(char*)*(strlen(ob->id.name)-1), "object node name");
	obj_vnode->name[0] = '\0';
	strcat(obj_vnode->name, ob->id.name+2);

	/* set up object node transformation */
	VECCOPY(((VObjectData*)obj_vnode->data)->pos, ob->loc);
	EulToQuat(ob->rot, ((VObjectData*)obj_vnode->data)->quat);
	VECCOPY(((VObjectData*)obj_vnode->data)->scale, ob->size);

	/* set up pointers between Object and VerseNode */
	((VObjectData*)obj_vnode->data)->object = (void*)ob;
	ob->vnode = (void*)obj_vnode;

	/* add node to sending queue */
	add_item_to_send_queue(&(session->queue), obj_vnode, VERSE_NODE);

	if(ob->type==OB_MESH) {
		struct VNode *geom_vnode;
		struct VLink *vlink; 

		if(G.obedit)
			geom_vnode = create_geom_vnode_data_from_editmesh(session, G.editMesh);
		else
			geom_vnode = create_geom_vnode_data_from_mesh(session, get_mesh(ob));
		
		if(geom_vnode) {
			/* create new link between VereseNodes */
			vlink = create_verse_link(session, obj_vnode, geom_vnode, -1, -1, "geometry");
			/* send link to verse server */
			add_item_to_send_queue(&(((VObjectData*)obj_vnode->data)->queue), vlink, VERSE_LINK);
		}
	}
}

/*
 * creates blender object from verse object node and it
 * will create links between them
 */
Object *create_object_from_verse_node(VNode *vnode)
{
	struct Object *ob;

	if(vnode->type != V_NT_OBJECT) return NULL;
	
	/* create new object*/
	ob = add_object(OB_MESH);
	/* set up bindings between verse node and blender object */
	ob->vnode = (void*)vnode;
	((VObjectData*)vnode->data)->object = (void*)ob;
	/* set up flags */
	((VObjectData*)vnode->data)->flag |= POS_RECEIVE_READY;
	((VObjectData*)vnode->data)->flag |= ROT_RECEIVE_READY;
	((VObjectData*)vnode->data)->flag |= SCALE_RECEIVE_READY;
	/* copy name from verse node to object */
	if(vnode->name) {
		char *str;
		str = (char*)MEM_mallocN(sizeof(char)*(strlen(vnode->name)+3), "temp object name");
		str[0] = '\0';
		strcat(str, "OB");
		strcat(str, vnode->name);
		strncpy(ob->id.name, str, 23);
		MEM_freeN(str);
	}
	/* subscribe for object transformation */
	verse_send_o_transform_subscribe(vnode->id, 0);
	
	return ob;
}

/*
 * Create blender object-mesh from verse object node, verse geometry node,
 */
void b_verse_pop_node(VNode *vnode)
{
	if((!vnode) || (!(vnode->data))) return;

	if(vnode->type==V_NT_OBJECT) {
		struct VNode *geom_node=NULL;
		struct VLink *vlink;
		struct VLayer *vlayer;
		struct Object *ob;
		struct Mesh *me;

		if(((VObjectData*)vnode->data)->object) {
			printf("\tError: already subscribed to object node.\n");
			return;
		}

		vlink = ((VObjectData*)vnode->data)->links.lb.first;

		/* try to find geometry node */
		while(vlink) {
			if(vlink->target && vlink->target->type==V_NT_GEOMETRY){
				geom_node = vlink->target;
				break;
			}
			vlink = vlink->next;
		}

		/* we are not interested now in avatars node, etc. (vnodes without
		 * links at geometry node) */
		if(!geom_node) return;

		/* subscribe to all verse geometry layer */
		vlayer = ((VGeomData*)geom_node->data)->layers.lb.first;
		while(vlayer) {
			verse_send_g_layer_subscribe(geom_node->id, vlayer->id, 0);
			vlayer = vlayer->next;
		}

		ob = create_object_from_verse_node(vnode);

		me = create_mesh_from_geom_node(geom_node);
		
		/* set up bindings between object and mesh */
		if(ob && me) ob->data = me;
	}
	else if(vnode->type==V_NT_BITMAP) {
		struct VBitmapData *vbitmap;
		struct VBitmapLayer *vblayer;
		float color[] = {0, 0, 0, 1};

		vbitmap = (VBitmapData*)vnode->data;

		vblayer = vbitmap->layers.lb.first;

		while(vblayer) {
			if(!(vblayer->flag & VBLAYER_SUBSCRIBED)) {
				/* 0 means level of subscription (full resolution) */
				verse_send_b_layer_subscribe(vnode->id, vblayer->id, 0);
				vblayer->flag |= VBLAYER_SUBSCRIBED;
			}
			vblayer = vblayer->next;
		}

		if(vbitmap->image) {
			printf("\tError: already subscribed to image node.\n");
			return;
		}

		vbitmap->image = (void*)BKE_add_image_size(
				vbitmap->width,
				vbitmap->height,
				vnode->name,
				0,
				0,
				color);
		((Image*)vbitmap->image)->vnode = (void*)vnode;
		sync_blender_image_with_verse_bitmap_node(vnode);

		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
}

/*
 * this function will unsubscribe object node from transformation, but it will
 * keep all tags and links at other nodes ... user could subscribe to this node
 * again in future
 */
void unsubscribe_from_obj_node(VNode *vnode)
{
	struct VerseSession *session = vnode->session;
	struct VLink *vlink;

	if(vnode->type != V_NT_OBJECT) return;

	/* unsubscribe from receiving changes of transformation matrix */
	if(session->flag & VERSE_CONNECTED)
		verse_send_o_transform_unsubscribe(vnode->id, 0);

	/* we have to reinitialize object node transformation */
	((VObjectData*)vnode->data)->pos[0] = 0.0f;
	((VObjectData*)vnode->data)->pos[1] = 0.0f;
	((VObjectData*)vnode->data)->pos[2] = 0.0f;
	
	((VObjectData*)vnode->data)->quat[0] = 0.0f;
	((VObjectData*)vnode->data)->quat[1] = 0.0f;
	((VObjectData*)vnode->data)->quat[2] = 0.0f;
	((VObjectData*)vnode->data)->quat[3] = 0.0f;

	((VObjectData*)vnode->data)->scale[0] = 0.0f;
	((VObjectData*)vnode->data)->scale[1] = 0.0f;
	((VObjectData*)vnode->data)->scale[2] = 0.0f;
	
	/* clear bindings between object and object node */
	if(((VObjectData*)vnode->data)->object) {
		((Object*)((VObjectData*)vnode->data)->object)->vnode = NULL;
		((VObjectData*)vnode->data)->object = NULL;
	}
	
	/* unsubscribe from all supported verse nodes */
	vlink = ((VObjectData*)vnode->data)->links.lb.first;
	while(vlink) {
		if(vlink->target->counter==1) {
			switch(vlink->target->type) {
				case V_NT_OBJECT:
					unsubscribe_from_obj_node(vlink->target);
					break;
				case V_NT_GEOMETRY:
					unsubscribe_from_geom_node(vlink->target);
					break;
				case V_NT_BITMAP:
					unsubscribe_from_bitmap_node(vlink->target);
					break;
				default:
					break;
			}
		}
		vlink = vlink->next;
	}
}

/*
 * when blender Object is deleted, then we have to unsubscribe and free all
 * VerseNode dependent on this object
 */
void b_verse_delete_object(Object *object)
{
	struct VNode *vnode;

	vnode = (VNode*)object->vnode;

	if(vnode) unsubscribe_from_obj_node(vnode);
}

/*
 * "fake" unsubscribing from object node and all child nodes
 */
void b_verse_unsubscribe(VNode *vnode)
{
	struct VLink *vlink = ((VObjectData*)vnode->data)->links.lb.first;
	struct Object *ob = (Object*)((VObjectData*)vnode->data)->object;
	
	if(vnode->type==V_NT_OBJECT) {
		/* exit edit mode */
		if(G.obedit && G.obedit->vnode == (void*)vnode)
			exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);

		/* when some geometry node is child of this object node, then create mesh data */
		while(vlink){
			if(vlink->target->type == V_NT_GEOMETRY) {
				struct Mesh *me;
				me = ((VGeomData*)vlink->target->data)->mesh;
				create_meshdata_from_geom_node(me, vlink->target);
				break;
			}
			vlink = vlink->next;
		}

		/* unsubscribe from object transformation and clear bindings between
		 * verse object node and object */
		unsubscribe_from_obj_node(vnode);

		/* when geometry node was shared with more object nodes, then make
		 * data single user */
		if(ob->type == OB_MESH) {
			struct ID *id = ob->data;
			if(id && id->us>1 && id->lib==0) {
				ob->recalc= OB_RECALC_DATA;
				ob->data = copy_mesh(ob->data);
				id->us--;
				id->newid= ob->data;
			}
		}
		
		/* reinitialize object derived mesh */
		makeDerivedMesh(ob, get_viewedit_datamask());
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}
	else if(vnode->type==V_NT_BITMAP) {
		/* fake ... it isn't impelemented yet ... poke jiri, when needed */
		unsubscribe_from_bitmap_node(vnode);
	}

	allqueue(REDRAWVIEW3D, 1);
}

/*
 * whe VerseLink is created between two nodes, the Object start to point at
 * coresponding data
 */
void post_link_set(VLink *vlink)
{
	struct VNode *target, *source;
	struct Object *ob=NULL;
	struct Mesh *me=NULL;

	source = vlink->source;
	target = vlink->target;

	if(source->type==V_NT_OBJECT && target->type==V_NT_GEOMETRY){
		if(((VObjectData*)source->data)->object)
			ob = (Object*)((VObjectData*)source->data)->object;
		if(((VGeomData*)target->data)->mesh)
			me = (Mesh*)((VGeomData*)target->data)->mesh;
		if(ob && me && ob->data!=me)  {
			ob->data = me;
			makeDerivedMesh(ob, get_viewedit_datamask());
		}
	}

	allqueue(REDRAWALL, 1);
}

/*
 * when VerseLink is deleted, then bindings between Object and data should be removed
 */
void post_link_destroy(VLink *vlink)
{
	struct VNode *source, *target;
	struct Object *ob;

	source = vlink->source;
	target = vlink->target;

	if(source->type==V_NT_OBJECT && target->type==V_NT_GEOMETRY) {
		if(((VObjectData*)source->data)->object) {
			ob = (Object*)((VObjectData*)source->data)->object;
			ob->data=NULL;
		}
	}
	
	allqueue(REDRAWALL, 1);
}

/*
 * update position of blender object
 */
void post_transform_pos(VNode *vnode)
{
	struct VObjectData *obj_data = (VObjectData*)vnode->data;
	struct Object *ob = (Object*)obj_data->object;

	VECCOPY(ob->loc, obj_data->pos);
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 1);
}

/*
 * update rotation of blender object
 */
void post_transform_rot(VNode *vnode)
{
	struct VObjectData *obj_data = (VObjectData*)vnode->data;
	struct Object *ob = (Object*)obj_data->object;

	/* convert quaternion to euler rotation */
	QuatToEul(obj_data->quat, ob->rot);

	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 1);
}

/*
 * update scale of blender object
 */
void post_transform_scale(VNode *vnode)
{
	struct VObjectData *obj_data = (VObjectData*)vnode->data;
	struct Object *ob = (Object*)obj_data->object;

	VECCOPY(ob->size, obj_data->scale);

	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 1);
}

/*
 * send transformation of Object to verse server
 */
void b_verse_send_transformation(Object *ob)
{
	struct VNode *vnode= ob->vnode;
	float quat[4];

	if(!vnode) return;

	/* if last sent position wasn't received yet, then next change of position
	 * can't be send until last send change is received */
	if( ((VObjectData*)vnode->data)->flag & POS_SEND_READY ) {
		if((((VObjectData*)vnode->data)->pos[0]!=ob->loc[0]) ||
				(((VObjectData*)vnode->data)->pos[1]!=ob->loc[1]) ||
				(((VObjectData*)vnode->data)->pos[2]!=ob->loc[2])) {
			VECCOPY(((VObjectData*)vnode->data)->pos, ob->loc);
			send_verse_object_position(vnode);
		}
	}

	/* if last sent rotation wasn't received yet, then next change of rotation
	 * can't be send until last send change is received */
	if( ((VObjectData*)vnode->data)->flag & ROT_SEND_READY ) {
		EulToQuat(ob->rot, quat);

		if((((VObjectData*)vnode->data)->quat[0] != quat[0]) ||
				(((VObjectData*)vnode->data)->quat[1] != quat[1]) ||
				(((VObjectData*)vnode->data)->quat[2] != quat[2]) ||
				(((VObjectData*)vnode->data)->quat[3] != quat[3])) {
			QUATCOPY(((VObjectData*)vnode->data)->quat, quat);
			send_verse_object_rotation(vnode);
		}
	}

	/* if last sent object size wasn't received yet, then next change of object size
	 * can't be send until last send change is received */
	if( ((VObjectData*)vnode->data)->flag & SCALE_SEND_READY ) {
		if((((VObjectData*)vnode->data)->scale[0]!=ob->size[0]) ||
				(((VObjectData*)vnode->data)->scale[1]!=ob->size[1]) ||
				(((VObjectData*)vnode->data)->scale[2]!=ob->size[2])) {
			VECCOPY(((VObjectData*)vnode->data)->scale, ob->size);
			send_verse_object_scale(vnode);
		}
	}

	verse_callback_update(0);
}

/*
 * free constraint between object VerseNode and blender Object
 */
void post_object_free_constraint(VNode *vnode)
{
	if(((VObjectData*)vnode->data)->object) {
		/* free pointer at verse derived mesh */
		struct Object *ob = (Object*)((VObjectData*)vnode->data)->object;
		if(ob) {
			if(ob->derivedFinal) {
				ob->derivedFinal->needsFree = 1;
				ob->derivedFinal->release((DerivedMesh*)ob->derivedFinal);
				ob->derivedFinal = NULL;
			}
			if(ob->derivedDeform) {
				ob->derivedDeform->needsFree = 1;
				ob->derivedDeform->release((DerivedMesh*)ob->derivedDeform);
				ob->derivedDeform = NULL;
			}
		}
		/* free constraint */
		((Object*)((VObjectData*)vnode->data)->object)->vnode = NULL;
		((VObjectData*)vnode->data)->object = NULL;
	}
}

#endif

