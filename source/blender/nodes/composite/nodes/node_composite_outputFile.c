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

/** \file blender/nodes/composite/nodes/node_composite_outputFile.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** OUTPUT FILE ******************** */
static bNodeSocketTemplate cmp_node_output_file_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Z",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static void node_composit_exec_output_file(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	
	if(in[0]->data) {
		RenderData *rd= data;
		NodeImageFile *nif= node->storage;
		if(nif->sfra!=nif->efra && (rd->cfra<nif->sfra || rd->cfra>nif->efra)) {
			return;	/* BAIL OUT RETURN */
		}
		else if (!G.rendering) {
			/* only output files when rendering a sequence -
			 * otherwise, it overwrites the output files just 
			 * scrubbing through the timeline when the compositor updates */
			return;
		} else {
			Main *bmain= G.main; /* TODO, have this passed along */
			CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
			ImBuf *ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
			char string[256];
			
			ibuf->rect_float= cbuf->rect;
			ibuf->dither= rd->dither_intensity;
			
			if (rd->color_mgt_flag & R_COLOR_MANAGEMENT)
				ibuf->profile = IB_PROFILE_LINEAR_RGB;
			
			if(in[1]->data) {
				CompBuf *zbuf= in[1]->data;
				if(zbuf->type==CB_VAL && zbuf->x==cbuf->x && zbuf->y==cbuf->y) {
					nif->im_format.flag |= R_IMF_FLAG_ZBUF;
					ibuf->zbuf_float= zbuf->rect;
				}
			}
			
			BKE_makepicstring(string, nif->name, bmain->name, rd->cfra, nif->im_format.imtype, (rd->scemode & R_EXTENSION), TRUE);
			
			if(0 == BKE_write_ibuf(ibuf, string, &nif->im_format))
				printf("Cannot save Node File Output to %s\n", string);
			else
				printf("Saved: %s\n", string);
			
			IMB_freeImBuf(ibuf);	
			
			generate_preview(data, node, cbuf);
			
			if(in[0]->data != cbuf) 
				free_compbuf(cbuf);
		}
	}
}

static void node_composit_init_output_file(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	Scene *scene= (Scene *)node->id;
	NodeImageFile *nif= MEM_callocN(sizeof(NodeImageFile), "node image file");
	node->storage= nif;

	if(scene) {
		BLI_strncpy(nif->name, scene->r.pic, sizeof(nif->name));
		nif->im_format= scene->r.im_format;
		if (BKE_imtype_is_movie(nif->im_format.imtype)) {
			nif->im_format.imtype= R_IMF_IMTYPE_OPENEXR;
		}
		nif->sfra= scene->r.sfra;
		nif->efra= scene->r.efra;
	}
}

void register_node_type_cmp_output_file(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_OUTPUT_FILE, "File Output", NODE_CLASS_OUTPUT, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_output_file_in, NULL);
	node_type_size(&ntype, 140, 80, 300);
	node_type_init(&ntype, node_composit_init_output_file);
	node_type_storage(&ntype, "NodeImageFile", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_output_file);

	nodeRegisterType(ttype, &ntype);
}
