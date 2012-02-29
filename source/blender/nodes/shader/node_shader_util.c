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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/node_shader_util.c
 *  \ingroup nodes
 */


#include "DNA_node_types.h"

#include "node_shader_util.h"

#include "node_exec.h"

/* ****** */

void nodestack_get_vec(float *in, short type_in, bNodeStack *ns)
{
	float *from= ns->vec;
		
	if(type_in==SOCK_FLOAT) {
		if(ns->sockettype==SOCK_FLOAT)
			*in= *from;
		else 
			*in= 0.333333f*(from[0]+from[1]+from[2]);
	}
	else if(type_in==SOCK_VECTOR) {
		if(ns->sockettype==SOCK_FLOAT) {
			in[0]= from[0];
			in[1]= from[0];
			in[2]= from[0];
		}
		else {
			copy_v3_v3(in, from);
		}
	}
	else { /* type_in==SOCK_RGBA */
		if(ns->sockettype==SOCK_RGBA) {
			copy_v4_v4(in, from);
		}
		else if(ns->sockettype==SOCK_FLOAT) {
			in[0]= from[0];
			in[1]= from[0];
			in[2]= from[0];
			in[3]= 1.0f;
		}
		else {
			copy_v3_v3(in, from);
			in[3]= 1.0f;
		}
	}
}


/* go over all used Geometry and Texture nodes, and return a texco flag */
/* no group inside needed, this function is called for groups too */
void ntreeShaderGetTexcoMode(bNodeTree *ntree, int r_mode, short *texco, int *mode)
{
	bNode *node;
	bNodeSocket *sock;
	int a;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==SH_NODE_TEXTURE) {
			if((r_mode & R_OSA) && node->id) {
				Tex *tex= (Tex *)node->id;
				if ELEM3(tex->type, TEX_IMAGE, TEX_PLUGIN, TEX_ENVMAP) 
					*texco |= TEXCO_OSA|NEED_UV;
			}
			/* usability exception... without input we still give the node orcos */
			sock= node->inputs.first;
			if(sock==NULL || sock->link==NULL)
				*texco |= TEXCO_ORCO|NEED_UV;
		}
		else if(node->type==SH_NODE_GEOMETRY) {
			/* note; sockets always exist for the given type! */
			for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
				if(sock->flag & SOCK_IN_USE) {
					switch(a) {
						case GEOM_OUT_GLOB: 
							*texco |= TEXCO_GLOB|NEED_UV; break;
						case GEOM_OUT_VIEW: 
							*texco |= TEXCO_VIEW|NEED_UV; break;
						case GEOM_OUT_ORCO: 
							*texco |= TEXCO_ORCO|NEED_UV; break;
						case GEOM_OUT_UV: 
							*texco |= TEXCO_UV|NEED_UV; break;
						case GEOM_OUT_NORMAL: 
							*texco |= TEXCO_NORM|NEED_UV; break;
						case GEOM_OUT_VCOL:
							*texco |= NEED_UV; *mode |= MA_VERTEXCOL; break;
						case GEOM_OUT_VCOL_ALPHA:
							*texco |= NEED_UV; *mode |= MA_VERTEXCOL; break;
					}
				}
			}
		}
	}
}

/* nodes that use ID data get synced with local data */
void nodeShaderSynchronizeID(bNode *node, int copyto)
{
	if(node->id==NULL) return;
	
	if(ELEM(node->type, SH_NODE_MATERIAL, SH_NODE_MATERIAL_EXT)) {
		bNodeSocket *sock;
		Material *ma= (Material *)node->id;
		int a;
		
		/* hrmf, case in loop isnt super fast, but we dont edit 100s of material at same time either! */
		for(a=0, sock= node->inputs.first; sock; sock= sock->next, a++) {
			if(!nodeSocketIsHidden(sock)) {
				if(copyto) {
					switch(a) {
						case MAT_IN_COLOR:
							copy_v3_v3(&ma->r, ((bNodeSocketValueRGBA*)sock->default_value)->value); break;
						case MAT_IN_SPEC:
							copy_v3_v3(&ma->specr, ((bNodeSocketValueRGBA*)sock->default_value)->value); break;
						case MAT_IN_REFL:
							ma->ref= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
						case MAT_IN_MIR:
							copy_v3_v3(&ma->mirr, ((bNodeSocketValueRGBA*)sock->default_value)->value); break;
						case MAT_IN_AMB:
							ma->amb= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
						case MAT_IN_EMIT:
							ma->emit= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
						case MAT_IN_SPECTRA:
							ma->spectra= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
						case MAT_IN_RAY_MIRROR:
							ma->ray_mirror= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
						case MAT_IN_ALPHA:
							ma->alpha= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
						case MAT_IN_TRANSLUCENCY:
							ma->translucency= ((bNodeSocketValueFloat*)sock->default_value)->value; break;
					}
				}
				else {
					switch(a) {
						case MAT_IN_COLOR:
							copy_v3_v3(((bNodeSocketValueRGBA*)sock->default_value)->value, &ma->r); break;
						case MAT_IN_SPEC:
							copy_v3_v3(((bNodeSocketValueRGBA*)sock->default_value)->value, &ma->specr); break;
						case MAT_IN_REFL:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->ref; break;
						case MAT_IN_MIR:
							copy_v3_v3(((bNodeSocketValueRGBA*)sock->default_value)->value, &ma->mirr); break;
						case MAT_IN_AMB:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->amb; break;
						case MAT_IN_EMIT:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->emit; break;
						case MAT_IN_SPECTRA:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->spectra; break;
						case MAT_IN_RAY_MIRROR:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->ray_mirror; break;
						case MAT_IN_ALPHA:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->alpha; break;
						case MAT_IN_TRANSLUCENCY:
							((bNodeSocketValueFloat*)sock->default_value)->value= ma->translucency; break;
					}
				}
			}
		}
	}
	
}


void node_gpu_stack_from_data(struct GPUNodeStack *gs, int type, bNodeStack *ns)
{
	memset(gs, 0, sizeof(*gs));
	
	copy_v4_v4(gs->vec, ns->vec);
	gs->link= ns->data;
	
	if (type == SOCK_FLOAT)
		gs->type= GPU_FLOAT;
	else if (type == SOCK_VECTOR)
		gs->type= GPU_VEC3;
	else if (type == SOCK_RGBA)
		gs->type= GPU_VEC4;
	else if (type == SOCK_SHADER)
		gs->type= GPU_VEC4;
	else
		gs->type= GPU_NONE;
	
	gs->name = "";
	gs->hasinput= ns->hasinput && ns->data;
	/* XXX Commented out the ns->data check here, as it seems it’s not alwas set,
	 *     even though there *is* a valid connection/output... But that might need
	 *     further investigation.
	 */
	gs->hasoutput= ns->hasoutput /*&& ns->data*/;
	gs->sockettype= ns->sockettype;
}

void node_data_from_gpu_stack(bNodeStack *ns, GPUNodeStack *gs)
{
	ns->data= gs->link;
	ns->sockettype= gs->sockettype;
}

static void gpu_stack_from_data_list(GPUNodeStack *gs, ListBase *sockets, bNodeStack **ns)
{
	bNodeSocket *sock;
	int i;
	
	for (sock=sockets->first, i=0; sock; sock=sock->next, i++)
		node_gpu_stack_from_data(&gs[i], sock->type, ns[i]);
	
	gs[i].type= GPU_NONE;
}

static void data_from_gpu_stack_list(ListBase *sockets, bNodeStack **ns, GPUNodeStack *gs)
{
	bNodeSocket *sock;
	int i;

	for (sock=sockets->first, i=0; sock; sock=sock->next, i++)
		node_data_from_gpu_stack(ns[i], &gs[i]);
}

bNode *nodeGetActiveTexture(bNodeTree *ntree)
{
	/* this is the node we texture paint and draw in textured draw */
	bNode *node;

	if(!ntree)
		return NULL;

	/* check for group edit */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;

	if(node)
		ntree= (bNodeTree*)node->id;

	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_ACTIVE_TEXTURE)
			return node;
	
	return NULL;
}

void ntreeExecGPUNodes(bNodeTreeExec *exec, GPUMaterial *mat, int do_outputs)
{
	bNodeExec *nodeexec;
	bNode *node;
	int n;
	bNodeStack *stack;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	GPUNodeStack gpuin[MAX_SOCKET+1], gpuout[MAX_SOCKET+1];
	int doit;

	stack= exec->stack;

	for(n=0, nodeexec= exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
		node = nodeexec->node;
		
		doit = 0;
		/* for groups, only execute outputs for edited group */
		if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
			if(do_outputs && (node->flag & NODE_DO_OUTPUT))
				doit = 1;
		}
		else
			doit = 1;

		if (doit) {
			if(node->typeinfo->gpufunc) {
				node_get_stack(node, stack, nsin, nsout);
				gpu_stack_from_data_list(gpuin, &node->inputs, nsin);
				gpu_stack_from_data_list(gpuout, &node->outputs, nsout);
				if(node->typeinfo->gpufunc(mat, node, gpuin, gpuout))
					data_from_gpu_stack_list(&node->outputs, nsout, gpuout);
			}
			else if(node->typeinfo->gpuextfunc) {
				node_get_stack(node, stack, nsin, nsout);
				gpu_stack_from_data_list(gpuin, &node->inputs, nsin);
				gpu_stack_from_data_list(gpuout, &node->outputs, nsout);
				if(node->typeinfo->gpuextfunc(mat, node, nodeexec->data, gpuin, gpuout))
					data_from_gpu_stack_list(&node->outputs, nsout, gpuout);
			}
		}
	}
}

void node_shader_gpu_tex_mapping(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *UNUSED(out))
{
	NodeTexBase *base= node->storage;
	TexMapping *texmap= &base->tex_mapping;
	float domin= (texmap->flag & TEXMAP_CLIP_MIN) != 0;
	float domax= (texmap->flag & TEXMAP_CLIP_MAX) != 0;

	if(domin || domax || !(texmap->flag & TEXMAP_UNIT_MATRIX)) {
		GPUNodeLink *tmat = GPU_uniform((float*)texmap->mat);
		GPUNodeLink *tmin = GPU_uniform(texmap->min);
		GPUNodeLink *tmax = GPU_uniform(texmap->max);
		GPUNodeLink *tdomin = GPU_uniform(&domin);
		GPUNodeLink *tdomax = GPU_uniform(&domax);

		GPU_link(mat, "mapping", in[0].link, tmat, tmin, tmax, tdomin, tdomax, &in[0].link);
	}
}

