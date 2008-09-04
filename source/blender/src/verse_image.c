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

#include "mydevice.h"

#include "BKE_verse.h"
#include "BKE_image.h"

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BDR_drawmesh.h"

#include "BIF_verse.h"
#include "BIF_space.h"

/*
 * unsubscribe from verse bitmap
 */
void unsubscribe_from_bitmap_node(VNode *vnode)
{
	if(vnode->type != V_NT_BITMAP) return;
	
	/* TODO */
}

/*
 * upload image to verse server
 */
void push_image_to_verse_server(VerseSession *session, Image *image)
{
	ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);
	struct VNode *vnode;

	if(!session) return;

	if(!(session->flag & VERSE_CONNECTED)) return;

	/* create "my" new object VerseNode */
	vnode= create_verse_node(session, -1 , V_NT_BITMAP, VN_OWNER_MINE);
	/* create object data */
	vnode->data = create_bitmap_data();

	/* set up name of VerseNode */
	vnode->name = (char*)MEM_mallocN(sizeof(char*)*(strlen(image->id.name)-1), "object node name");
	vnode->name[0] = '\0';
	strcat(vnode->name, image->id.name+2);

	/* set up dimension of image */
	if(ibuf) {
		((VBitmapData*)vnode->data)->width = ibuf->x;
		((VBitmapData*)vnode->data)->height = ibuf->y;
	}
	else {
		((VBitmapData*)vnode->data)->width = 0;
		((VBitmapData*)vnode->data)->height = 0;
	}
	((VBitmapData*)(vnode->data))->height = 1;

	/* set up pointers between Object and VerseNode */
	((VBitmapData*)vnode->data)->image = (void*)image;
	image->vnode = (void*)vnode;

	/* add node to sending queue */
	add_item_to_send_queue(&(session->queue), vnode, VERSE_NODE);
}

/*
 * synchronize blender image channel (R,G,B,A) with verse bitmap layer
 */
void sync_blender_image_channel_with_verse_layer(VNode *vnode, VBitmapLayer *vblayer)
{
	struct Image *image = (Image*)((VBitmapData*)(vnode->data))->image;
	struct ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);
	unsigned char *rect;
	int x, y, height, t_width, i, channel=0;

	if(!image) return;

	if(!ibuf) return;

	rect = (unsigned char*)ibuf->rect;

	/* select channel due to verse layer name */
	if(strcmp(vblayer->name,"col_r")==0)
		channel = 0;
	else if(strcmp(vblayer->name,"col_g")==0)
		channel = 1;
	else if(strcmp(vblayer->name, "col_b")==0)
		channel = 2;
	else if(strcmp(vblayer->name,"alpha")==0)
		channel = 3;

#ifdef VERSE_DEBUG_PRINT
	printf(" %s:%d\n", vblayer->name, channel);
#endif

	height = ((VBitmapData*)(vnode->data))->height;
	t_width = ((VBitmapData*)(vnode->data))->t_width;

	i = (height-1)*t_width;

#ifdef VERSE_DEBUG_PRINT
	printf("\ti:%d\n", i);
#endif

	if(vblayer->type==VN_B_LAYER_UINT8) {
		unsigned char *vuint8 = (unsigned char*)vblayer->data;
		for(y=height-1; y>=0; y--, i=y*t_width)
			for(x=0; x<ibuf->x; x++, rect+=4, i++)
				rect[channel] = (char)vuint8[i];
	}

	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}

/*
 * synchronize blender image with verse image
 */
void sync_blender_image_with_verse_bitmap_node(VNode *vnode)
{
	struct VBitmapLayer *vblayer;

	vblayer = ((VBitmapData*)(vnode->data))->layers.lb.first;

	while(vblayer) {
#ifdef VERSE_DEBUG_PRINT
		printf("\tsyncing layer:");
#endif
		sync_blender_image_channel_with_verse_layer(vnode, vblayer);
		vblayer = vblayer->next;
	}
}

/*
 * This function is called, when some other verse client change dimension of image.
 * It is neccesary to reallocate blender image too, when dimension of verse image
 * is different from blender image.
 */
void post_bitmap_dimension_set(VNode *vnode)
{
	struct Image *image = (Image*)((VBitmapData*)(vnode->data))->image;
	struct ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);

	if(!image) return;

	if(!ibuf) return;

	if(vnode->owner_id == VN_OWNER_MINE) {
		if( ((VBitmapData*)vnode->data)->layers.lb.first == NULL ) {
			/* send all verse bitmap layers (RGBA) to verse server */
			printf("\tsending all bitmap layers to verse server\n");
			verse_send_b_layer_create(vnode->id, -1, "col_r", VN_B_LAYER_UINT8);
			verse_send_b_layer_create(vnode->id, -1, "col_g", VN_B_LAYER_UINT8);
			verse_send_b_layer_create(vnode->id, -1, "col_b", VN_B_LAYER_UINT8);
			verse_send_b_layer_create(vnode->id, -1, "alpha", VN_B_LAYER_UINT8);

			return;
		}
	}

	if((ibuf->x!=((VBitmapData*)vnode->data)->width) || (ibuf->y!=((VBitmapData*)vnode->data)->height)) {
		struct VBitmapLayer *vblayer;
		struct ImBuf *new_ibuf;

		/* allocate new ibuf */
		new_ibuf= IMB_allocImBuf(((VBitmapData*)vnode->data)->width,
				((VBitmapData*)vnode->data)->height, 24, IB_rect, 0);
		/* free old ibuf */
		BKE_image_signal(image, NULL, IMA_SIGNAL_FREE);
		/* set up pointer at new ibuf */
		BKE_image_assign_ibuf(image, ibuf);
		
		/* sync blender image with all verse layers */
		vblayer = ((VBitmapData*)(vnode->data))->layers.lb.first;
		while(vblayer) {
			sync_blender_image_channel_with_verse_layer(vnode, vblayer);
			vblayer = vblayer->next;
		}
	}
}

/*
 * when blender tries to upload image to verse server, then it is neccessary
 * to push coresponding channel data to verse server, when verse bitmap layer
 * was created
 */
void post_bitmap_layer_create(VBitmapLayer *vblayer)
{
	struct VNode *vnode = vblayer->vnode;
	struct Image *image = (Image*)((VBitmapData*)(vnode->data))->image;
	struct ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);
	unsigned char *rect;
	short channel;
/*	VNBTile tile[VN_B_TILE_SIZE*VN_B_TILE_SIZE];
	unsigned int width, t_width, height, t_height, x, y, i, j; */

	/* if this application doesn't try to upload this image to verse
	 * server then do nothing */
	if(vnode->owner_id != VN_OWNER_MINE) return;
	
	if(!image) return;

	if(!ibuf) return;

	rect = (unsigned char*)ibuf->rect;

	if(strncmp(vblayer->name, "col_r", 5))
		channel = 0;
	else if(strncmp(vblayer->name, "col_g", 5))
		channel = 1;
	else if(strncmp(vblayer->name, "col_b", 5))
		channel = 2;
	else if(strncmp(vblayer->name, "alpha", 5))
		channel = 3;

	/* TODO: send all data of channel to verse server */
}

/*
 * dummy function now
 */
void post_bitmap_layer_destroy(VBitmapLayer *vblayer)
{
}

/*
 * this function is executed, when some image changed tile comes from verse server,
 * it is neccessary to do some crazy transformation here, because blender uses
 * different (very unstandard) image coordinate system (begining of coordinate
 * system is in bottom left corner) ... all other programs (including verse) has
 * begining of image coordinate system in left top corner
 */
void post_bitmap_tile_set(VBitmapLayer *vblayer, unsigned int xs, unsigned int ys)
{
	struct VNode *vnode = vblayer->vnode;
	struct Image *image = (Image*)((VBitmapData*)(vnode->data))->image;
	struct ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);
	unsigned char *rect, *i_rect;
	unsigned int x, y, t_width, t_height, height, m_ys, m_y, d, i, j, channel=0;

	if(!image) return;

	if(!ibuf) return;

	/* select channel due to verse layer name */
	if(strcmp(vblayer->name,"color_r")==0)
		channel = 0;
	else if(strcmp(vblayer->name,"color_g")==0)
		channel = 1;
	else if(strcmp(vblayer->name, "color_b")==0)
		channel = 2;
	else if(strcmp(vblayer->name,"transparency")==0)
		channel = 3;

	i_rect = rect = (unsigned char*)ibuf->rect;

	/* width of verse image including all tiles */
	t_width =((VBitmapData*)vnode->data)->t_width;
	/* height of verse image including all tiles */
	t_height =((VBitmapData*)vnode->data)->t_height;
	/* height of blender image */
	height = ((VBitmapData*)vnode->data)->height;

	/* if the bitmap's dimensions are not integer multiples of the tile
	 * side length, eight, then d will not be zero (height of "uncomplete
	 * tile") */
	d = VN_B_TILE_SIZE - (t_height - height);
	/* mirrored coordination of received tile */
	m_ys = t_height - ys - VN_B_TILE_SIZE;

	/* ys and m_ys are y axis, where we will do some changes */
	if(ys + VN_B_TILE_SIZE > height) {
		m_ys = 0;
		ys = ys + d - 1;
	}
	else {
		m_ys = m_ys - VN_B_TILE_SIZE + d;
		ys = ys + VN_B_TILE_SIZE - 1;
	}

	/* "index" of blender image */
	j = m_ys*ibuf->x + xs;
	/* index of verse image */
	i = ys*t_width + xs;

	/* pointer at image data, that will be changed in following loop */
	rect = i_rect + 4*j;

	/* it seems hackish, but I didn't find better solution :-/ */
	if(vblayer->type==VN_B_LAYER_UINT8) {
		unsigned char *vuint8 = (unsigned char*)vblayer->data;
		for(y=ys, m_y = m_ys;
			(m_y<m_ys+VN_B_TILE_SIZE) && (m_y<ibuf->y);
			y--, m_y++, i=y*t_width+xs, j=m_y*ibuf->x+xs, rect=i_rect+4*j)
			for(x=xs; (x<xs+VN_B_TILE_SIZE) && (x<ibuf->x); x++, rect+=4, i++, j++)
				rect[channel] = (char)vuint8[i];
	}

	GPU_free_image(image);

	/* redraw preview of image ... uncommented, because rendering
	 * was computed too often */
/*	BIF_preview_changed(ID_TE); */
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}

#endif

