/**
* $Id:
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
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <math.h>
#include <string.h>


#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"

#include "BKE_colortools.h"
#include "BKE_utildefines.h"
#include "BKE_node.h"

/* local include */
#include "raycounter.h"
#include "renderpipeline.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "rendercore.h"
#include "shadbuf.h"
#include "shading.h"
#include "strand.h"
#include "texture.h"
#include "volumetric.h"
#include "zbuf.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


#define VECADDISFAC(v1,v3,fac) {*(v1)+= *(v3)*(fac); *(v1+1)+= *(v3+1)*(fac); *(v1+2)+= *(v3+2)*(fac);}



/* Shade Sample order:

- shade_samples_fill_with_ps()
	- for each sample
		- shade_input_set_triangle()  <- if prev sample-face is same, use shade_input_copy_triangle()
		- if vlr
			- shade_input_set_viewco()    <- not for ray or bake
			- shade_input_set_uv()        <- not for ray or bake
			- shade_input_set_normals()
- shade_samples()
	- if AO
		- shade_samples_do_AO()
	- if shading happens
		- for each sample
			- shade_input_set_shade_texco()
			- shade_samples_do_shade()
- OSA: distribute sample result with filter masking

	*/

/* initialise material variables in shadeinput, 
 * doing inverse gamma correction where applicable */
void shade_input_init_material(ShadeInput *shi)
{
	if (R.r.color_mgt_flag & R_COLOR_MANAGEMENT) {
		color_manage_linearize(&shi->r, &shi->mat->r);
		color_manage_linearize(&shi->specr, &shi->mat->specr);
		color_manage_linearize(&shi->mirr, &shi->mat->mirr);
		
		/* material ambr / ambg / ambb is overwritten from world
		color_manage_linearize(shi->ambr, shi->mat->ambr);
		*/
		
		/* note, keep this synced with render_types.h */
		memcpy(&shi->amb, &shi->mat->amb, 11*sizeof(float));
		shi->har= shi->mat->har;
	} else {
		/* note, keep this synced with render_types.h */
		memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
		shi->har= shi->mat->har;
	}

}

static void shadeinput_colors_linearize(ShadeInput *shi)
{
	color_manage_linearize(&shi->r, &shi->r);
	color_manage_linearize(&shi->specr, &shi->specr);
	color_manage_linearize(&shi->mirr, &shi->mirr);
}

/* also used as callback for nodes */
/* delivers a fully filled in ShadeResult, for all passes */
void shade_material_loop(ShadeInput *shi, ShadeResult *shr)
{
	/* because node materials don't have access to rendering context,
	 * inverse gamma correction must happen here. evil. */
	if (R.r.color_mgt_flag & R_COLOR_MANAGEMENT && shi->nodes == 1)
		shadeinput_colors_linearize(shi);
	
	shade_lamp_loop(shi, shr);	/* clears shr */
	
	if(shi->translucency!=0.0f) {
		ShadeResult shr_t;
		float fac= shi->translucency;
		
		shade_input_init_material(shi);

		VECCOPY(shi->vn, shi->vno);
		VECMUL(shi->vn, -1.0f);
		VECMUL(shi->facenor, -1.0f);
		shi->depth++;	/* hack to get real shadow now */
		shade_lamp_loop(shi, &shr_t);
		shi->depth--;

		/* a couple of passes */
		VECADDISFAC(shr->combined, shr_t.combined, fac);
		if(shi->passflag & SCE_PASS_SPEC)
			VECADDISFAC(shr->spec, shr_t.spec, fac);
		if(shi->passflag & SCE_PASS_DIFFUSE)
			VECADDISFAC(shr->diff, shr_t.diff, fac);
		if(shi->passflag & SCE_PASS_SHADOW)
			VECADDISFAC(shr->shad, shr_t.shad, fac);

		VECMUL(shi->vn, -1.0f);
		VECMUL(shi->facenor, -1.0f);
	}
	
	/* depth >= 1 when ray-shading */
	if(shi->depth==0) {
		if(R.r.mode & R_RAYTRACE) {
			if(shi->ray_mirror!=0.0f || ((shi->mat->mode & MA_TRANSP) && (shi->mat->mode & MA_RAYTRANSP) && shr->alpha!=1.0f)) {
				/* ray trace works on combined, but gives pass info */
				ray_trace(shi, shr);
			}
		}
		/* disable adding of sky for raytransp */
		if((shi->mat->mode & MA_TRANSP) && (shi->mat->mode & MA_RAYTRANSP))
			if((shi->layflag & SCE_LAY_SKY) && (R.r.alphamode==R_ADDSKY))
				shr->alpha= 1.0f;
	}	
	
	if(R.r.mode & R_RAYTRACE) {
		if (R.render_volumes_inside.first)
			shade_volume_inside(shi, shr);
	}
}


/* do a shade, finish up some passes, apply mist */
void shade_input_do_shade(ShadeInput *shi, ShadeResult *shr)
{
	float alpha;
	
	/* ------  main shading loop -------- */
#ifdef RE_RAYCOUNTER
	memset(&shi->raycounter, 0, sizeof(shi->raycounter));
#endif
	
	if(shi->mat->nodetree && shi->mat->use_nodes) {
		ntreeShaderExecTree(shi->mat->nodetree, shi, shr);
	}
	else {
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		shade_input_init_material(shi);
		
		if (shi->mat->material_type == MA_TYPE_VOLUME) {
			if(R.r.mode & R_RAYTRACE)
				shade_volume_outside(shi, shr);
		} else { /* MA_TYPE_SURFACE, MA_TYPE_WIRE */
			shade_material_loop(shi, shr);
		}
	}
	
	/* copy additional passes */
	if(shi->passflag & (SCE_PASS_VECTOR|SCE_PASS_NORMAL|SCE_PASS_RADIO)) {
		QUATCOPY(shr->winspeed, shi->winspeed);
		VECCOPY(shr->nor, shi->vn);
	}
	
	/* MIST */
	if((shi->passflag & SCE_PASS_MIST) || ((R.wrld.mode & WO_MIST) && (shi->mat->mode & MA_NOMIST)==0))  {
		if(R.r.mode & R_ORTHO)
			shr->mist= mistfactor(-shi->co[2], shi->co);
		else
			shr->mist= mistfactor(VecLength(shi->co), shi->co);
	}
	else shr->mist= 0.0f;
	
	if((R.wrld.mode & WO_MIST) && (shi->mat->mode & MA_NOMIST)==0 ) {
		alpha= shr->mist;
	}
	else alpha= 1.0f;
	
	/* add mist and premul color */
	if(shr->alpha!=1.0f || alpha!=1.0f) {
		float fac= alpha*(shr->alpha);
		shr->combined[3]= fac;
		
		if (shi->mat->material_type!= MA_TYPE_VOLUME)
			VecMulf(shr->combined, fac);
	}
	else
		shr->combined[3]= 1.0f;
	
	/* add z */
	shr->z= -shi->co[2];
	
	/* RAYHITS */
/*
	if(1 || shi->passflag & SCE_PASS_RAYHITS)
	{
		shr->rayhits[0] = (float)shi->raycounter.faces.test;
		shr->rayhits[1] = (float)shi->raycounter.bb.hit;
		shr->rayhits[2] = 0.0;
		shr->rayhits[3] = 1.0;
	}
 */
	RE_RC_MERGE(&re_rc_counter[shi->thread], &shi->raycounter);
}

/* **************************************************************************** */
/*                    ShadeInput                                                */
/* **************************************************************************** */


void vlr_set_uv_indices(VlakRen *vlr, int *i1, int *i2, int *i3)
{
	/* to prevent storing new tfaces or vcols, we check a split runtime */
	/* 		4---3		4---3 */
	/*		|\ 1|	or  |1 /| */
	/*		|0\ |		|/ 0| */
	/*		1---2		1---2 	0 = orig face, 1 = new face */
	
	/* Update vert nums to point to correct verts of original face */
	if(vlr->flag & R_DIVIDE_24) {  
		if(vlr->flag & R_FACE_SPLIT) {
			(*i1)++; (*i2)++; (*i3)++;
		}
		else {
			(*i3)++;
		}
	}
	else if(vlr->flag & R_FACE_SPLIT) {
		(*i2)++; (*i3)++; 
	}
}

/* copy data from face to ShadeInput, general case */
/* indices 0 1 2 3 only */
void shade_input_set_triangle_i(ShadeInput *shi, ObjectInstanceRen *obi, VlakRen *vlr, short i1, short i2, short i3)
{
	VertRen **vpp= &vlr->v1;
	
	shi->vlr= vlr;
	shi->obi= obi;
	shi->obr= obi->obr;

	shi->v1= vpp[i1];
	shi->v2= vpp[i2];
	shi->v3= vpp[i3];
	
	shi->i1= i1;
	shi->i2= i2;
	shi->i3= i3;
	
	/* note, shi->mat is set in node shaders */
	shi->mat= shi->mat_override?shi->mat_override:vlr->mat;
	
	shi->osatex= (shi->mat->texco & TEXCO_OSA);
	shi->mode= shi->mat->mode_l;		/* or-ed result for all nodes */

	/* facenormal copy, can get flipped */
	shi->flippednor= RE_vlakren_get_normal(&R, obi, vlr, shi->facenor);
	
	/* copy of original pre-flipped normal, for geometry->front/back node output */
	VECCOPY(shi->orignor, shi->facenor);
	if(shi->flippednor)
		VECMUL(shi->orignor, -1.0f);
	
	/* calculate vertexnormals */
	if(vlr->flag & R_SMOOTH) {
		VECCOPY(shi->n1, shi->v1->n);
		VECCOPY(shi->n2, shi->v2->n);
		VECCOPY(shi->n3, shi->v3->n);

		if(obi->flag & R_TRANSFORMED) {
			Mat3MulVecfl(obi->nmat, shi->n1);
			Mat3MulVecfl(obi->nmat, shi->n2);
			Mat3MulVecfl(obi->nmat, shi->n3);
		}

		if(!(vlr->flag & (R_NOPUNOFLIP|R_TANGENT))) {
			if(INPR(shi->facenor, shi->n1) < 0.0f) {
				shi->n1[0]= -shi->n1[0];
				shi->n1[1]= -shi->n1[1];
				shi->n1[2]= -shi->n1[2];
			}
			if(INPR(shi->facenor, shi->n2) < 0.0f) {
				shi->n2[0]= -shi->n2[0];
				shi->n2[1]= -shi->n2[1];
				shi->n2[2]= -shi->n2[2];
			}
			if(INPR(shi->facenor, shi->n3) < 0.0f) {
				shi->n3[0]= -shi->n3[0];
				shi->n3[1]= -shi->n3[1];
				shi->n3[2]= -shi->n3[2];
			}
		}
	}
}

/* note, facenr declared volatile due to over-eager -O2 optimizations
 * on cygwin (particularly -frerun-cse-after-loop)
 */

/* copy data from face to ShadeInput, scanline case */
void shade_input_set_triangle(ShadeInput *shi, volatile int obi, volatile int facenr, int normal_flip)
{
	if(facenr>0) {
		shi->obi= &R.objectinstance[obi];
		shi->obr= shi->obi->obr;
		shi->facenr= (facenr-1) & RE_QUAD_MASK;
		if( shi->facenr < shi->obr->totvlak ) {
			VlakRen *vlr= RE_findOrAddVlak(shi->obr, shi->facenr);
			
			if(facenr & RE_QUAD_OFFS)
				shade_input_set_triangle_i(shi, shi->obi, vlr, 0, 2, 3);
			else
				shade_input_set_triangle_i(shi, shi->obi, vlr, 0, 1, 2);
		}
		else
			shi->vlr= NULL;	/* general signal we got sky */
	}
	else
		shi->vlr= NULL;	/* general signal we got sky */
}

/* full osa case: copy static info */
void shade_input_copy_triangle(ShadeInput *shi, ShadeInput *from)
{
	/* not so nice, but works... warning is in RE_shader_ext.h */
	memcpy(shi, from, sizeof(struct ShadeInputCopy));
}

/* copy data from strand to shadeinput */
void shade_input_set_strand(ShadeInput *shi, StrandRen *strand, StrandPoint *spoint)
{
	/* note, shi->mat is set in node shaders */
	shi->mat= shi->mat_override? shi->mat_override: strand->buffer->ma;
	
	shi->osatex= (shi->mat->texco & TEXCO_OSA);
	shi->mode= shi->mat->mode_l;		/* or-ed result for all nodes */

	/* shade_input_set_viewco equivalent */
	VECCOPY(shi->co, spoint->co);
	VECCOPY(shi->view, shi->co);
	Normalize(shi->view);

	shi->xs= (int)spoint->x;
	shi->ys= (int)spoint->y;

	if(shi->osatex || (R.r.mode & R_SHADOW)) {
		VECCOPY(shi->dxco, spoint->dtco);
		VECCOPY(shi->dyco, spoint->dsco);
	}

	/* dxview, dyview, not supported */

	/* facenormal, simply viewco flipped */
	VECCOPY(shi->facenor, spoint->nor);
	VECCOPY(shi->orignor, shi->facenor);

	/* shade_input_set_normals equivalent */
	if(shi->mat->mode & MA_TANGENT_STR) {
		VECCOPY(shi->vn, spoint->tan)
	}
	else {
		float cross[3];

		Crossf(cross, spoint->co, spoint->tan);
		Crossf(shi->vn, cross, spoint->tan);
		Normalize(shi->vn);

		if(INPR(shi->vn, shi->view) < 0.0f)
			VecNegf(shi->vn);
	}

	VECCOPY(shi->vno, shi->vn);
}

void shade_input_set_strand_texco(ShadeInput *shi, StrandRen *strand, StrandVert *svert, StrandPoint *spoint)
{
	StrandBuffer *strandbuf= strand->buffer;
	ObjectRen *obr= strandbuf->obr;
	StrandVert *sv;
	int mode= shi->mode;		/* or-ed result for all nodes */
	short texco= shi->mat->texco;

	if((shi->mat->texco & TEXCO_REFL)) {
		/* shi->dxview, shi->dyview, not supported */
	}

	if(shi->osatex && (texco & (TEXCO_NORM|TEXCO_REFL))) {
		/* not supported */
	}

	if(mode & (MA_TANGENT_V|MA_NORMAP_TANG)) {
		VECCOPY(shi->tang, spoint->tan);
		VECCOPY(shi->nmaptang, spoint->tan);
	}

	if(mode & MA_STR_SURFDIFF) {
		float *surfnor= RE_strandren_get_surfnor(obr, strand, 0);

		if(surfnor)
			VECCOPY(shi->surfnor, surfnor)
		else
			VECCOPY(shi->surfnor, shi->vn)

		if(shi->mat->strand_surfnor > 0.0f) {
			shi->surfdist= 0.0f;
			for(sv=strand->vert; sv!=svert; sv++)
				shi->surfdist+=VecLenf(sv->co, (sv+1)->co);
			shi->surfdist += spoint->t*VecLenf(sv->co, (sv+1)->co);
		}
	}

	if(R.r.mode & R_SPEED) {
		float *speed;
		
		speed= RE_strandren_get_winspeed(shi->obi, strand, 0);
		if(speed)
			QUATCOPY(shi->winspeed, speed)
		else
			shi->winspeed[0]= shi->winspeed[1]= shi->winspeed[2]= shi->winspeed[3]= 0.0f;
	}

	/* shade_input_set_shade_texco equivalent */
	if(texco & NEED_UV) {
		if(texco & TEXCO_ORCO) {
			VECCOPY(shi->lo, strand->orco);
			/* no shi->osatex, orco derivatives are zero */
		}

		if(texco & TEXCO_GLOB) {
			VECCOPY(shi->gl, shi->co);
			Mat4MulVecfl(R.viewinv, shi->gl);
			
			if(shi->osatex) {
				VECCOPY(shi->dxgl, shi->dxco);
				Mat3MulVecfl(R.imat, shi->dxco);
				VECCOPY(shi->dygl, shi->dyco);
				Mat3MulVecfl(R.imat, shi->dyco);
			}
		}

		if(texco & TEXCO_STRAND) {
			shi->strandco= spoint->strandco;

			if(shi->osatex) {
				shi->dxstrand= spoint->dtstrandco;
				shi->dystrand= 0.0f;
			}
		}

		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))  {
			MCol *mcol;
			float *uv;
			char *name;
			int i;

			shi->totuv= 0;
			shi->totcol= 0;
			shi->actuv= obr->actmtface;
			shi->actcol= obr->actmcol;

			if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
				for (i=0; (mcol=RE_strandren_get_mcol(obr, strand, i, &name, 0)); i++) {
					ShadeInputCol *scol= &shi->col[i];
					char *cp= (char*)mcol;
					
					shi->totcol++;
					scol->name= name;

					scol->col[0]= cp[3]/255.0f;
					scol->col[1]= cp[2]/255.0f;
					scol->col[2]= cp[1]/255.0f;
				}

				if(shi->totcol) {
					shi->vcol[0]= shi->col[shi->actcol].col[0];
					shi->vcol[1]= shi->col[shi->actcol].col[1];
					shi->vcol[2]= shi->col[shi->actcol].col[2];
				}
				else {
					shi->vcol[0]= 0.0f;
					shi->vcol[1]= 0.0f;
					shi->vcol[2]= 0.0f;
				}
			}

			for (i=0; (uv=RE_strandren_get_uv(obr, strand, i, &name, 0)); i++) {
				ShadeInputUV *suv= &shi->uv[i];

				shi->totuv++;
				suv->name= name;

				if(strandbuf->overrideuv == i) {
					suv->uv[0]= -1.0f;
					suv->uv[1]= spoint->strandco;
					suv->uv[2]= 0.0f;
				}
				else {
					suv->uv[0]= -1.0f + 2.0f*uv[0];
					suv->uv[1]= -1.0f + 2.0f*uv[1];
					suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				}

				if(shi->osatex) {
					suv->dxuv[0]= 0.0f;
					suv->dxuv[1]= 0.0f;
					suv->dyuv[0]= 0.0f;
					suv->dyuv[1]= 0.0f;
				}

				if((mode & MA_FACETEXTURE) && i==obr->actmtface) {
					if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
						shi->vcol[0]= 1.0f;
						shi->vcol[1]= 1.0f;
						shi->vcol[2]= 1.0f;
					}
				}
			}

			if(shi->totuv == 0) {
				ShadeInputUV *suv= &shi->uv[0];

				suv->uv[0]= 0.0f;
				suv->uv[1]= spoint->strandco;
				suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				
				if(mode & MA_FACETEXTURE) {
					/* no tface? set at 1.0f */
					shi->vcol[0]= 1.0f;
					shi->vcol[1]= 1.0f;
					shi->vcol[2]= 1.0f;
				}
			}

		}

		if(texco & TEXCO_NORM) {
			shi->orn[0]= -shi->vn[0];
			shi->orn[1]= -shi->vn[1];
			shi->orn[2]= -shi->vn[2];
		}

		if(texco & TEXCO_REFL) {
			/* mirror reflection color textures (and envmap) */
			calc_R_ref(shi);    /* wrong location for normal maps! XXXXXXXXXXXXXX */
		}

		if(texco & TEXCO_STRESS) {
			/* not supported */
		}

		if(texco & TEXCO_TANGENT) {
			if((mode & MA_TANGENT_V)==0) {
				/* just prevent surprises */
				shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
				shi->nmaptang[0]= shi->nmaptang[1]= shi->nmaptang[2]= 0.0f;
			}
		}
	}

	/* this only avalailable for scanline renders */
	if(shi->depth==0) {
		if(texco & TEXCO_WINDOW) {
			shi->winco[0]= -1.0f + 2.0f*spoint->x/(float)R.winx;
			shi->winco[1]= -1.0f + 2.0f*spoint->y/(float)R.winy;
			shi->winco[2]= 0.0f;

			/* not supported */
			if(shi->osatex) {
				shi->dxwin[0]= 0.0f;
				shi->dywin[1]= 0.0f;
				shi->dxwin[0]= 0.0f;
				shi->dywin[1]= 0.0f;
			}
		}

		if(texco & TEXCO_STICKY) {
			/* not supported */
		}
	}
	
	if (R.r.color_mgt_flag & R_COLOR_MANAGEMENT) {
		if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)) {
			color_manage_linearize(shi->vcol, shi->vcol);
		}
	}
	
}

/* from scanline pixel coordinates to 3d coordinates, requires set_triangle */
void shade_input_calc_viewco(ShadeInput *shi, float x, float y, float z, float *view, float *dxyview, float *co, float *dxco, float *dyco)
{
	/* returns not normalized, so is in viewplane coords */
	calc_view_vector(view, x, y);
	
	if(shi->mat->material_type == MA_TYPE_WIRE) {
		/* wire cannot use normal for calculating shi->co, so
		 * we reconstruct the coordinate less accurate */
		if(R.r.mode & R_ORTHO)
			calc_renderco_ortho(co, x, y, z);
		else
			calc_renderco_zbuf(co, view, z);
	}
	else {
		/* for non-wire, intersect with the triangle to get the exact coord */
		float fac, dface, v1[3];
		
		VECCOPY(v1, shi->v1->co);
		if(shi->obi->flag & R_TRANSFORMED)
			Mat4MulVecfl(shi->obi->mat, v1);
		
		dface= v1[0]*shi->facenor[0]+v1[1]*shi->facenor[1]+v1[2]*shi->facenor[2];
		
		/* ortho viewplane cannot intersect using view vector originating in (0,0,0) */
		if(R.r.mode & R_ORTHO) {
			/* x and y 3d coordinate can be derived from pixel coord and winmat */
			float fx= 2.0f/(R.winx*R.winmat[0][0]);
			float fy= 2.0f/(R.winy*R.winmat[1][1]);
			
			co[0]= (x - 0.5f*R.winx)*fx - R.winmat[3][0]/R.winmat[0][0];
			co[1]= (y - 0.5f*R.winy)*fy - R.winmat[3][1]/R.winmat[1][1];
			
			/* using a*x + b*y + c*z = d equation, (a b c) is normal */
			if(shi->facenor[2]!=0.0f)
				co[2]= (dface - shi->facenor[0]*co[0] - shi->facenor[1]*co[1])/shi->facenor[2];
			else
				co[2]= 0.0f;
			
			if(dxco && dyco) {
				dxco[0]= fx;
				dxco[1]= 0.0f;
				if(shi->facenor[2]!=0.0f)
					dxco[2]= (shi->facenor[0]*fx)/shi->facenor[2];
				else 
					dxco[2]= 0.0f;
				
				dyco[0]= 0.0f;
				dyco[1]= fy;
				if(shi->facenor[2]!=0.0f)
					dyco[2]= (shi->facenor[1]*fy)/shi->facenor[2];
				else 
					dyco[2]= 0.0f;
				
				if(dxyview) {
					if(co[2]!=0.0f) fac= 1.0f/co[2]; else fac= 0.0f;
					dxyview[0]= -R.viewdx*fac;
					dxyview[1]= -R.viewdy*fac;
				}
			}
		}
		else {
			float div;
			
			div= shi->facenor[0]*view[0] + shi->facenor[1]*view[1] + shi->facenor[2]*view[2];
			if (div!=0.0f) fac= dface/div;
			else fac= 0.0f;
			
			co[0]= fac*view[0];
			co[1]= fac*view[1];
			co[2]= fac*view[2];
			
			/* pixel dx/dy for render coord */
			if(dxco && dyco) {
				float u= dface/(div - R.viewdx*shi->facenor[0]);
				float v= dface/(div - R.viewdy*shi->facenor[1]);
				
				dxco[0]= co[0]- (view[0]-R.viewdx)*u;
				dxco[1]= co[1]- (view[1])*u;
				dxco[2]= co[2]- (view[2])*u;
				
				dyco[0]= co[0]- (view[0])*v;
				dyco[1]= co[1]- (view[1]-R.viewdy)*v;
				dyco[2]= co[2]- (view[2])*v;
				
				if(dxyview) {
					if(fac!=0.0f) fac= 1.0f/fac;
					dxyview[0]= -R.viewdx*fac;
					dxyview[1]= -R.viewdy*fac;
				}
			}
		}
	}
	
	/* set camera coords - for scanline, it's always 0.0,0.0,0.0 (render is in camera space)
	 * however for raytrace it can be different - the position of the last intersection */
	shi->camera_co[0] = shi->camera_co[1] = shi->camera_co[2] = 0.0f;
	
	/* cannot normalize earlier, code above needs it at viewplane level */
	Normalize(view);
}

/* from scanline pixel coordinates to 3d coordinates, requires set_triangle */
void shade_input_set_viewco(ShadeInput *shi, float x, float y, float xs, float ys, float z)
{
	float *dxyview= NULL, *dxco= NULL, *dyco= NULL;
	
	/* currently in use for dithering (soft shadow), node preview, irregular shad */
	shi->xs= (int)xs;
	shi->ys= (int)ys;

	/* original scanline coordinate without jitter */
	shi->scanco[0]= x;
	shi->scanco[1]= y;
	shi->scanco[2]= z;

	/* check if we need derivatives */
	if(shi->osatex || (R.r.mode & R_SHADOW)) {
		dxco= shi->dxco;
		dyco= shi->dyco;

		if((shi->mat->texco & TEXCO_REFL))
			dxyview= &shi->dxview;
	}

	shade_input_calc_viewco(shi, xs, ys, z, shi->view, dxyview, shi->co, dxco, dyco);
}

/* calculate U and V, for scanline (silly render face u and v are in range -1 to 0) */
void shade_input_set_uv(ShadeInput *shi)
{
	VlakRen *vlr= shi->vlr;
	
	if((vlr->flag & R_SMOOTH) || (shi->mat->texco & NEED_UV) || (shi->passflag & SCE_PASS_UV)) {
		float v1[3], v2[3], v3[3];

		VECCOPY(v1, shi->v1->co);
		VECCOPY(v2, shi->v2->co);
		VECCOPY(v3, shi->v3->co);

		if(shi->obi->flag & R_TRANSFORMED) {
			Mat4MulVecfl(shi->obi->mat, v1);
			Mat4MulVecfl(shi->obi->mat, v2);
			Mat4MulVecfl(shi->obi->mat, v3);
		}

		/* exception case for wire render of edge */
		if(vlr->v2==vlr->v3) {
			float lend, lenc;
			
			lend= VecLenf(v2, v1);
			lenc= VecLenf(shi->co, v1);
			
			if(lend==0.0f) {
				shi->u=shi->v= 0.0f;
			}
			else {
				shi->u= - (1.0f - lenc/lend);
				shi->v= 0.0f;
			}
			
			if(shi->osatex) {
				shi->dx_u=  0.0f;
				shi->dx_v=  0.0f;
				shi->dy_u=  0.0f;
				shi->dy_v=  0.0f;
			}
		}
		else {
			/* most of this could become re-used for faces */
			float detsh, t00, t10, t01, t11, xn, yn, zn;
			int axis1, axis2;

			/* find most stable axis to project */
			xn= fabs(shi->facenor[0]);
			yn= fabs(shi->facenor[1]);
			zn= fabs(shi->facenor[2]);

			if(zn>=xn && zn>=yn) { axis1= 0; axis2= 1; }
			else if(yn>=xn && yn>=zn) { axis1= 0; axis2= 2; }
			else { axis1= 1; axis2= 2; }

			/* compute u,v and derivatives */
			t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
			t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];

			detsh= 1.0f/(t00*t11-t10*t01);
			t00*= detsh; t01*=detsh; 
			t10*=detsh; t11*=detsh;

			shi->u= (shi->co[axis1]-v3[axis1])*t11-(shi->co[axis2]-v3[axis2])*t10;
			shi->v= (shi->co[axis2]-v3[axis2])*t00-(shi->co[axis1]-v3[axis1])*t01;
			if(shi->osatex) {
				shi->dx_u=  shi->dxco[axis1]*t11- shi->dxco[axis2]*t10;
				shi->dx_v=  shi->dxco[axis2]*t00- shi->dxco[axis1]*t01;
				shi->dy_u=  shi->dyco[axis1]*t11- shi->dyco[axis2]*t10;
				shi->dy_v=  shi->dyco[axis2]*t00- shi->dyco[axis1]*t01;
			}

			/* u and v are in range -1 to 0, we allow a little bit extra but not too much, screws up speedvectors */
			CLAMP(shi->u, -2.0f, 1.0f);
			CLAMP(shi->v, -2.0f, 1.0f);
		}
	}	
}

void shade_input_set_normals(ShadeInput *shi)
{
	float u= shi->u, v= shi->v;
	float l= 1.0f+u+v;
	
	/* calculate vertexnormals */
	if(shi->vlr->flag & R_SMOOTH) {
		float *n1= shi->n1, *n2= shi->n2, *n3= shi->n3;
		
		shi->vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
		shi->vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
		shi->vn[2]= l*n3[2]-u*n1[2]-v*n2[2];
		
		Normalize(shi->vn);
	}
	else
		VECCOPY(shi->vn, shi->facenor);
	
	/* used in nodes */
	VECCOPY(shi->vno, shi->vn);

}

/* use by raytrace, sss, bake to flip into the right direction */
void shade_input_flip_normals(ShadeInput *shi)
{
	shi->facenor[0]= -shi->facenor[0];
	shi->facenor[1]= -shi->facenor[1];
	shi->facenor[2]= -shi->facenor[2];

	shi->vn[0]= -shi->vn[0];
	shi->vn[1]= -shi->vn[1];
	shi->vn[2]= -shi->vn[2];

	shi->vno[0]= -shi->vno[0];
	shi->vno[1]= -shi->vno[1];
	shi->vno[2]= -shi->vno[2];

	shi->flippednor= !shi->flippednor;
}

void shade_input_set_shade_texco(ShadeInput *shi)
{
	ObjectInstanceRen *obi= shi->obi;
	ObjectRen *obr= shi->obr;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;
	float u= shi->u, v= shi->v;
	float l= 1.0f+u+v, dl;
	int mode= shi->mode;		/* or-ed result for all nodes */
	short texco= shi->mat->texco;

	/* calculate dxno */
	if(shi->vlr->flag & R_SMOOTH) {
		
		if(shi->osatex && (texco & (TEXCO_NORM|TEXCO_REFL)) ) {
			float *n1= shi->n1, *n2= shi->n2, *n3= shi->n3;
			
			dl= shi->dx_u+shi->dx_v;
			shi->dxno[0]= dl*n3[0]-shi->dx_u*n1[0]-shi->dx_v*n2[0];
			shi->dxno[1]= dl*n3[1]-shi->dx_u*n1[1]-shi->dx_v*n2[1];
			shi->dxno[2]= dl*n3[2]-shi->dx_u*n1[2]-shi->dx_v*n2[2];
			dl= shi->dy_u+shi->dy_v;
			shi->dyno[0]= dl*n3[0]-shi->dy_u*n1[0]-shi->dy_v*n2[0];
			shi->dyno[1]= dl*n3[1]-shi->dy_u*n1[1]-shi->dy_v*n2[1];
			shi->dyno[2]= dl*n3[2]-shi->dy_u*n1[2]-shi->dy_v*n2[2];
			
		}
	}

	/* calc tangents */
	if (mode & (MA_TANGENT_V|MA_NORMAP_TANG) || R.flag & R_NEED_TANGENT) {
		float *tangent, *s1, *s2, *s3;
		float tl, tu, tv;

		if(shi->vlr->flag & R_SMOOTH) {
			tl= l;
			tu= u;
			tv= v;
		}
		else {
			/* qdn: flat faces have tangents too,
			   could pick either one, using average here */
			tl= 1.0f/3.0f;
			tu= -1.0f/3.0f;
			tv= -1.0f/3.0f;
		}

		shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
		shi->nmaptang[0]= shi->nmaptang[1]= shi->nmaptang[2]= 0.0f;

		if(mode & MA_TANGENT_V) {
			s1 = RE_vertren_get_tangent(obr, v1, 0);
			s2 = RE_vertren_get_tangent(obr, v2, 0);
			s3 = RE_vertren_get_tangent(obr, v3, 0);

			if(s1 && s2 && s3) {
				shi->tang[0]= (tl*s3[0] - tu*s1[0] - tv*s2[0]);
				shi->tang[1]= (tl*s3[1] - tu*s1[1] - tv*s2[1]);
				shi->tang[2]= (tl*s3[2] - tu*s1[2] - tv*s2[2]);

				if(obi->flag & R_TRANSFORMED)
					Mat3MulVecfl(obi->nmat, shi->tang);

				Normalize(shi->tang);
				VECCOPY(shi->nmaptang, shi->tang);
			}
		}

		if(mode & MA_NORMAP_TANG || R.flag & R_NEED_TANGENT) {
			tangent= RE_vlakren_get_nmap_tangent(obr, shi->vlr, 0);

			if(tangent) {
				int j1= shi->i1, j2= shi->i2, j3= shi->i3;

				vlr_set_uv_indices(shi->vlr, &j1, &j2, &j3);

				s1= &tangent[j1*3];
				s2= &tangent[j2*3];
				s3= &tangent[j3*3];

				shi->nmaptang[0]= (tl*s3[0] - tu*s1[0] - tv*s2[0]);
				shi->nmaptang[1]= (tl*s3[1] - tu*s1[1] - tv*s2[1]);
				shi->nmaptang[2]= (tl*s3[2] - tu*s1[2] - tv*s2[2]);

				if(obi->flag & R_TRANSFORMED)
					Mat3MulVecfl(obi->nmat, shi->nmaptang);

				Normalize(shi->nmaptang);
			}
		}
	}

	if(mode & MA_STR_SURFDIFF) {
		float *surfnor= RE_vlakren_get_surfnor(obr, shi->vlr, 0);

		if(surfnor) {
			VECCOPY(shi->surfnor, surfnor)
			if(obi->flag & R_TRANSFORMED)
				Mat3MulVecfl(obi->nmat, shi->surfnor);
		}
		else
			VECCOPY(shi->surfnor, shi->vn)

		shi->surfdist= 0.0f;
	}
	
	if(R.r.mode & R_SPEED) {
		float *s1, *s2, *s3;
		
		s1= RE_vertren_get_winspeed(obi, v1, 0);
		s2= RE_vertren_get_winspeed(obi, v2, 0);
		s3= RE_vertren_get_winspeed(obi, v3, 0);
		if(s1 && s2 && s3) {
			shi->winspeed[0]= (l*s3[0] - u*s1[0] - v*s2[0]);
			shi->winspeed[1]= (l*s3[1] - u*s1[1] - v*s2[1]);
			shi->winspeed[2]= (l*s3[2] - u*s1[2] - v*s2[2]);
			shi->winspeed[3]= (l*s3[3] - u*s1[3] - v*s2[3]);
		}
		else {
			shi->winspeed[0]= shi->winspeed[1]= shi->winspeed[2]= shi->winspeed[3]= 0.0f;
		}
	}

	/* pass option forces UV calc */
	if(shi->passflag & SCE_PASS_UV)
		texco |= (NEED_UV|TEXCO_UV);
	
	/* texture coordinates. shi->dxuv shi->dyuv have been set */
	if(texco & NEED_UV) {
		
		if(texco & TEXCO_ORCO) {
			if(v1->orco) {
				float *o1, *o2, *o3;
				
				o1= v1->orco;
				o2= v2->orco;
				o3= v3->orco;
				
				shi->lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
				shi->lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
				shi->lo[2]= l*o3[2]-u*o1[2]-v*o2[2];
				
				if(shi->osatex) {
					dl= shi->dx_u+shi->dx_v;
					shi->dxlo[0]= dl*o3[0]-shi->dx_u*o1[0]-shi->dx_v*o2[0];
					shi->dxlo[1]= dl*o3[1]-shi->dx_u*o1[1]-shi->dx_v*o2[1];
					shi->dxlo[2]= dl*o3[2]-shi->dx_u*o1[2]-shi->dx_v*o2[2];
					dl= shi->dy_u+shi->dy_v;
					shi->dylo[0]= dl*o3[0]-shi->dy_u*o1[0]-shi->dy_v*o2[0];
					shi->dylo[1]= dl*o3[1]-shi->dy_u*o1[1]-shi->dy_v*o2[1];
					shi->dylo[2]= dl*o3[2]-shi->dy_u*o1[2]-shi->dy_v*o2[2];
				}
			}

			VECCOPY(shi->duplilo, obi->dupliorco);
		}
		
		if(texco & TEXCO_GLOB) {
			VECCOPY(shi->gl, shi->co);
			Mat4MulVecfl(R.viewinv, shi->gl);
			if(shi->osatex) {
				VECCOPY(shi->dxgl, shi->dxco);
				// TXF: bug was here, but probably should be in convertblender.c, R.imat only valid if there is a world
				//Mat3MulVecfl(R.imat, shi->dxco);
				Mat4Mul3Vecfl(R.viewinv, shi->dxco);
				VECCOPY(shi->dygl, shi->dyco);
				//Mat3MulVecfl(R.imat, shi->dyco);
				Mat4Mul3Vecfl(R.viewinv, shi->dyco);
			}
		}
		
		if(texco & TEXCO_STRAND) {
			shi->strandco= (l*v3->accum - u*v1->accum - v*v2->accum);
			if(shi->osatex) {
				dl= shi->dx_u+shi->dx_v;
				shi->dxstrand= dl*v3->accum-shi->dx_u*v1->accum-shi->dx_v*v2->accum;
				dl= shi->dy_u+shi->dy_v;
				shi->dystrand= dl*v3->accum-shi->dy_u*v1->accum-shi->dy_v*v2->accum;
			}
		}
				
		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))  {
			VlakRen *vlr= shi->vlr;
			MTFace *tface;
			MCol *mcol;
			char *name;
			int i, j1=shi->i1, j2=shi->i2, j3=shi->i3;

			/* uv and vcols are not copied on split, so set them according vlr divide flag */
			vlr_set_uv_indices(vlr, &j1, &j2, &j3);

			shi->totuv= 0;
			shi->totcol= 0;
			shi->actuv= obr->actmtface;
			shi->actcol= obr->actmcol;

			if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
				for (i=0; (mcol=RE_vlakren_get_mcol(obr, vlr, i, &name, 0)); i++) {
					ShadeInputCol *scol= &shi->col[i];
					char *cp1, *cp2, *cp3;
					
					shi->totcol++;
					scol->name= name;

					cp1= (char *)(mcol+j1);
					cp2= (char *)(mcol+j2);
					cp3= (char *)(mcol+j3);
					
					scol->col[0]= (l*((float)cp3[3]) - u*((float)cp1[3]) - v*((float)cp2[3]))/255.0f;
					scol->col[1]= (l*((float)cp3[2]) - u*((float)cp1[2]) - v*((float)cp2[2]))/255.0f;
					scol->col[2]= (l*((float)cp3[1]) - u*((float)cp1[1]) - v*((float)cp2[1]))/255.0f;
				}

				if(shi->totcol) {
					shi->vcol[0]= shi->col[shi->actcol].col[0];
					shi->vcol[1]= shi->col[shi->actcol].col[1];
					shi->vcol[2]= shi->col[shi->actcol].col[2];
					shi->vcol[3]= 1.0f;
				}
				else {
					shi->vcol[0]= 0.0f;
					shi->vcol[1]= 0.0f;
					shi->vcol[2]= 0.0f;
					shi->vcol[3]= 1.0f;
				}
			}

			for (i=0; (tface=RE_vlakren_get_tface(obr, vlr, i, &name, 0)); i++) {
				ShadeInputUV *suv= &shi->uv[i];
				float *uv1, *uv2, *uv3;

				shi->totuv++;
				suv->name= name;
				
				uv1= tface->uv[j1];
				uv2= tface->uv[j2];
				uv3= tface->uv[j3];
				
				suv->uv[0]= -1.0f + 2.0f*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
				suv->uv[1]= -1.0f + 2.0f*(l*uv3[1]-u*uv1[1]-v*uv2[1]);
				suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */

				if(shi->osatex) {
					float duv[2];
					
					dl= shi->dx_u+shi->dx_v;
					duv[0]= shi->dx_u; 
					duv[1]= shi->dx_v;
					
					suv->dxuv[0]= 2.0f*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
					suv->dxuv[1]= 2.0f*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
					
					dl= shi->dy_u+shi->dy_v;
					duv[0]= shi->dy_u; 
					duv[1]= shi->dy_v;
					
					suv->dyuv[0]= 2.0f*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
					suv->dyuv[1]= 2.0f*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
				}

				if((mode & MA_FACETEXTURE) && i==obr->actmtface) {
					if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
						shi->vcol[0]= 1.0f;
						shi->vcol[1]= 1.0f;
						shi->vcol[2]= 1.0f;
						shi->vcol[3]= 1.0f;
					}
					if(tface && tface->tpage)
						render_realtime_texture(shi, tface->tpage);
				}


			}

			shi->dupliuv[0]= -1.0f + 2.0f*obi->dupliuv[0];
			shi->dupliuv[1]= -1.0f + 2.0f*obi->dupliuv[1];
			shi->dupliuv[2]= 0.0f;

			if(shi->totuv == 0) {
				ShadeInputUV *suv= &shi->uv[0];

				suv->uv[0]= 2.0f*(u+.5f);
				suv->uv[1]= 2.0f*(v+.5f);
				suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				
				if(mode & MA_FACETEXTURE) {
					/* no tface? set at 1.0f */
					shi->vcol[0]= 1.0f;
					shi->vcol[1]= 1.0f;
					shi->vcol[2]= 1.0f;
					shi->vcol[3]= 1.0f;
				}
			}
		}
		
		if(texco & TEXCO_NORM) {
			shi->orn[0]= -shi->vn[0];
			shi->orn[1]= -shi->vn[1];
			shi->orn[2]= -shi->vn[2];
		}
		
		if(texco & TEXCO_REFL) {
			/* mirror reflection color textures (and envmap) */
			calc_R_ref(shi);	/* wrong location for normal maps! XXXXXXXXXXXXXX */
		}
		
		if(texco & TEXCO_STRESS) {
			float *s1, *s2, *s3;
			
			s1= RE_vertren_get_stress(obr, v1, 0);
			s2= RE_vertren_get_stress(obr, v2, 0);
			s3= RE_vertren_get_stress(obr, v3, 0);
			if(s1 && s2 && s3) {
				shi->stress= l*s3[0] - u*s1[0] - v*s2[0];
				if(shi->stress<1.0f) shi->stress-= 1.0f;
				else shi->stress= (shi->stress-1.0f)/shi->stress;
			}
			else shi->stress= 0.0f;
		}
		
		if(texco & TEXCO_TANGENT) {
			if((mode & MA_TANGENT_V)==0) {
				/* just prevent surprises */
				shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
				shi->nmaptang[0]= shi->nmaptang[1]= shi->nmaptang[2]= 0.0f;
			}
		}
	}
	
	/* this only avalailable for scanline renders */
	if(shi->depth==0) {
		float x= shi->xs;
		float y= shi->ys;
		
		if(texco & TEXCO_WINDOW) {
			shi->winco[0]= -1.0f + 2.0f*x/(float)R.winx;
			shi->winco[1]= -1.0f + 2.0f*y/(float)R.winy;
			shi->winco[2]= 0.0f;
			if(shi->osatex) {
				shi->dxwin[0]= 2.0f/(float)R.winx;
				shi->dywin[1]= 2.0f/(float)R.winy;
				shi->dxwin[1]= shi->dxwin[2]= 0.0f;
				shi->dywin[0]= shi->dywin[2]= 0.0f;
			}
		}

		if(texco & TEXCO_STICKY) {
			float *s1, *s2, *s3;
			
			s1= RE_vertren_get_sticky(obr, v1, 0);
			s2= RE_vertren_get_sticky(obr, v2, 0);
			s3= RE_vertren_get_sticky(obr, v3, 0);
			
			if(s1 && s2 && s3) {
				float winmat[4][4], ho1[4], ho2[4], ho3[4];
				float Zmulx, Zmuly;
				float hox, hoy, l, dl, u, v;
				float s00, s01, s10, s11, detsh;
				
				/* old globals, localized now */
				Zmulx=  ((float)R.winx)/2.0f; Zmuly=  ((float)R.winy)/2.0f;

				if(shi->obi->flag & R_TRANSFORMED)
					zbuf_make_winmat(&R, shi->obi->mat, winmat);
				else
					zbuf_make_winmat(&R, NULL, winmat);

				zbuf_render_project(winmat, v1->co, ho1);
				zbuf_render_project(winmat, v2->co, ho2);
				zbuf_render_project(winmat, v3->co, ho3);
				
				s00= ho3[0]/ho3[3] - ho1[0]/ho1[3];
				s01= ho3[1]/ho3[3] - ho1[1]/ho1[3];
				s10= ho3[0]/ho3[3] - ho2[0]/ho2[3];
				s11= ho3[1]/ho3[3] - ho2[1]/ho2[3];
				
				detsh= s00*s11-s10*s01;
				s00/= detsh; s01/=detsh; 
				s10/=detsh; s11/=detsh;
				
				/* recalc u and v again */
				hox= x/Zmulx -1.0f;
				hoy= y/Zmuly -1.0f;
				u= (hox - ho3[0]/ho3[3])*s11 - (hoy - ho3[1]/ho3[3])*s10;
				v= (hoy - ho3[1]/ho3[3])*s00 - (hox - ho3[0]/ho3[3])*s01;
				l= 1.0f+u+v;
				
				shi->sticky[0]= l*s3[0]-u*s1[0]-v*s2[0];
				shi->sticky[1]= l*s3[1]-u*s1[1]-v*s2[1];
				shi->sticky[2]= 0.0f;
				
				if(shi->osatex) {
					float dxuv[2], dyuv[2];
					dxuv[0]=  s11/Zmulx;
					dxuv[1]=  - s01/Zmulx;
					dyuv[0]=  - s10/Zmuly;
					dyuv[1]=  s00/Zmuly;
					
					dl= dxuv[0] + dxuv[1];
					shi->dxsticky[0]= dl*s3[0] - dxuv[0]*s1[0] - dxuv[1]*s2[0];
					shi->dxsticky[1]= dl*s3[1] - dxuv[0]*s1[1] - dxuv[1]*s2[1];
					dl= dyuv[0] + dyuv[1];
					shi->dysticky[0]= dl*s3[0] - dyuv[0]*s1[0] - dyuv[1]*s2[0];
					shi->dysticky[1]= dl*s3[1] - dyuv[0]*s1[1] - dyuv[1]*s2[1];
				}
			}
		}
	} /* else {
	 Note! For raytracing winco is not set, important because thus means all shader input's need to have their variables set to zero else in-initialized values are used
	*/
	if (R.r.color_mgt_flag & R_COLOR_MANAGEMENT) {
		if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)) {
			color_manage_linearize(shi->vcol, shi->vcol);
		}
	}
	
}

/* ****************** ShadeSample ************************************** */

/* initialize per part, not per pixel! */
void shade_input_initialize(ShadeInput *shi, RenderPart *pa, RenderLayer *rl, int sample)
{
	
	memset(shi, 0, sizeof(ShadeInput));
	
	shi->sample= sample;
	shi->thread= pa->thread;
	shi->do_preview= R.r.scemode & R_NODE_PREVIEW;
	shi->lay= rl->lay;
	shi->layflag= rl->layflag;
	shi->passflag= rl->passflag;
	shi->combinedflag= ~rl->pass_xor;
	shi->mat_override= rl->mat_override;
	shi->light_override= rl->light_override;
//	shi->rl= rl;
	/* note shi.depth==0  means first hit, not raytracing */
	
}

/* initialize per part, not per pixel! */
void shade_sample_initialize(ShadeSample *ssamp, RenderPart *pa, RenderLayer *rl)
{
	int a, tot;
	
	tot= R.osa==0?1:R.osa;
	
	for(a=0; a<tot; a++) {
		shade_input_initialize(&ssamp->shi[a], pa, rl, a);
		memset(&ssamp->shr[a], 0, sizeof(ShadeResult));
	}
	
	get_sample_layers(pa, rl, ssamp->rlpp);
}

/* Do AO or (future) GI */
void shade_samples_do_AO(ShadeSample *ssamp)
{
	ShadeInput *shi;
	int sample;
	
	if(!(R.r.mode & R_SHADOW))
		return;
	if(!(R.r.mode & R_RAYTRACE) && !(R.wrld.ao_gather_method == WO_AOGATHER_APPROX))
		return;
	
	if(R.wrld.mode & WO_AMB_OCC) {
		shi= &ssamp->shi[0];

		if(((shi->passflag & SCE_PASS_COMBINED) && (shi->combinedflag & SCE_PASS_AO))
			|| (shi->passflag & SCE_PASS_AO))
			for(sample=0, shi= ssamp->shi; sample<ssamp->tot; shi++, sample++)
				if(!(shi->mode & MA_SHLESS))
					ambient_occlusion(shi);		/* stores in shi->ao[] */
	}
}


void shade_samples_fill_with_ps(ShadeSample *ssamp, PixStr *ps, int x, int y)
{
	ShadeInput *shi;
	float xs, ys;
	
	ssamp->tot= 0;
	
	for(shi= ssamp->shi; ps; ps= ps->next) {
		shade_input_set_triangle(shi, ps->obi, ps->facenr, 1);
		
		if(shi->vlr) {	/* NULL happens for env material or for 'all z' */
			unsigned short curmask= ps->mask;
			
			/* full osa is only set for OSA renders */
			if(shi->vlr->flag & R_FULL_OSA) {
				short shi_cp= 0, samp;
				
				for(samp=0; samp<R.osa; samp++) {
					if(curmask & (1<<samp)) {
						/* zbuffer has this inverse corrected, ensures xs,ys are inside pixel */
						xs= (float)x + R.jit[samp][0] + 0.5f;
						ys= (float)y + R.jit[samp][1] + 0.5f;
						
						if(shi_cp)
							shade_input_copy_triangle(shi, shi-1);
						
						shi->mask= (1<<samp);
//						shi->rl= ssamp->rlpp[samp];
						shi->samplenr= R.shadowsamplenr[shi->thread]++;	/* this counter is not being reset per pixel */
						shade_input_set_viewco(shi, x, y, xs, ys, (float)ps->z);
						shade_input_set_uv(shi);
						shade_input_set_normals(shi);
						
						shi_cp= 1;
						shi++;
					}
				}
			}
			else {
				if(R.osa) {
					short b= R.samples->centmask[curmask];
					xs= (float)x + R.samples->centLut[b & 15] + 0.5f;
					ys= (float)y + R.samples->centLut[b>>4] + 0.5f;
				}
				else {
					xs= (float)x + 0.5f;
					ys= (float)y + 0.5f;
				}

				shi->mask= curmask;
				shi->samplenr= R.shadowsamplenr[shi->thread]++;
				shade_input_set_viewco(shi, x, y, xs, ys, (float)ps->z);
				shade_input_set_uv(shi);
				shade_input_set_normals(shi);
				shi++;
			}
			
			/* total sample amount, shi->sample is static set in initialize */
			if(shi!=ssamp->shi)
				ssamp->tot= (shi-1)->sample + 1;
		}
	}
}

/* shades samples, returns true if anything happened */
int shade_samples(ShadeSample *ssamp, PixStr *ps, int x, int y)
{
	shade_samples_fill_with_ps(ssamp, ps, x, y);
	
	if(ssamp->tot) {
		ShadeInput *shi= ssamp->shi;
		ShadeResult *shr= ssamp->shr;
		int samp;
		
		/* if shadow or AO? */
		shade_samples_do_AO(ssamp);
		
		/* if shade (all shadepinputs have same passflag) */
		if(ssamp->shi[0].passflag & ~(SCE_PASS_Z|SCE_PASS_INDEXOB)) {

			for(samp=0; samp<ssamp->tot; samp++, shi++, shr++) {
				shade_input_set_shade_texco(shi);
				shade_input_do_shade(shi, shr);
			}
		}
		
		return 1;
	}
	return 0;
}

