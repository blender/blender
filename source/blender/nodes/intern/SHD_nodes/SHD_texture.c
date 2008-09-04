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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_texture_types.h"

#include "../SHD_util.h"

/* **************** TEXTURE ******************** */
static bNodeSocketType sh_node_texture_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},	/* no limit */
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_texture_out[]= {
	{	SOCK_VALUE, 0, "Value",		1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA , 0, "Color",		1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_texture(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		TexResult texres;
		float vec[3], nor[3]={0.0f, 0.0f, 0.0f};
		int retval;
		
		/* out: value, color, normal */
		
		/* we should find out if a normal as output is needed, for now we do all */
		texres.nor= nor;
		
		if(in[0]->hasinput) {
			nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
			
			if(in[0]->datatype==NS_OSA_VECTORS) {
				float *fp= in[0]->data;
				retval= multitex_ext((Tex *)node->id, vec, fp, fp+3, shi->osatex, &texres);
			}
			else if(in[0]->datatype==NS_OSA_VALUES) {
				float *fp= in[0]->data;
				float dxt[3], dyt[3];
				
				dxt[0]= fp[0]; dxt[1]= dxt[2]= 0.0f;
				dyt[0]= fp[1]; dyt[1]= dyt[2]= 0.0f;
				retval= multitex_ext((Tex *)node->id, vec, dxt, dyt, shi->osatex, &texres);
			}
			else
				retval= multitex_ext((Tex *)node->id, vec, NULL, NULL, 0, &texres);
		}
		else {	/* only for previewrender, so we see stuff */
			VECCOPY(vec, shi->lo);
			retval= multitex_ext((Tex *)node->id, vec, NULL, NULL, 0, &texres);
		}
		
		/* stupid exception */
		if( ((Tex *)node->id)->type==TEX_STUCCI) {
			texres.tin= 0.5f + 0.7f*texres.nor[0];
			CLAMP(texres.tin, 0.0f, 1.0f);
		}
		
		/* intensity and color need some handling */
		if(texres.talpha)
			out[0]->vec[0]= texres.ta;
		else
			out[0]->vec[0]= texres.tin;
		
		if((retval & TEX_RGB)==0) {
			out[1]->vec[0]= out[0]->vec[0];
			out[1]->vec[1]= out[0]->vec[0];
			out[1]->vec[2]= out[0]->vec[0];
			out[1]->vec[3]= 1.0f;
		}
		else {
			out[1]->vec[0]= texres.tr;
			out[1]->vec[1]= texres.tg;
			out[1]->vec[2]= texres.tb;
			out[1]->vec[3]= 1.0f;
		}
		
		VECCOPY(out[2]->vec, nor);
		
		if(shi->do_preview)
			nodeAddToPreview(node, out[1]->vec, shi->xs, shi->ys);
		
	}
}

static int gpu_shader_texture(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	Tex *tex = (Tex*)node->id;

	if(tex && tex->type == TEX_IMAGE && tex->ima) {
		GPUNodeLink *texlink = GPU_image(tex->ima, NULL);
		return GPU_stack_link(mat, "texture_image", in, out, texlink);
	}
	else
		return 0;
}

bNodeType sh_node_texture= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_TEXTURE,
	/* name        */	"Texture",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_texture_in,
	/* output sock */	sh_node_texture_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_texture,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_texture
	
};

