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

#include <stdlib.h>

#include "DNA_ID.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_texture_types.h"

#include "BKE_blender.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

#include "render.h"		/* <- shadeinput/output */

/* ********* exec data struct, remains internal *********** */

typedef struct ShaderCallData {
	ShadeInput *shi;
	ShadeResult *shr;
} ShaderCallData;


/* **************** call to switch lamploop for material node ************ */

static void (*node_shader_lamp_loop)(ShadeInput *, ShadeResult *);
									 
void set_node_shader_lamp_loop(void (*lamp_loop_func)(ShadeInput *, ShadeResult *))
{
	node_shader_lamp_loop= lamp_loop_func;
}


/* **************** output node ************ */

static void node_shader_exec_output(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		float col[4];
		
		/* stack order input sockets: col, alpha, normal */
		VECCOPY(col, in[0]->vec);
		col[3]= in[1]->vec[0];
		
		if(shi->do_preview) {
			nodeAddToPreview(node, col, shi->xs, shi->ys);
			node->lasty= shi->ys;
		}
		
		if(node->flag & NODE_DO_OUTPUT) {
			ShadeResult *shr= ((ShaderCallData *)data)->shr;

			VECCOPY(shr->diff, col);
			col[0]= col[1]= col[2]= 0.0f;
			VECCOPY(shr->spec, col);
			shr->alpha= col[3];
			
			//	VECCOPY(shr->nor, in[3]->vec);
		}
	}	
}

/* **************** material node ************ */

static void node_shader_exec_material(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeResult shrnode;
		ShadeInput *shi;
		float col[4], *nor;
	
		shi= ((ShaderCallData *)data)->shi;
		
		shi->mat= (Material *)node->id;
		
		/* retrieve normal */
		if(in[0]->hasinput)
			nor= in[0]->vec;
		else
			nor= shi->vno;
		
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -nor[0];
			shi->vn[1]= -nor[1];
			shi->vn[2]= -nor[2];
		}
		else {
			VECCOPY(shi->vn, nor);
		}
		
		node_shader_lamp_loop(shi, &shrnode);
		
		if(node->custom1 & SH_NODE_MAT_DIFF) {
			VECCOPY(col, shrnode.diff);
			if(node->custom1 & SH_NODE_MAT_SPEC) {
				VecAddf(col, col, shrnode.spec);
			}
		}
		else if(node->custom1 & SH_NODE_MAT_SPEC) {
			VECCOPY(col, shrnode.spec);
		}
		else
			col[0]= col[1]= col[2]= 0.0f;
			
		col[3]= shrnode.alpha;
		
		if(shi->do_preview)
			nodeAddToPreview(node, col, shi->xs, shi->ys);
		
		/* stack order output: color, alpha, normal */
		VECCOPY(out[0]->vec, col);
		out[1]->vec[0]= shrnode.alpha;
		
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		VECCOPY(out[2]->vec, shi->vn);
	}
}

/* **************** texture node ************ */

static void node_shader_exec_texture(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		float *vec;
		int retval;
		
		/* out: value, color, normal */
		if(in[0]->hasinput)
			vec= in[0]->vec;
		else
			vec= shi->co;
		
		retval= multitex_ext((Tex *)node->id, vec, out[0]->vec, out[1]->vec, out[1]->vec+1, out[1]->vec+2, out[1]->vec+3);
		if((retval & TEX_RGB)==0) {
			out[1]->vec[0]= out[0]->vec[0];
			out[1]->vec[1]= out[0]->vec[0];
			out[1]->vec[2]= out[0]->vec[0];
			out[1]->vec[3]= 1.0f;
		}
		if(shi->do_preview)
			nodeAddToPreview(node, out[1]->vec, shi->xs, shi->ys);
		
	}
}

/* **************** geometry node ************ */

static void node_shader_exec_geom(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		
		/* out: global, local, view, orco, uv, normal */
		VECCOPY(out[0]->vec, shi->gl);
		VECCOPY(out[1]->vec, shi->co);
		VECCOPY(out[2]->vec, shi->view);
		VECCOPY(out[3]->vec, shi->lo);
		VECCOPY(out[4]->vec, shi->uv);
		VECCOPY(out[5]->vec, shi->vno);
	}
}

/* **************** normal node ************ */

/* generates normal, does dot product */
static void node_shader_exec_normal(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	/* stack order input:  normal */
	/* stack order output: normal, value */
	
	VECCOPY(out[0]->vec, sock->ns.vec);
	/* render normals point inside... the widget points outside */
	out[1]->vec[0]= -INPR(out[0]->vec, in[0]->vec);
}

/* **************** value node ************ */

static void node_shader_exec_value(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	out[0]->vec[0]= sock->ns.vec[0];
}

/* **************** rgba node ************ */

static void node_shader_exec_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	VECCOPY(out[0]->vec, sock->ns.vec);
}
									 

/* **************** mix rgb node ************ */

static void node_shader_exec_mix_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, col1, col2 */
	/* stack order out: col */
	float col[3];
	float fac= in[0]->vec[0];
	
	CLAMP(fac, 0.0f, 1.0f);
	
	VECCOPY(col, in[1]->vec);
	ramp_blend(node->custom1, col, col+1, col+2, fac, in[2]->vec);
	VECCOPY(out[0]->vec, col);
}

/* **************** val to rgb node ************ */

static void node_shader_exec_valtorgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac */
	/* stack order out: col, alpha */
	
	if(node->storage) {
		do_colorband(node->storage, in[0]->vec[0], out[0]->vec);
		out[1]->vec[0]= out[0]->vec[3];
	}
}

/* **************** rgb to bw node ************ */

static void node_shader_exec_rgbtobw(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */
	
	out[0]->vec[0]= in[0]->vec[0]*0.35f + in[0]->vec[1]*0.45f + in[0]->vec[2]*0.2f;
}


/* ******************* execute ************ */

void ntreeShaderExecTree(bNodeTree *ntree, ShadeInput *shi, ShadeResult *shr)
{
	ShaderCallData scd;
	
	/* convert caller data to struct */
	scd.shi= shi;
	scd.shr= shr;
	ntree->data= &scd;
	
	ntreeExecTree(ntree);
	
}


/* ******************************************************** */
/* ********* Shader Node type definitions ***************** */
/* ******************************************************** */

/* SocketType syntax: 
   socket type, max connections (0 is no limit), name, 4 values for default, 2 values for range */

/* Verification rule: If name changes, a saved socket and its links will be removed! Type changes are OK */

/* **************** OUTPUT ******************** */
static bNodeSocketType sh_node_output_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_output= {
	/* type code   */	SH_NODE_OUTPUT,
	/* name        */	"Output",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	sh_node_output_in,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_output,
	
};

/* **************** GEOMETRY INFO ******************** */
static bNodeSocketType sh_node_geom_out[]= {
	{	SOCK_VECTOR, 0, "Global",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},	/* btw; uses no limit */
	{	SOCK_VECTOR, 0, "Local",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "View",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Orco",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "UV",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_geom= {
	/* type code   */	SH_NODE_GEOMETRY,
	/* name        */	"Geometry",
	/* width+range */	60, 40, 100,
	/* class+opts  */	NODE_CLASS_INPUT, 0,
	/* input sock  */	NULL,
	/* output sock */	sh_node_geom_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_geom,
	
};

/* **************** MATERIAL ******************** */
static bNodeSocketType sh_node_material_in[]= {
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_material_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_material= {
	/* type code   */	SH_NODE_MATERIAL,
	/* name        */	"Material",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_material_in,
	/* output sock */	sh_node_material_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_material,
	
};

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

static bNodeType sh_node_texture= {
	/* type code   */	SH_NODE_TEXTURE,
	/* name        */	"Texture",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_texture_in,
	/* output sock */	sh_node_texture_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_texture,
	
};

/* **************** NORMAL  ******************** */
static bNodeSocketType sh_node_normal_in[]= {
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_normal_out[]= {
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VALUE, 0, "Dot",		1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_normal= {
	/* type code   */	SH_NODE_NORMAL,
	/* name        */	"Normal",
	/* width+range */	100, 60, 200,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	sh_node_normal_in,
	/* output sock */	sh_node_normal_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_normal,
	
};

/* **************** VALUE ******************** */
static bNodeSocketType sh_node_value_out[]= {
	{	SOCK_VALUE, 0, "Value",		0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_value= {
	/* type code   */	SH_NODE_VALUE,
	/* name        */	"Value",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	sh_node_value_out,
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_value,
	
};

/* **************** RGB ******************** */
static bNodeSocketType sh_node_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_rgb= {
	/* type code   */	SH_NODE_RGB,
	/* name        */	"RGB",
	/* width+range */	100, 60, 140,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	sh_node_rgb_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_rgb,
	
};

/* **************** MIX RGB ******************** */
static bNodeSocketType sh_node_mix_rgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Color1",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Color2",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_mix_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_mix_rgb= {
	/* type code   */	SH_NODE_MIX_RGB,
	/* name        */	"Mix",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	sh_node_mix_rgb_in,
	/* output sock */	sh_node_mix_rgb_out,
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_mix_rgb,
	
};


/* **************** VALTORGB ******************** */
static bNodeSocketType sh_node_valtorgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_valtorgb_out[]= {
	{	SOCK_RGBA, 0, "Color",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_valtorgb= {
	/* type code   */	SH_NODE_VALTORGB,
	/* name        */	"ColorRamp",
	/* width+range */	240, 200, 300,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	sh_node_valtorgb_in,
	/* output sock */	sh_node_valtorgb_out,
	/* storage     */	"ColorBand",
	/* execfunc    */	node_shader_exec_valtorgb,
	
};


/* **************** RGBTOBW ******************** */
static bNodeSocketType sh_node_rgbtobw_in[]= {
	{	SOCK_RGBA, 1, "Color",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_rgbtobw_out[]= {
	{	SOCK_VALUE, 0, "Val",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeType sh_node_rgbtobw= {
	/* type code   */	SH_NODE_RGBTOBW,
	/* name        */	"RGB to BW",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OPERATOR, 0,
	/* input sock  */	sh_node_rgbtobw_in,
	/* output sock */	sh_node_rgbtobw_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_rgbtobw,
	
};


/* ****************** types array for all shaders ****************** */

bNodeType *node_all_shaders[]= {
	&node_group_typeinfo,
	&sh_node_output,
	&sh_node_material,
	&sh_node_value,
	&sh_node_rgb,
	&sh_node_mix_rgb,
	&sh_node_valtorgb,
	&sh_node_rgbtobw,
	&sh_node_texture,
	&sh_node_normal,
	&sh_node_geom,
	NULL
};


