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
#include <string.h>

#include "DNA_ID.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_texture_types.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

#include "RE_shader_ext.h"		/* <- ShadeInput Shaderesult TexResult */


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
	{	-1, 0, ""	}
};

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

static bNodeType sh_node_output= {
	/* type code   */	SH_NODE_OUTPUT,
	/* name        */	"Output",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	sh_node_output_in,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_output
	
};

/* **************** GEOMETRY  ******************** */

/* output socket defines */
#define GEOM_OUT_GLOB	0
#define GEOM_OUT_LOCAL	1
#define GEOM_OUT_VIEW	2
#define GEOM_OUT_ORCO	3
#define GEOM_OUT_UV		4
#define GEOM_OUT_NORMAL	5

/* output socket type definition */
static bNodeSocketType sh_node_geom_out[]= {
	{	SOCK_VECTOR, 0, "Global",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},	/* btw; uses no limit */
	{	SOCK_VECTOR, 0, "Local",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "View",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Orco",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "UV",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

/* node execute callback */
static void node_shader_exec_geom(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		
		/* out: global, local, view, orco, uv, normal */
		VECCOPY(out[GEOM_OUT_GLOB]->vec, shi->gl);
		VECCOPY(out[GEOM_OUT_LOCAL]->vec, shi->co);
		VECCOPY(out[GEOM_OUT_VIEW]->vec, shi->view);
		VECCOPY(out[GEOM_OUT_ORCO]->vec, shi->lo);
		VECCOPY(out[GEOM_OUT_UV]->vec, shi->uv);
		VECCOPY(out[GEOM_OUT_NORMAL]->vec, shi->vno);
		
		if(shi->osatex) {
			out[GEOM_OUT_GLOB]->data= shi->dxgl;
			out[GEOM_OUT_GLOB]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_LOCAL]->data= shi->dxco;
			out[GEOM_OUT_LOCAL]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_VIEW]->data= &shi->dxview;
			out[GEOM_OUT_VIEW]->datatype= NS_OSA_VALUES;
			out[GEOM_OUT_ORCO]->data= shi->dxlo;
			out[GEOM_OUT_ORCO]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_UV]->data= shi->dxuv;
			out[GEOM_OUT_UV]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_NORMAL]->data= shi->dxno;
			out[GEOM_OUT_NORMAL]->datatype= NS_OSA_VECTORS;
		}
	}
}

/* node type definition */
static bNodeType sh_node_geom= {
	/* type code   */	SH_NODE_GEOMETRY,
	/* name        */	"Geometry",
	/* width+range */	60, 40, 100,
	/* class+opts  */	NODE_CLASS_INPUT, 0,
	/* input sock  */	NULL,
	/* output sock */	sh_node_geom_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_geom
	
};

/* **************** MATERIAL ******************** */

/* input socket defines */
#define MAT_IN_COLOR	0
#define MAT_IN_SPEC		1
#define MAT_IN_REFL		2
#define MAT_IN_NORMAL	3

static bNodeSocketType sh_node_material_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Spec",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Refl",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

/* output socket defines */
#define MAT_OUT_COLOR	0
#define MAT_OUT_ALPHA	1
#define MAT_OUT_NORMAL	2

static bNodeSocketType sh_node_material_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_material(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeResult shrnode;
		ShadeInput *shi;
		float col[4], *nor;
		
		shi= ((ShaderCallData *)data)->shi;
		shi->mat= (Material *)node->id;
		
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
		shi->har= shi->mat->har;
		
		/* write values */
		if(in[MAT_IN_COLOR]->hasinput)
			VECCOPY(&shi->r,  in[MAT_IN_COLOR]->vec);
		
		if(in[MAT_IN_SPEC]->hasinput)
			VECCOPY(&shi->specr,  in[MAT_IN_SPEC]->vec);
		
		if(in[MAT_IN_REFL]->hasinput)
			shi->mat->ref= in[MAT_IN_REFL]->vec[0]; 
		
		/* retrieve normal */
		if(in[MAT_IN_NORMAL]->hasinput) {
			nor= in[MAT_IN_NORMAL]->vec;
			Normalise(nor);
		}
		else
			nor= shi->vno;
		
		/* custom option to flip normal */
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -nor[0];
			shi->vn[1]= -nor[1];
			shi->vn[2]= -nor[2];
		}
		else {
			VECCOPY(shi->vn, nor);
		}
		
		node_shader_lamp_loop(shi, &shrnode);
		
		/* write to outputs */
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
		
		VECCOPY(out[MAT_OUT_COLOR]->vec, col);
		out[MAT_OUT_ALPHA]->vec[0]= shrnode.alpha;
		
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		
		VECCOPY(out[MAT_OUT_NORMAL]->vec, shi->vn);
		
	}
}

static bNodeType sh_node_material= {
	/* type code   */	SH_NODE_MATERIAL,
	/* name        */	"Material",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_material_in,
	/* output sock */	sh_node_material_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_material
	
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

static void node_shader_exec_texture(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		TexResult texres;
		float *vec, nor[3]={0.0f, 0.0f, 0.0f};
		int retval;
		
		/* out: value, color, normal */
		
		/* we should find out if a normal as output is needed, for now we do all */
		texres.nor= nor;
		
		if(in[0]->hasinput) {
			vec= in[0]->vec;
			
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
			vec= shi->lo;
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

static bNodeType sh_node_texture= {
	/* type code   */	SH_NODE_TEXTURE,
	/* name        */	"Texture",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_texture_in,
	/* output sock */	sh_node_texture_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_texture
	
};

/* **************** MAPPING  ******************** */
static bNodeSocketType sh_node_mapping_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_mapping_out[]= {
	{	SOCK_VECTOR, 0, "Vector",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

/* do the regular mapping options for blender textures */
static void node_shader_exec_mapping(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	TexMapping *texmap= node->storage;
	float *vec= out[0]->vec;
	
	/* stack order input:  vector */
	/* stack order output: vector */
	VECCOPY(vec, in[0]->vec);
	Mat4MulVecfl(texmap->mat, vec);
	
	if(texmap->flag & TEXMAP_CLIP_MIN) {
		if(vec[0]<texmap->min[0]) vec[0]= texmap->min[0];
		if(vec[1]<texmap->min[1]) vec[1]= texmap->min[1];
		if(vec[2]<texmap->min[2]) vec[2]= texmap->min[2];
	}
	if(texmap->flag & TEXMAP_CLIP_MAX) {
		if(vec[0]>texmap->max[0]) vec[0]= texmap->max[0];
		if(vec[1]>texmap->max[1]) vec[1]= texmap->max[1];
		if(vec[2]>texmap->max[2]) vec[2]= texmap->max[2];
	}
}

static bNodeType sh_node_mapping= {
	/* type code   */	SH_NODE_MAPPING,
	/* name        */	"Mapping",
	/* width+range */	240, 160, 320,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_mapping_in,
	/* output sock */	sh_node_mapping_out,
	/* storage     */	"TexMapping",
	/* execfunc    */	node_shader_exec_mapping
	
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

static bNodeType sh_node_normal= {
	/* type code   */	SH_NODE_NORMAL,
	/* name        */	"Normal",
	/* width+range */	100, 60, 200,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_normal_in,
	/* output sock */	sh_node_normal_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_normal
	
};

/* **************** CURVE VEC  ******************** */
static bNodeSocketType sh_node_curve_vec_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_curve_vec_out[]= {
	{	SOCK_VECTOR, 0, "Vector",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_curve_vec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  vec */
	/* stack order output: vec */
	
	curvemapping_evaluate3F(node->storage, out[0]->vec, in[0]->vec);
}

static bNodeType sh_node_curve_vec= {
	/* type code   */	SH_NODE_CURVE_VEC,
	/* name        */	"Vector Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_curve_vec_in,
	/* output sock */	sh_node_curve_vec_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_shader_exec_curve_vec
	
};

/* **************** CURVE RGB  ******************** */
static bNodeSocketType sh_node_curve_rgb_in[]= {
	{	SOCK_RGBA, 1, "Color",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_curve_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_curve_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  vec */
	/* stack order output: vec */
	
	curvemapping_evaluateRGBF(node->storage, out[0]->vec, in[0]->vec);
}

static bNodeType sh_node_curve_rgb= {
	/* type code   */	SH_NODE_CURVE_RGB,
	/* name        */	"RGB Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	sh_node_curve_rgb_in,
	/* output sock */	sh_node_curve_rgb_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_shader_exec_curve_rgb
	
};

/* **************** VALUE ******************** */
static bNodeSocketType sh_node_value_out[]= {
	{	SOCK_VALUE, 0, "Value",		0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_value(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	out[0]->vec[0]= sock->ns.vec[0];
}

static bNodeType sh_node_value= {
	/* type code   */	SH_NODE_VALUE,
	/* name        */	"Value",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	sh_node_value_out,
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_value
	
};

/* **************** RGB ******************** */
static bNodeSocketType sh_node_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	VECCOPY(out[0]->vec, sock->ns.vec);
}

static bNodeType sh_node_rgb= {
	/* type code   */	SH_NODE_RGB,
	/* name        */	"RGB",
	/* width+range */	100, 60, 140,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	sh_node_rgb_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_rgb
	
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

static bNodeType sh_node_mix_rgb= {
	/* type code   */	SH_NODE_MIX_RGB,
	/* name        */	"Mix",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	sh_node_mix_rgb_in,
	/* output sock */	sh_node_mix_rgb_out,
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_mix_rgb
	
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

static void node_shader_exec_valtorgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac */
	/* stack order out: col, alpha */
	
	if(node->storage) {
		do_colorband(node->storage, in[0]->vec[0], out[0]->vec);
		out[1]->vec[0]= out[0]->vec[3];
	}
}

static bNodeType sh_node_valtorgb= {
	/* type code   */	SH_NODE_VALTORGB,
	/* name        */	"ColorRamp",
	/* width+range */	240, 200, 300,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_valtorgb_in,
	/* output sock */	sh_node_valtorgb_out,
	/* storage     */	"ColorBand",
	/* execfunc    */	node_shader_exec_valtorgb
	
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


static void node_shader_exec_rgbtobw(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */
	
	out[0]->vec[0]= in[0]->vec[0]*0.35f + in[0]->vec[1]*0.45f + in[0]->vec[2]*0.2f;
}

static bNodeType sh_node_rgbtobw= {
	/* type code   */	SH_NODE_RGBTOBW,
	/* name        */	"RGB to BW",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	sh_node_rgbtobw_in,
	/* output sock */	sh_node_rgbtobw_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_rgbtobw
	
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
	&sh_node_mapping,
	&sh_node_curve_vec,
	&sh_node_curve_rgb,
	NULL
};

/* ******************* execute and parse ************ */

void ntreeShaderExecTree(bNodeTree *ntree, ShadeInput *shi, ShadeResult *shr)
{
	ShaderCallData scd;
	
	/* convert caller data to struct */
	scd.shi= shi;
	scd.shr= shr;
	
	ntreeExecTree(ntree, &scd, shi->thread);	/* threads */
}

/* go over all used Geometry and Texture nodes, and return a texco flag */
/* no group inside needed, this function is called for groups too */
int ntreeShaderGetTexco(bNodeTree *ntree, int osa)
{
	bNode *node;
	bNodeSocket *sock;
	int texco= 0, a;
	
	ntreeSocketUseFlags(ntree);
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==SH_NODE_TEXTURE) {
			if(osa && node->id) {
				Tex *tex= (Tex *)node->id;
				if ELEM3(tex->type, TEX_IMAGE, TEX_PLUGIN, TEX_ENVMAP) 
					texco |= TEXCO_OSA|NEED_UV;
			}
		}
		else if(node->type==SH_NODE_GEOMETRY) {
			/* note; sockets always exist for the given type! */
			for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
				if(sock->flag & SOCK_IN_USE) {
					switch(a) {
						case GEOM_OUT_GLOB: 
							texco |= TEXCO_GLOB|NEED_UV; break;
						case GEOM_OUT_VIEW: 
							texco |= TEXCO_VIEW|NEED_UV; break;
						case GEOM_OUT_ORCO: 
							texco |= TEXCO_ORCO|NEED_UV; break;
						case GEOM_OUT_UV: 
							texco |= TEXCO_UV|NEED_UV; break;
						case GEOM_OUT_NORMAL: 
							texco |= TEXCO_NORM|NEED_UV; break;
					}
				}
			}
		}
	}
	
	return texco;
}

/* nodes that use ID data get synced with local data */
void nodeShaderSynchronizeID(bNode *node, int copyto)
{
	if(node->id==NULL) return;
	
	if(node->type==SH_NODE_MATERIAL) {
		bNodeSocket *sock;
		Material *ma= (Material *)node->id;
		int a;
		
		/* hrmf, case in loop isnt super fast, but we dont edit 100s of material at same time either! */
		for(a=0, sock= node->inputs.first; sock; sock= sock->next, a++) {
			if(!(sock->flag & SOCK_HIDDEN)) {
				if(copyto) {
					switch(a) {
						case MAT_IN_COLOR:
							VECCOPY(&ma->r, sock->ns.vec); break;
						case MAT_IN_SPEC:
							VECCOPY(&ma->specr, sock->ns.vec); break;
						case MAT_IN_REFL:
							ma->ref= sock->ns.vec[0]; break;
					}
				}
				else {
					switch(a) {
						case MAT_IN_COLOR:
							VECCOPY(sock->ns.vec, &ma->r); break;
						case MAT_IN_SPEC:
							VECCOPY(sock->ns.vec, &ma->specr); break;
						case MAT_IN_REFL:
							sock->ns.vec[0]= ma->ref; break;
					}
				}
			}
		}
	}
	
}
