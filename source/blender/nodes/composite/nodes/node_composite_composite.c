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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_composite.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"



/* **************** COMPOSITE ******************** */
static bNodeSocketTemplate cmp_node_composite_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};

/* applies to render pipeline */
static void node_composit_exec_composite(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha, z */
	
	if (node->flag & NODE_DO_OUTPUT) {	/* only one works on out */
		Scene *scene= (Scene *)node->id;
		RenderData *rd= data;
		
		if (scene && (rd->scemode & R_DOCOMP)) {
			Render *re= RE_GetRender(scene->id.name);
			RenderResult *rr= RE_AcquireResultWrite(re); 
			if (rr) {
				CompBuf *outbuf, *zbuf=NULL;
				
				if (rr->rectf) 
					MEM_freeN(rr->rectf);
				outbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 1);
				
				if (in[1]->data==NULL)
					composit1_pixel_processor(node, outbuf, in[0]->data, in[0]->vec, do_copy_rgba, CB_RGBA);
				else
					composit2_pixel_processor(node, outbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
				
				if (in[2]->data) {
					if (rr->rectz) 
						MEM_freeN(rr->rectz);
					zbuf= alloc_compbuf(rr->rectx, rr->recty, CB_VAL, 1);
					composit1_pixel_processor(node, zbuf, in[2]->data, in[2]->vec, do_copy_value, CB_VAL);
					rr->rectz= zbuf->rect;
					zbuf->malloc= 0;
					free_compbuf(zbuf);
				}
				generate_preview(data, node, outbuf);
				
				/* we give outbuf to rr... */
				rr->rectf= outbuf->rect;
				outbuf->malloc= 0;
				free_compbuf(outbuf);

				/* signal for imageviewer to refresh (it converts to byte rects...) */
				BKE_image_signal(BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result"), NULL, IMA_SIGNAL_FREE);

				RE_ReleaseResult(re);
				return;
			}
			else
				RE_ReleaseResult(re);
		}
	}
	if (in[0]->data)
		generate_preview(data, node, in[0]->data);
}

void register_node_type_cmp_composite(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_COMPOSITE, "Composite", NODE_CLASS_OUTPUT, NODE_PREVIEW);
	node_type_socket_templates(&ntype, cmp_node_composite_in, NULL);
	node_type_size(&ntype, 80, 60, 200);
	node_type_exec(&ntype, node_composit_exec_composite);
	/* Do not allow muting for this node. */
	node_type_internal_connect(&ntype, NULL);

	nodeRegisterType(ttype, &ntype);
}
