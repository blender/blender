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

//XXX #include "BIF_verse.h"

#include "BKE_verse.h"

#include "verse.h"

/* function prototypes of static functions */
static void cb_b_dimension_set(void *user_data, VNodeID node_id, uint16 width, uint16 height, uint16 depth);
static void cb_b_layer_create(void *user_data, VNodeID node_id, VLayerID layer_id, const char *name, VNBLayerType type);
static void cb_b_layer_destroy(void *user_data, VNodeID node_id, VLayerID layer_id);
static void cb_b_tile_set(void *user_data, VNodeID node_id, VLayerID layer_id, uint16 tile_x, uint16 tile_y, uint16 z, 	VNBLayerType type, const VNBTile *tile);

static void change_layer_dimension(
		VBitmapLayer *vblayer,
		unsigned int old_width,
		unsigned int old_height,
		unsigned int t_old_width,
		unsigned int t_old_height);
static void *alloc_verse_bitmap_layer_data(struct VBitmapLayer *vblayer);

/*
 * resize/crop verse bitmap layer
 */
static void change_layer_dimension(
		VBitmapLayer *vblayer,
		unsigned int old_width,
		unsigned int old_height,
		unsigned int t_old_width,
		unsigned int t_old_height)
{
	struct VNode *vnode = vblayer->vnode;
	unsigned int t_width = ((VBitmapData*)(vnode->data))->t_width;
	unsigned int width = ((VBitmapData*)(vnode->data))->width;
	unsigned int height = ((VBitmapData*)(vnode->data))->height;
	unsigned int x, y, i, j;
	
	i = j = 0;

	/* "copy" old data to new data */
	if(vblayer->type==VN_B_LAYER_UINT8) {
		unsigned char *data = (unsigned char*)vblayer->data;
		/* allocate new verse bitmap layer data */
		unsigned char *new_data = (unsigned char*)alloc_verse_bitmap_layer_data(vblayer);
		for(y=0; y<old_height && y<height; y++, i=y*t_width, j=y*t_old_width) {
			for(x=0; x<old_width && y<width; x++, i++, j++) {
				new_data[i] = data[j];
			}
		}
		MEM_freeN(vblayer->data);
		vblayer->data = new_data;
	}
}

/*
 * free data stored in verse bitmap layer
 */
void free_bitmap_layer_data(VBitmapLayer *vblayer)
{
	struct VerseSession *session = vblayer->vnode->session;

	/* free name of bitmap layer */
	MEM_freeN(vblayer->name);

	/* unsubscribe from verse bitmap layer */
	if(session->flag & VERSE_CONNECTED)
		verse_send_b_layer_unsubscribe(vblayer->vnode->id, vblayer->id);

	/* free image data of bitmap layer */
	if(vblayer->data) MEM_freeN(vblayer->data);
}

/*
 * allocate data of verse bitmap layer
 */
static void *alloc_verse_bitmap_layer_data(VBitmapLayer *vblayer)
{
	struct VNode *vnode = vblayer->vnode;
	unsigned int t_width = ((VBitmapData*)(vnode->data))->t_width;
	unsigned int t_height = ((VBitmapData*)(vnode->data))->t_height;
	unsigned int size;
	void *data;

	size = t_width*t_height;

	/* allocation of own data stored in verse bitmap layer */
	switch (vblayer->type) {
		case VN_B_LAYER_UINT1:
			data = (void*)MEM_mallocN(sizeof(unsigned char)*size, "VBLayer data uint1");
			break;
		case VN_B_LAYER_UINT8:
			data = (void*)MEM_mallocN(sizeof(unsigned char)*size, "VBLayer data uint8");
			break;
		case VN_B_LAYER_UINT16:
			data = (void*)MEM_mallocN(sizeof(unsigned int)*size, "VBLayer data uint16");
			break;
		case VN_B_LAYER_REAL32:
			data = (void*)MEM_mallocN(sizeof(float)*size, "VBLayer data float16");
			break;
		case VN_B_LAYER_REAL64:
			data = (void*)MEM_mallocN(sizeof(double)*size, "VBLayer data float32");
			break;
		default:
			data = NULL;
			break;
	}

	return data;
}

/*
 * create verse bitmap layer
 */
VBitmapLayer *create_bitmap_layer(
		VNode *vnode,
		VLayerID layer_id,
		const char *name,
		VNBLayerType type)
{
	struct VBitmapLayer *vblayer;
	unsigned int width = ((VBitmapData*)(vnode->data))->width;
	unsigned int height = ((VBitmapData*)(vnode->data))->height;

	/* allocate memory for own verse bitmap layer */
	vblayer = (VBitmapLayer*)MEM_mallocN(sizeof(VBitmapLayer), "Verse Bitmap Layer");

	/* verse bitmap layer will include pointer at parent verse node and own id */
	vblayer->vnode = vnode;
	vblayer->id = layer_id;

	/* name of verse layer */
	vblayer->name = (char*)MEM_mallocN(sizeof(char)*(strlen(name)+1), "Verse Bitmap Layer name");
	vblayer->name[0] = '\0';
	strcpy(vblayer->name, name);

	/* type of data stored in verse bitmap layer */
	vblayer->type = type;

	/* we can allocate memory for layer data, when we know dimmension of layers; when
	 * we don't know it, then we will allocate this data when we will receive dimmension */
	if(width==0 || height==0)
		vblayer->data = NULL;
	else
		vblayer->data = alloc_verse_bitmap_layer_data(vblayer);

	vblayer->flag = 0;

	return vblayer;
}

/*
 * free data of bitmap node
 */
void free_bitmap_node_data(VNode *vnode)
{
	if(vnode->data) {
		struct VBitmapLayer *vblayer = (VBitmapLayer*)((VBitmapData*)(vnode->data))->layers.lb.first;

		/* free all VerseLayer data */
		while(vblayer) {
			free_bitmap_layer_data(vblayer);
			vblayer = vblayer->next;
		}

		/* free all VerseLayers */
		BLI_dlist_destroy(&(((VGeomData*)vnode->data)->layers));
	}
}

/*
 * create data of bitmap node
 */
VBitmapData *create_bitmap_data()
{
	struct VBitmapData *vbitmap;

	vbitmap = (VBitmapData*)MEM_mallocN(sizeof(VBitmapData), "Verse Bitmap Data");

	BLI_dlist_init(&(vbitmap->layers));
	vbitmap->queue.first = vbitmap->queue.last = NULL;

	vbitmap->width = 0;
	vbitmap->height = 0;
	vbitmap->depth = 0;

	vbitmap->image = NULL;

	//XXX vbitmap->post_bitmap_dimension_set = post_bitmap_dimension_set;
	//XXX vbitmap->post_bitmap_layer_create = post_bitmap_layer_create;
	//XXX vbitmap->post_bitmap_layer_destroy = post_bitmap_layer_destroy;
	//XXX vbitmap->post_bitmap_tile_set = post_bitmap_tile_set;

	return vbitmap;
}

/*
 * callback function, dimension of image was changed, it is neccessary to
 * crop all layers
 */
static void cb_b_dimension_set(
		void *user_data,
		VNodeID node_id,
		uint16 width,
		uint16 height,
		uint16 depth)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VBitmapLayer *vblayer;
	unsigned int old_width, old_height, t_old_width, t_old_height;
	
	if(!session) return;

	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	if(!vnode) return;

#ifdef VERSE_DEBUG_PRINT
	printf("\t cb_b_dimension_set()\n");
#endif

	/* backup old width and height */
	old_width = ((VBitmapData*)(vnode->data))->width;
	old_height = ((VBitmapData*)(vnode->data))->height;
	t_old_width = ((VBitmapData*)(vnode->data))->t_width;
	t_old_height = ((VBitmapData*)(vnode->data))->t_height;

	/* set up new dimension of layers */
	((VBitmapData*)(vnode->data))->width = width;
	((VBitmapData*)(vnode->data))->height = height;
	((VBitmapData*)(vnode->data))->depth = depth;

	/* we cache t_width because tiles aren't one pixel width */
	if((width % VN_B_TILE_SIZE)!=0)
		((VBitmapData*)(vnode->data))->t_width = (width/VN_B_TILE_SIZE + 1)*VN_B_TILE_SIZE;
	else
		((VBitmapData*)(vnode->data))->t_width = width;

	/* we cache t_height because tiles aren't one pixel height */
	if((height % VN_B_TILE_SIZE)!=0)
		((VBitmapData*)(vnode->data))->t_height = (height/VN_B_TILE_SIZE + 1)*VN_B_TILE_SIZE;
	else
		((VBitmapData*)(vnode->data))->t_height = height;

	/* crop resize all layers */
	vblayer = ((VBitmapData*)vnode->data)->layers.lb.first;

	while(vblayer) {
		/* when this callback function received after cb_b_layer_create,
		 * then we have to allocate memory for verse bitmap layer data */
		if(!vblayer->data) vblayer->data = alloc_verse_bitmap_layer_data(vblayer);
		/* crop/resize all verse bitmap layers */
		else change_layer_dimension(vblayer, old_width, old_height, t_old_width, t_old_height);

		vblayer = vblayer->next;
	}
	
	/* post callback function */
	((VBitmapData*)(vnode->data))->post_bitmap_dimension_set(vnode);
}

/*
 * callback function, new layer channel of image was created
 */
static void cb_b_layer_create(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id,
		const char *name,
		VNBLayerType type)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VBitmapLayer *vblayer;
	
	if(!session) return;

	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	if(!vnode) return;
	
#ifdef VERSE_DEBUG_PRINT
	printf("\t cb_b_layer_create()\n");
#endif

	/* when no layer exists, then new layer will be created */
	vblayer = create_bitmap_layer(vnode, layer_id, name, type);

	/* add verse bitmap layer to list of layers */
	BLI_dlist_add_item_index(&((VBitmapData*)vnode->data)->layers, vblayer, layer_id);

	/* post callback function */
	((VBitmapData*)(vnode->data))->post_bitmap_layer_create(vblayer);

}

/*
 * callback function, existing layer of image was destroyed
 */
static void cb_b_layer_destroy(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VBitmapLayer *vblayer;
	
	if(!session) return;

	/* find node of this layer*/
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	if(!vnode) return;

	vblayer = (VBitmapLayer*)BLI_dlist_find_link(&(((VBitmapData*)vnode->data)->layers), layer_id);
	if(!vblayer) return;

#ifdef VERSE_DEBUG_PRINT
	printf("\t cb_b_layer_destroy()\n");
#endif

	/* remove verse bitmap layer from list of layers */
	BLI_dlist_rem_item(&(((VBitmapData*)vnode->data)->layers), layer_id);

	/* post callback function */
	((VBitmapData*)(vnode->data))->post_bitmap_layer_destroy(vblayer);

	/* free data of verse bitmap layer */
	free_bitmap_layer_data(vblayer);

	/* free verse bitmap layer */
	MEM_freeN(vblayer);
}

/*
 * callback function, small part (8x8 pixels) was changed
 */
static void cb_b_tile_set(
		void *user_data,
		VNodeID node_id,
		VLayerID layer_id,
		uint16 tile_x,
		uint16 tile_y,
		uint16 z,
		VNBLayerType type,
		const VNBTile *tile)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VBitmapLayer *vblayer;
	unsigned int x, y, xs, ys, width, height, t_height, t_width, i, j;
	
	if(!session) return;

	/* try to find verse node in dynamic list nodes */
	vnode = (VNode*)BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	if(!vnode) return;

	/* try to find verse bitmap layer in list of layers */
	vblayer = (VBitmapLayer*)BLI_dlist_find_link(&(((VBitmapData*)vnode->data)->layers), layer_id);
	if(!vblayer) return;

	/* we have to have allocated memory for bitmap layer */
	if(!vblayer->data) return;

	width = ((VBitmapData*)vnode->data)->width;
	height = ((VBitmapData*)vnode->data)->height;

	/* width of verse image including all tiles */
	t_height = ((VBitmapData*)vnode->data)->t_height;
	/* height of verse image including all tiles */
	t_width = ((VBitmapData*)vnode->data)->t_width;

#ifdef VERSE_DEBUG_PRINT
	printf("\t cb_b_tile_set()\n");
#endif

	xs = tile_x*VN_B_TILE_SIZE;
	ys = tile_y*VN_B_TILE_SIZE;

	/* initial position in one dimension vblayer->data (y_start*width + x_start) */
	i = ys*t_width + xs;
	/* intial position in one dimension tile array */
	j = 0;

	if(type==VN_B_LAYER_UINT8) {
		unsigned char *data = (unsigned char*)vblayer->data;
		for(y=ys; y<ys+VN_B_TILE_SIZE && y<height; y++, i=y*t_width+xs)
			for(x=xs; x<xs+VN_B_TILE_SIZE && x<width; x++, i++, j++)
				data[i] = (unsigned char)tile->vuint8[j];
	}

	/* post callback function */
	((VBitmapData*)(vnode->data))->post_bitmap_tile_set(vblayer, xs, ys);
}

/*
 * set up all callbacks functions for image nodes
 */
void set_bitmap_callbacks(void)
{
	/* dimension (size) of bitmap was set up or changes (image will be croped) */
	verse_callback_set(verse_send_b_dimensions_set, cb_b_dimension_set, NULL);

	/* new layer (chanell) of image was added or created */
	verse_callback_set(verse_send_b_layer_create, cb_b_layer_create, NULL);

	/* existing layer was destroyed */
	verse_callback_set(verse_send_b_layer_destroy, cb_b_layer_destroy, NULL);

	/* some tile (small part 8x8 pixels of image was changed) */
	verse_callback_set(verse_send_b_tile_set, cb_b_tile_set, NULL);
}

#endif

