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

#include "SHD_util.h"





/* ****** */

void nodestack_get_vec(float *in, short type_in, bNodeStack *ns)
{
	float *from= ns->vec;
		
	if(type_in==SOCK_VALUE) {
		if(ns->sockettype==SOCK_VALUE)
			*in= *from;
		else 
			*in= 0.333333f*(from[0]+from[1]+from[2]);
	}
	else if(type_in==SOCK_VECTOR) {
		if(ns->sockettype==SOCK_VALUE) {
			in[0]= from[0];
			in[1]= from[0];
			in[2]= from[0];
		}
		else {
			VECCOPY(in, from);
		}
	}
	else { /* type_in==SOCK_RGBA */
		if(ns->sockettype==SOCK_RGBA) {
			QUATCOPY(in, from);
		}
		else if(ns->sockettype==SOCK_VALUE) {
			in[0]= from[0];
			in[1]= from[0];
			in[2]= from[0];
			in[3]= 1.0f;
		}
		else {
			VECCOPY(in, from);
			in[3]= 1.0f;
		}
	}
}


/* ******************* execute and parse ************ */

void ntreeShaderExecTree(bNodeTree *ntree, ShadeInput *shi, ShadeResult *shr)
{
	ShaderCallData scd;
	
	/* convert caller data to struct */
	scd.shi= shi;
	scd.shr= shr;
	
	/* each material node has own local shaderesult, with optional copying */
	memset(shr, 0, sizeof(ShadeResult));
		   
	ntreeExecTree(ntree, &scd, shi->thread);	/* threads */
	
	/* better not allow negative for now */
	if(shr->combined[0]<0.0f) shr->combined[0]= 0.0f;
	if(shr->combined[1]<0.0f) shr->combined[1]= 0.0f;
	if(shr->combined[2]<0.0f) shr->combined[2]= 0.0f;
	
}

/* go over all used Geometry and Texture nodes, and return a texco flag */
/* no group inside needed, this function is called for groups too */
void ntreeShaderGetTexcoMode(bNodeTree *ntree, int r_mode, short *texco, int *mode)
{
	bNode *node;
	bNodeSocket *sock;
	int a;
	
	ntreeSocketUseFlags(ntree);

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
			if(!(sock->flag & SOCK_HIDDEN)) {
				if(copyto) {
					switch(a) {
						case MAT_IN_COLOR:
							VECCOPY(&ma->r, sock->ns.vec); break;
						case MAT_IN_SPEC:
							VECCOPY(&ma->specr, sock->ns.vec); break;
						case MAT_IN_REFL:
							ma->ref= sock->ns.vec[0]; break;
						case MAT_IN_MIR:
							VECCOPY(&ma->mirr, sock->ns.vec); break;
						case MAT_IN_AMB:
							ma->amb= sock->ns.vec[0]; break;
						case MAT_IN_EMIT:
							ma->emit= sock->ns.vec[0]; break;
						case MAT_IN_SPECTRA:
							ma->spectra= sock->ns.vec[0]; break;
						case MAT_IN_RAY_MIRROR:
							ma->ray_mirror= sock->ns.vec[0]; break;
						case MAT_IN_ALPHA:
							ma->alpha= sock->ns.vec[0]; break;
						case MAT_IN_TRANSLUCENCY:
							ma->translucency= sock->ns.vec[0]; break;
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
						case MAT_IN_MIR:
							VECCOPY(sock->ns.vec, &ma->mirr); break;
						case MAT_IN_AMB:
							sock->ns.vec[0]= ma->amb; break;
						case MAT_IN_EMIT:
							sock->ns.vec[0]= ma->emit; break;
						case MAT_IN_SPECTRA:
							sock->ns.vec[0]= ma->spectra; break;
						case MAT_IN_RAY_MIRROR:
							sock->ns.vec[0]= ma->ray_mirror; break;
						case MAT_IN_ALPHA:
							sock->ns.vec[0]= ma->alpha; break;
						case MAT_IN_TRANSLUCENCY:
							sock->ns.vec[0]= ma->translucency; break;
					}
				}
			}
		}
	}
	
}
