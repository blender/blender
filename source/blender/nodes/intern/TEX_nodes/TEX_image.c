/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/intern/TEX_nodes/TEX_image.c
 *  \ingroup texnodes
 */


#include "../TEX_util.h"
#include "TEX_node.h"

static bNodeSocketType outputs[]= {
	{ SOCK_RGBA, 0, "Image",  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ -1, 0, "" }
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **UNUSED(in), short UNUSED(thread))
{
	float x = p->co[0];
	float y = p->co[1];
	Image *ima= (Image *)node->id;
	ImageUser *iuser= (ImageUser *)node->storage;
	
	if( ima ) {
		ImBuf *ibuf = BKE_image_get_ibuf(ima, iuser);
		if( ibuf ) {
			float xsize, ysize;
			float xoff, yoff;
			int px, py;
			
			float *result;

			xsize = ibuf->x / 2;
			ysize = ibuf->y / 2;
			xoff = yoff = -1;
					
			px = (int)( (x-xoff) * xsize );
			py = (int)( (y-yoff) * ysize );
		
			if( (!xsize) || (!ysize) ) return;
			
			if( !ibuf->rect_float ) {
				BLI_lock_thread(LOCK_IMAGE);
				if( !ibuf->rect_float )
					IMB_float_from_rect(ibuf);
				BLI_unlock_thread(LOCK_IMAGE);
			}
			
			while( px < 0 ) px += ibuf->x;
			while( py < 0 ) py += ibuf->y;
			while( px >= ibuf->x ) px -= ibuf->x;
			while( py >= ibuf->y ) py -= ibuf->y;
			
			result = ibuf->rect_float + py*ibuf->x*4 + px*4;
			QUATCOPY( out, result );
		}
	}
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn, data);
}

static void init(bNode* node)
{
   ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "node image user");
   node->storage= iuser;
   iuser->sfra= 1;
   iuser->fie_ima= 2;
   iuser->ok= 1;
}

void register_node_type_tex_image(ListBase *lb)
{
	static bNodeType ntype;
	
	node_type_base(&ntype, TEX_NODE_IMAGE, "Image", NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
				   NULL, outputs);
	node_type_size(&ntype, 120, 80, 300);
	node_type_init(&ntype, init);
	node_type_storage(&ntype, "ImageUser", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(lb, &ntype);
}
