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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"


/* **************** VIEWER ******************** */
static bNodeSocketType cmp_node_viewer_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


static void node_composit_exec_viewer(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha, z */
	
	if(node->id && (node->flag & NODE_DO_OUTPUT)) {	/* only one works on out */
		RenderData *rd= data;
		Image *ima= (Image *)node->id;
		ImBuf *ibuf;
		CompBuf *cbuf, *tbuf;
		int rectx, recty;
		
		BKE_image_user_calc_imanr(node->storage, rd->cfra, 0);

		/* always returns for viewer image, but we check nevertheless */
		ibuf= BKE_image_get_ibuf(ima, node->storage);
		if(ibuf==NULL) {
			printf("node_composit_exec_viewer error\n");
			return;
		}
		
		/* free all in ibuf */
		imb_freerectImBuf(ibuf);
		imb_freerectfloatImBuf(ibuf);
		IMB_freezbuffloatImBuf(ibuf);
		
		/* get size */
		tbuf= in[0]->data?in[0]->data:(in[1]->data?in[1]->data:in[2]->data);
		if(tbuf==NULL) {
			rectx= 320; recty= 256;
		}
		else {
			rectx= tbuf->x;
			recty= tbuf->y;
		}
		
		/* make ibuf, and connect to ima */
		ibuf->x= rectx;
		ibuf->y= recty;
		imb_addrectfloatImBuf(ibuf);
		
		ima->ok= IMA_OK_LOADED;

		/* now we combine the input with ibuf */
		cbuf= alloc_compbuf(rectx, recty, CB_RGBA, 0);	/* no alloc*/
		cbuf->rect= ibuf->rect_float;
		
		/* when no alpha, we can simply copy */
		if(in[1]->data==NULL) {
			composit1_pixel_processor(node, cbuf, in[0]->data, in[0]->vec, do_copy_rgba, CB_RGBA);
		}
		else
			composit2_pixel_processor(node, cbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
		
		/* zbuf option */
		if(in[2]->data) {
			CompBuf *zbuf= alloc_compbuf(rectx, recty, CB_VAL, 1);
			ibuf->zbuf_float= zbuf->rect;
			ibuf->mall |= IB_zbuffloat;
			
			composit1_pixel_processor(node, zbuf, in[2]->data, in[2]->vec, do_copy_value, CB_VAL);
			
			/* free compbuf, but not the rect */
			zbuf->malloc= 0;
			free_compbuf(zbuf);
		}

		generate_preview(data, node, cbuf);
		free_compbuf(cbuf);

	}
	else if(in[0]->data) {
		generate_preview(data, node, in[0]->data);
	}
}

static void node_composit_init_viewer(bNode* node)
{
   ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "node image user");
   node->storage= iuser;
   iuser->sfra= 1;
   iuser->fie_ima= 2;
   iuser->ok= 1;
}

bNodeType cmp_node_viewer= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_VIEWER,
	/* name        */	"Viewer",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	cmp_node_viewer_in,
	/* output sock */	NULL,
	/* storage     */	"ImageUser",
	/* execfunc    */	node_composit_exec_viewer,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_viewer,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
	
};

