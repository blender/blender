
/*  displist.c
 * 
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"

#include "DNA_texture_types.h"
#include "DNA_meta_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_listBase.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_view3d_types.h"
#include "DNA_lattice_types.h"
#include "DNA_key_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_displist.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_world.h"
#include "BKE_mesh.h"
#include "BKE_effect.h"
#include "BKE_mball.h"
#include "BKE_material.h"
#include "BKE_curve.h"
#include "BKE_key.h"
#include "BKE_anim.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_library.h"
#include "BKE_font.h"
#include "BKE_lattice.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_modifier.h"

#include "nla.h" /* For __NLA: Please do not remove yet */
#include "render.h"


/***/

typedef struct _FastLamp FastLamp;
struct _FastLamp {
	FastLamp *next;
	
	short type, mode, lay, rt;
	float co[3];
	float vec[3];
	float dist, distkw, att1, att2, spotsi, spotbl, r, g, b;
};

/***/

static void boundbox_displist(Object *ob);

static FastLamp *fastlamplist= NULL;
static float fviewmat[4][4];

void displistmesh_free(DispListMesh *dlm) 
{
	// also check on mvert and mface, can be NULL after decimator (ton)
	if (!dlm->dontFreeVerts && dlm->mvert) MEM_freeN(dlm->mvert);
	if (!dlm->dontFreeNors && dlm->nors) MEM_freeN(dlm->nors);
	if (!dlm->dontFreeOther) {
		if (dlm->medge) MEM_freeN(dlm->medge);
		if (dlm->mface) MEM_freeN(dlm->mface);
		if (dlm->mcol) MEM_freeN(dlm->mcol);
		if (dlm->tface) MEM_freeN(dlm->tface);
	}
	MEM_freeN(dlm);
}

DispListMesh *displistmesh_copy(DispListMesh *odlm) 
{
	DispListMesh *ndlm= MEM_dupallocN(odlm);
	ndlm->mvert= MEM_dupallocN(odlm->mvert);
	if (odlm->medge) ndlm->medge= MEM_dupallocN(odlm->medge);
	ndlm->mface= MEM_dupallocN(odlm->mface);
	if (odlm->nors) ndlm->nors = MEM_dupallocN(odlm->nors);
	if (odlm->mcol) ndlm->mcol= MEM_dupallocN(odlm->mcol);
	if (odlm->tface) ndlm->tface= MEM_dupallocN(odlm->tface);

	return ndlm;
}

DispListMesh *displistmesh_copyShared(DispListMesh *odlm) 
{
	DispListMesh *ndlm= MEM_dupallocN(odlm);
	ndlm->dontFreeNors = ndlm->dontFreeOther = ndlm->dontFreeVerts = 1;
	
	return ndlm;
}

void displistmesh_to_mesh(DispListMesh *dlm, Mesh *me) 
{
		/* We assume, rather courageously, that any
		 * shared data came from the mesh itself and so
		 * we can ignore the dlm->dontFreeOther flag.
		 */

	if (me->mvert && dlm->mvert!=me->mvert) MEM_freeN(me->mvert);
	if (me->mface && dlm->mface!=me->mface) MEM_freeN(me->mface);
	if (me->tface && dlm->tface!=me->tface) MEM_freeN(me->tface);
	if (me->mcol && dlm->mcol!=me->mcol) MEM_freeN(me->mcol);
	if (me->medge && dlm->medge!=me->medge) MEM_freeN(me->medge);

	me->tface = NULL;
	me->mcol = NULL;
	me->medge = NULL;

	if (dlm->totvert!=me->totvert) {
		if (me->msticky) MEM_freeN(me->msticky);
		me->msticky = NULL;

		if (me->dvert) free_dverts(me->dvert, me->totvert);
		me->dvert = NULL;

		if(me->key) me->key->id.us--;
		me->key = NULL;
	}

	me->totface= dlm->totface;
	me->totvert= dlm->totvert;
	me->totedge= 0;

	me->mvert= dlm->mvert;
	me->mface= dlm->mface;
	if (dlm->tface)
		me->tface= dlm->tface;
	if (dlm->mcol)
		me->mcol= dlm->mcol;

	if(dlm->medge) {
		me->totedge= dlm->totedge;
		me->medge= dlm->medge;
	}

	if (dlm->nors && !dlm->dontFreeNors) MEM_freeN(dlm->nors);

	MEM_freeN(dlm);
}

void free_disp_elem(DispList *dl)
{
	if(dl) {
		if(dl->verts) MEM_freeN(dl->verts);
		if(dl->nors) MEM_freeN(dl->nors);
		if(dl->index) MEM_freeN(dl->index);
		if(dl->col1) MEM_freeN(dl->col1);
		if(dl->col2) MEM_freeN(dl->col2);
		if(dl->bevelSplitFlag) MEM_freeN(dl->bevelSplitFlag);
		MEM_freeN(dl);
	}
}

void freedisplist(ListBase *lb)
{
	DispList *dl;

	dl= lb->first;
	while(dl) {
		BLI_remlink(lb, dl);
		free_disp_elem(dl);
		dl= lb->first;
	}
}

DispList *find_displist_create(ListBase *lb, int type)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		if(dl->type==type) return dl;
		dl= dl->next;
	}

	dl= MEM_callocN(sizeof(DispList), "find_disp");
	dl->type= type;
	BLI_addtail(lb, dl);

	return dl;
}

DispList *find_displist(ListBase *lb, int type)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		if(dl->type==type) return dl;
		dl= dl->next;
	}

	return 0;
}

int displist_has_faces(ListBase *lb)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		if ELEM4(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF, DL_POLY)
			return 1;
		dl= dl->next;
	}
	return 0;
}

void copy_displist(ListBase *lbn, ListBase *lb)
{
	DispList *dln, *dl;
	
	lbn->first= lbn->last= 0;
	
	dl= lb->first;
	while(dl) {
		
		dln= MEM_dupallocN(dl);
		BLI_addtail(lbn, dln);
		dln->verts= MEM_dupallocN(dl->verts);
		dln->nors= MEM_dupallocN(dl->nors);
		dln->index= MEM_dupallocN(dl->index);
		dln->col1= MEM_dupallocN(dl->col1);
		dln->col2= MEM_dupallocN(dl->col2);
		
		dl= dl->next;
	}
}

static void initfastshade(void)
{
	Base *base;
	Object *ob;
	Lamp *la;
	FastLamp *fl;
	float mat[4][4];

	init_render_world();
	
	if(fastlamplist) return;
	if(G.scene->camera==0) G.scene->camera= scene_find_camera(G.scene);
	if(G.scene->camera==0) return;

	/* copied from 'roteerscene' (does that function still exist? (ton) */
	where_is_object(G.scene->camera);
	Mat4CpyMat4(R.viewinv, G.scene->camera->obmat);
	Mat4Ortho(R.viewinv);
	Mat4Invert(fviewmat, R.viewinv);

	/* initrendertexture(); */

	base= G.scene->base.first;
	while(base) {
		ob= base->object;
		if( ob->type==OB_LAMP && (base->lay & G.scene->lay)) {
			
			Mat4MulMat4(mat, ob->obmat, fviewmat);
			
			la= ob->data;
			fl= MEM_mallocN(sizeof(FastLamp), "initfastshade2");

			fl->next= fastlamplist;
			fastlamplist= fl;

			fl->type= la->type;
			fl->mode= la->mode;
			fl->lay= base->lay;
			
			fl->vec[0]= mat[2][0];
			fl->vec[1]= mat[2][1];
			fl->vec[2]= mat[2][2];
			Normalise(fl->vec);
			
			fl->co[0]= mat[3][0];
			fl->co[1]= mat[3][1];
			fl->co[2]= mat[3][2];

			fl->dist= la->dist;
			fl->distkw= fl->dist*fl->dist;
			fl->att1= la->att1;
			fl->att2= la->att2;

			fl->spotsi= (float)cos( M_PI*la->spotsize/360.0 );
			fl->spotbl= (1.0f-fl->spotsi)*la->spotblend;
	
			fl->r= la->energy*la->r;
			fl->g= la->energy*la->g;
			fl->b= la->energy*la->b;
		}
		
		if(base->next==0 && G.scene->set && base==G.scene->base.last) base= G.scene->set->base.first;
		else base= base->next;
	}
}


void freefastshade()
{
	while (fastlamplist) {
		FastLamp *fl= fastlamplist;
		fastlamplist= fl->next;
		
		MEM_freeN(fl);
	}
}


static void fastshade(float *co, float *nor, float *orco, Material *ma, char *col1, char *col2, char *vertcol)
{
	ShadeInput shi;
	FastLamp *fl;
	float i, t, inp, is, soft,  lv[3], lampdist, ld;
	float diff1[3], diff2[3];
	float isr1=0, isg1=0, isb1=0, isr=0, isg=0, isb=0;
	int a, back;

	if(ma==0) return;
	
	shi.mat= ma;
	shi.vlr= NULL;	// have to do this!
	
	// copy all relevant material vars, note, keep this synced with render_types.h
	memcpy(&shi.r, &shi.mat->r, 23*sizeof(float));
	// set special cases:
	shi.har= shi.mat->har;
	
	shi.osatex= 0;  // also prevents reading vlr
	
	VECCOPY(shi.vn, nor);
	
	if(ma->mode & MA_VERTEXCOLP) {
		if(vertcol) {
			shi.r= vertcol[3]/255.0;
			shi.g= vertcol[2]/255.0;
			shi.b= vertcol[1]/255.0;
		}
	}
	
	if(ma->texco) {
		VECCOPY(shi.lo, orco);
		
		if(ma->texco & TEXCO_GLOB) {
			VECCOPY(shi.gl, shi.lo);
		}
		if(ma->texco & TEXCO_WINDOW) {
			VECCOPY(shi.winco, shi.lo);
		}
		if(ma->texco & TEXCO_STICKY) {
			VECCOPY(shi.sticky, shi.lo);
		}
		if(ma->texco & TEXCO_UV) {
			VECCOPY(shi.uv, shi.lo);
		}
		if(ma->texco & TEXCO_OBJECT) {
			VECCOPY(shi.co, shi.lo);
		}
		if(ma->texco & TEXCO_NORM) {
			VECCOPY(shi.orn, shi.vn);
		}
		if(ma->texco & TEXCO_REFL) {
			
			inp= 2.0*(shi.vn[2]);
			shi.ref[0]= (inp*shi.vn[0]);
			shi.ref[1]= (inp*shi.vn[1]);
			shi.ref[2]= (-1.0+inp*shi.vn[2]);
		}

		do_material_tex(&shi);
	}

	if(ma->mode & MA_SHLESS) {
		if(vertcol && (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP))== MA_VERTEXCOL ) {
			float fac;
			fac= vertcol[3]*shi.r;
			col1[3]= fac>=1.0?255:(char)fac;
			fac= vertcol[2]*shi.g;
			col1[2]= fac>=1.0?255:(char)fac;
			fac= vertcol[1]*shi.b;
			col1[1]= fac>=1.0?255:(char)fac;
		}
		else {
			int fac;
			fac= (int) (255.0*shi.r);
			col1[3]= fac>255?255:(char)fac;
			fac= (int) (255.0*shi.g);
			col1[2]= fac>255?255:(char)fac;
			fac= (int) (255.0*shi.b);
			col1[1]= fac>255?255:(char)fac;
		}
		if(col2) {
			col2[3]= col1[3];
			col2[2]= col1[2];
			col2[1]= col1[1];
		}
		return;
	}

	if( vertcol && (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP))== MA_VERTEXCOL ) {
		diff1[0]= diff2[0]= shi.r*(shi.emit+vertcol[3]/255.0);
		diff1[1]= diff2[1]= shi.g*(shi.emit+vertcol[2]/255.0);
		diff1[2]= diff2[2]= shi.b*(shi.emit+vertcol[1]/255.0);
	}
	else {
		diff1[0]= diff2[0]= shi.r*shi.emit;
		diff1[1]= diff2[1]= shi.g*shi.emit;
		diff1[2]= diff2[2]= shi.b*shi.emit;
	}
	
	shi.view[0]= 0.0;
	shi.view[1]= 0.0;
	shi.view[2]= 1.0;
	
	Normalise(shi.view);

	for (fl= fastlamplist; fl; fl= fl->next) {
		/* if(fl->mode & LA_LAYER) if((fl->lay & ma->lay)==0) continue; */

		if(fl->type==LA_SUN || fl->type==LA_HEMI) {
			VECCOPY(lv, fl->vec);
			lampdist= 1.0;
		}
		else {
			lv[0]= fl->co[0] - co[0];
			lv[1]= fl->co[1] - co[1];
			lv[2]= fl->co[2] - co[2];
			ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
			lv[0]/=ld;
			lv[1]/=ld;
			lv[2]/=ld;

			if(fl->mode & LA_QUAD) {
				t= 1.0;
				if(fl->att1>0.0)
					t= fl->dist/(fl->dist+fl->att1*ld);
				if(fl->att2>0.0)
					t*= fl->distkw/(fl->distkw+fl->att2*ld*ld);

				lampdist= t;
			}
			else {
				lampdist= (fl->dist/(fl->dist+ld));
			}

			if(fl->mode & LA_SPHERE) {
				t= fl->dist - ld;
				if(t<0.0) continue;
				
				t/= fl->dist;
				lampdist*= (t);
			}
		}

		if(fl->type==LA_SPOT) {
			inp= lv[0]*fl->vec[0]+lv[1]*fl->vec[1]+lv[2]*fl->vec[2];
			if(inp<fl->spotsi) continue;
			else {
				t= inp-fl->spotsi;
				i= 1.0;
				soft= 1.0;
				if(t<fl->spotbl && fl->spotbl!=0.0) {
					/* soft area */
					i= t/fl->spotbl;
					t= i*i;
					soft= (3.0*t-2.0*t*i);
					inp*= soft;
				}

				lampdist*=inp;
			}
		}

		if(fl->mode & LA_NO_DIFF) is= 0.0;
		else {
			is= nor[0]*lv[0]+ nor[1]*lv[1]+ nor[2]*lv[2];
	
			if(ma->diff_shader==MA_DIFF_ORENNAYAR) is= OrenNayar_Diff(nor, lv, shi.view, ma->roughness);
			else if(ma->diff_shader==MA_DIFF_TOON) is= Toon_Diff(nor, lv, shi.view, ma->param[0], ma->param[1]);
			else if(ma->diff_shader==MA_DIFF_MINNAERT) is= Minnaert_Diff(is, nor, shi.view, ma->darkness);
		}
		
		back= 0;
		if(is<0.0) {
			back= 1;
			is= -is;
		}
		inp= is*lampdist*shi.refl;

		if(back==0) {
			add_to_diffuse(diff1, &shi, is, inp*fl->r, inp*fl->g, inp*fl->b);
			//diff1[0]+= inp*fl->r;
			//diff1[1]+= inp*fl->g;
			//diff1[2]+= inp*fl->b;
		} else if(col2) {
			add_to_diffuse(diff2, &shi, is, inp*fl->r, inp*fl->g, inp*fl->b);
			//diff2[0]+= inp*fl->r;
			//diff2[1]+= inp*fl->g;
			//diff2[2]+= inp*fl->b;
		}
		if(shi.spec!=0.0 && (fl->mode & LA_NO_SPEC)==0) {
			float specfac;
			
			if(ma->spec_shader==MA_SPEC_PHONG) 
				specfac= Phong_Spec(nor, lv, shi.view, shi.har, 0);
			else if(ma->spec_shader==MA_SPEC_COOKTORR) 
				specfac= CookTorr_Spec(nor, lv, shi.view, shi.har, 0);
			else if(ma->spec_shader==MA_SPEC_BLINN) 
				specfac= Blinn_Spec(nor, lv, shi.view, ma->refrac, (float)shi.har, 0);
			else if(ma->spec_shader==MA_SPEC_WARDISO)
				specfac= WardIso_Spec(nor, lv, shi.view, ma->rms, 0);
			else 
				specfac= Toon_Spec(nor, lv, shi.view, ma->param[2], ma->param[3], 0);
			
			if(specfac>0) {
				t= specfac*shi.spec*lampdist;
				if(back==0) {
					if(ma->mode & MA_RAMP_SPEC) {
						float spec[3];
						do_specular_ramp(&shi, specfac, t, spec);
						isr+= t*(fl->r * spec[0]);
						isg+= t*(fl->g * spec[1]);
						isb+= t*(fl->b * spec[2]);
					}
					else {
						isr+= t*(fl->r * shi.specr);
						isg+= t*(fl->g * shi.specg);
						isb+= t*(fl->b * shi.specb);
					}
				}
				else if(col2) {
					if(ma->mode & MA_RAMP_SPEC) {
						float spec[3];
						do_specular_ramp(&shi, specfac, t, spec);
						isr1+= t*(fl->r * spec[0]);
						isg1+= t*(fl->g * spec[1]);
						isb1+= t*(fl->b * spec[2]);
					}
					else {
						isr1+= t*(fl->r * shi.specr);
						isg1+= t*(fl->g * shi.specg);
						isb1+= t*(fl->b * shi.specb);
					}
				}
			}
		}

	}

	if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(diff1, &shi);
	if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(&isr, &isg, &isb, &shi);

	a= 256*(diff1[0] + shi.ambr +isr);
	if(a>255) col1[3]= 255; 
	else col1[3]= a;
	a= 256*(diff1[1] + shi.ambg +isg);
	if(a>255) col1[2]= 255; 
	else col1[2]= a;
	a= 256*(diff1[2] + shi.ambb +isb);
	if(a>255) col1[1]= 255; 
	else col1[1]= a;

	if(col2) {
		if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(diff2, &shi);
		if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(&isr1, &isg1, &isb1, &shi);
		
		a= 256*(diff2[0] + shi.ambr +isr1);
		if(a>255) col2[3]= 255; 
		else col2[3]= a;
		a= 256*(diff2[1] + shi.ambg +isg1);
		if(a>255) col2[2]= 255; 
		else col2[2]= a;
		a= 256*(diff2[2] + shi.ambb +isb1);
		if(a>255) col2[1]= 255; 
		else col2[1]= a;
	}

}

void addnormalsDispList(Object *ob, ListBase *lb)
{
	DispList *dl = NULL;
	float *vdata, *ndata, nor[3];
	float *v1, *v2, *v3, *v4;
	float *n1, *n2, *n3, *n4;
	int a, b, p1, p2, p3, p4;


	dl= lb->first;
	
	while(dl) {
		if(dl->type==DL_INDEX3) {
			if(dl->nors==0) {
				dl->nors= MEM_callocN(sizeof(float)*3, "dlnors");
				if(dl->verts[2]<0.0) dl->nors[2]= -1.0;
				else dl->nors[2]= 1.0;
			}
		}
		else if(dl->type==DL_SURF) {
			if(dl->nors==0) {
				dl->nors= MEM_callocN(sizeof(float)*3*dl->nr*dl->parts, "dlnors");
				
				vdata= dl->verts;
				ndata= dl->nors;
				
				for(a=0; a<dl->parts; a++) {
	
					DL_SURFINDEX(dl->flag & DL_CYCL_U, dl->flag & DL_CYCL_V, dl->nr, dl->parts);
	
					v1= vdata+ 3*p1; 
					n1= ndata+ 3*p1;
					v2= vdata+ 3*p2; 
					n2= ndata+ 3*p2;
					v3= vdata+ 3*p3; 
					n3= ndata+ 3*p3;
					v4= vdata+ 3*p4; 
					n4= ndata+ 3*p4;
					
					for(; b<dl->nr; b++) {
	
						CalcNormFloat4(v1, v3, v4, v2, nor);
	
						VecAddf(n1, n1, nor);
						VecAddf(n2, n2, nor);
						VecAddf(n3, n3, nor);
						VecAddf(n4, n4, nor);
	
						v2= v1; v1+= 3;
						v4= v3; v3+= 3;
						n2= n1; n1+= 3;
						n4= n3; n3+= 3;
					}
				}
				a= dl->parts*dl->nr;
				v1= ndata;
				while(a--) {
					Normalise(v1);
					v1+= 3;
				}
			}
		}
		dl= dl->next;
	}
}

static void init_fastshade_for_ob(Object *ob, int *need_orco_r, float mat[4][4], float imat[3][3])
{
	float tmat[4][4];
	int a;

	initfastshade();

	Mat4MulMat4(mat, ob->obmat, fviewmat);
	
	Mat4Invert(tmat, mat);
	Mat3CpyMat4(imat, tmat);
	if(ob->transflag & OB_NEG_SCALE) Mat3MulFloat((float *)imat, -1.0);

	if (need_orco_r) *need_orco_r= 0;
	for(a=0; a<ob->totcol; a++) {
		Material *ma= give_current_material(ob, a+1);
		if(ma) {
			init_render_material(ma);
			if(ma->texco & TEXCO_ORCO) {
				if (need_orco_r) *need_orco_r= 1;
			}
		}
	}
}
static void end_fastshade_for_ob(Object *ob)
{
	int a;

	for(a=0; a<ob->totcol; a++) {
		Material *ma= give_current_material(ob, a+1);
		if(ma) end_render_material(ma);
	}
}

void mesh_create_shadedColors(Object *ob, int onlyForMesh, unsigned int **col1_r, unsigned int **col2_r)
{
	Mesh *me= ob->data;
	int dmNeedsFree;
	DerivedMesh *dm;
	DispListMesh *dlm;
	unsigned int *col1, *col2;
	float *orco, *vnors, imat[3][3], mat[4][4], vec[3];
	int a, i, need_orco;

	init_fastshade_for_ob(ob, &need_orco, mat, imat);

	if (need_orco) {
		orco = mesh_create_orco(ob);
	} else {
		orco = NULL;
	}

	if (onlyForMesh) {
		dm = mesh_get_derived_deform(ob, &dmNeedsFree);
	} else {
		dm = mesh_get_derived_final(ob, &dmNeedsFree);
	}
	dlm= dm->convertToDispListMesh(dm, 1);

	col1 = MEM_mallocN(sizeof(*col1)*dlm->totface*4, "col1");
	if (col2_r && (me->flag & ME_TWOSIDED)) {
		col2 = MEM_mallocN(sizeof(*col2)*dlm->totface*4, "col1");
	} else {
		col2 = NULL;
	}
	
	*col1_r = col1;
	if (col2_r) *col2_r = col2;

		/* vertexnormals */
	vnors= MEM_mallocN(dlm->totvert*3*sizeof(float), "vnors disp");
	for (a=0; a<dlm->totvert; a++) {
		MVert *mv = &dlm->mvert[a];
		float *vn= &vnors[a*3];
		float xn= mv->no[0]; 
		float yn= mv->no[1]; 
		float zn= mv->no[2];
		
			/* transpose ! */
		vn[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		vn[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		vn[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		Normalise(vn);
	}		

	for (i=0; i<dlm->totface; i++) {
		MFace *mf= &dlm->mface[i];
		int j, vidx[4], nverts= mf->v4?4:3;
		unsigned char *col1base= (unsigned char*) &col1[i*4];
		unsigned char *col2base= (unsigned char*) (col2?&col2[i*4]:NULL);
		unsigned char *mcolbase;
		Material *ma= give_current_material(ob, mf->mat_nr+1);
		float nor[3], n1[3];
		
		if(ma==0) ma= &defmaterial;
		
		if (dlm->tface) {
			mcolbase = (unsigned char*) dlm->tface[i].col;
		} else if (dlm->mcol) {
			mcolbase = (unsigned char*) &dlm->mcol[i*4];
		} else {
			mcolbase = NULL;
		}

		vidx[0]= mf->v1;
		vidx[1]= mf->v2;
		vidx[2]= mf->v3;
		vidx[3]= mf->v4;

		if (dlm->nors) {
			VECCOPY(nor, &dlm->nors[i*3]);
		} else {
			if (mf->v4)
				CalcNormFloat4(dlm->mvert[mf->v1].co, dlm->mvert[mf->v2].co, dlm->mvert[mf->v3].co, dlm->mvert[mf->v4].co, nor);
			else
				CalcNormFloat(dlm->mvert[mf->v1].co, dlm->mvert[mf->v2].co, dlm->mvert[mf->v3].co, nor);
		}

		n1[0]= imat[0][0]*nor[0]+imat[0][1]*nor[1]+imat[0][2]*nor[2];
		n1[1]= imat[1][0]*nor[0]+imat[1][1]*nor[1]+imat[1][2]*nor[2];
		n1[2]= imat[2][0]*nor[0]+imat[2][1]*nor[1]+imat[2][2]*nor[2];
		Normalise(n1);

		for (j=0; j<nverts; j++) {
			MVert *mv= &dlm->mvert[vidx[j]];
			unsigned char *col1= &col1base[j*4];
			unsigned char *col2= col2base?&col2base[j*4]:NULL;
			unsigned char *mcol= mcolbase?&mcolbase[j*4]:NULL;
			float *vn = (mf->flag & ME_SMOOTH)?&vnors[3*vidx[j]]:n1;

			VECCOPY(vec, mv->co);
			Mat4MulVecfl(mat, vec);
			fastshade(vec, vn, orco?&orco[vidx[j]*3]:mv->co, ma, col1, col2, mcol);
		}
	}
	MEM_freeN(vnors);
	displistmesh_free(dlm);

	if (orco) {
		MEM_freeN(orco);
	}

	if (dmNeedsFree) dm->release(dm);

	end_fastshade_for_ob(ob);
}

void shadeDispList(Object *ob)
{
	DispList *dl, *dlob;
	Material *ma = NULL;
	Curve *cu;
	float imat[3][3], mat[4][4], vec[3];
	float *fp, *nor, n1[3];
	unsigned int *col1;
	int a;

	if(ob->flag & OB_FROMDUPLI) return;

	dl = find_displist(&ob->disp, DL_VERTCOL);
	if (dl) {
		BLI_remlink(&ob->disp, dl);
		free_disp_elem(dl);
	}

	if(ob->type==OB_MESH) {
		dl= MEM_callocN(sizeof(DispList), "displistshade");
		BLI_addtail(&ob->disp, dl);
		dl->type= DL_VERTCOL;

		mesh_create_shadedColors(ob, 0, &dl->col1, &dl->col2);

		return;
	}

	init_fastshade_for_ob(ob, NULL, mat, imat);
	
	if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
	
		/* now we need the normals */
		cu= ob->data;
		dl= cu->disp.first;
		
		while(dl) {
			dlob= MEM_callocN(sizeof(DispList), "displistshade");
			BLI_addtail(&ob->disp, dlob);
			dlob->type= DL_VERTCOL;
			dlob->parts= dl->parts;
			dlob->nr= dl->nr;
			
			if(dl->type==DL_INDEX3) {
				col1= dlob->col1= MEM_mallocN(sizeof(int)*dl->nr, "col1");
			}
			else {
				col1= dlob->col1= MEM_mallocN(sizeof(int)*dl->parts*dl->nr, "col1");
			}
			
		
			ma= give_current_material(ob, dl->col+1);
			if(ma==0) ma= &defmaterial;

			if(dl->type==DL_INDEX3) {
				if(dl->nors) {
					/* there's just one normal */
					n1[0]= imat[0][0]*dl->nors[0]+imat[0][1]*dl->nors[1]+imat[0][2]*dl->nors[2];
					n1[1]= imat[1][0]*dl->nors[0]+imat[1][1]*dl->nors[1]+imat[1][2]*dl->nors[2];
					n1[2]= imat[2][0]*dl->nors[0]+imat[2][1]*dl->nors[1]+imat[2][2]*dl->nors[2];
					Normalise(n1);
					
					fp= dl->verts;
					
					a= dl->nr;		
					while(a--) {
						VECCOPY(vec, fp);
						Mat4MulVecfl(mat, vec);
						
						fastshade(vec, n1, fp, ma, (char *)col1, 0, 0);
						
						fp+= 3; col1++;
					}
				}
			}
			else if(dl->type==DL_SURF) {
				if(dl->nors) {
					a= dl->nr*dl->parts;
					fp= dl->verts;
					nor= dl->nors;
					
					while(a--) {
						VECCOPY(vec, fp);
						Mat4MulVecfl(mat, vec);
						
						n1[0]= imat[0][0]*nor[0]+imat[0][1]*nor[1]+imat[0][2]*nor[2];
						n1[1]= imat[1][0]*nor[0]+imat[1][1]*nor[1]+imat[1][2]*nor[2];
						n1[2]= imat[2][0]*nor[0]+imat[2][1]*nor[1]+imat[2][2]*nor[2];
						Normalise(n1);
			
						fastshade(vec, n1, fp, ma, (char *)col1, 0, 0);
						
						fp+= 3; nor+= 3; col1++;
					}
				}
			}
			dl= dl->next;
		}
	}
	else if(ob->type==OB_MBALL) {
		/* there are normals already */
		dl= ob->disp.first;
		
		while(dl) {
			
			if(dl->type==DL_INDEX4) {
				if(dl->nors) {
					
					if(dl->col1) MEM_freeN(dl->col1);
					col1= dl->col1= MEM_mallocN(sizeof(int)*dl->nr, "col1");
			
					ma= give_current_material(ob, dl->col+1);
					if(ma==0) ma= &defmaterial;
	
					fp= dl->verts;
					nor= dl->nors;
					
					a= dl->nr;		
					while(a--) {
						VECCOPY(vec, fp);
						Mat4MulVecfl(mat, vec);
						
						/* transpose ! */
						n1[0]= imat[0][0]*nor[0]+imat[0][1]*nor[1]+imat[0][2]*nor[2];
						n1[1]= imat[1][0]*nor[0]+imat[1][1]*nor[1]+imat[1][2]*nor[2];
						n1[2]= imat[2][0]*nor[0]+imat[2][1]*nor[1]+imat[2][2]*nor[2];
						Normalise(n1);
					
						fastshade(vec, n1, fp, ma, (char *)col1, 0, 0);
						
						fp+= 3; col1++; nor+= 3;
					}
				}
			}
			dl= dl->next;
		}
	}
	
	end_fastshade_for_ob(ob);
}

void reshadeall_displist(void)
{
	Base *base;
	Object *ob;
	
	freefastshade();
	
	base= G.scene->base.first;
	while(base) {
		if(base->lay & G.scene->lay) {
			ob= base->object;
			
			/* Metaballs have standard displist at the Object */
			if(ob->type==OB_MBALL) shadeDispList(ob);
			else freedisplist(&ob->disp);
		}
		base= base->next;
	}
}

void count_displist(ListBase *lb, int *totvert, int *totface)
{
	DispList *dl;
	
	dl= lb->first;
	while(dl) {
		
		switch(dl->type) {
		case DL_SURF:
			*totvert+= dl->nr*dl->parts;
			*totface+= (dl->nr-1)*(dl->parts-1);
			break;
		case DL_INDEX3:
		case DL_INDEX4:
			*totvert+= dl->nr;
			*totface+= dl->parts;
			break;
		case DL_POLY:
		case DL_SEGM:
			*totvert+= dl->nr*dl->parts;
		}
		
		dl= dl->next;
	}
}

static void curve_to_displist(Curve *cu, ListBase *nubase, ListBase *dispbase)
{
	Nurb *nu;
	DispList *dl;
	BezTriple *bezt, *prevbezt;
	BPoint *bp;
	float *data, *v1, *v2;
	int a, len;
	
	nu= nubase->first;
	while(nu) {
		if(nu->hide==0) {
			if((nu->type & 7)==CU_BEZIER) {
				
				/* count */
				len= 0;
				a= nu->pntsu-1;
				if(nu->flagu & 1) a++;

				prevbezt= nu->bezt;
				bezt= prevbezt+1;
				while(a--) {
					if(a==0 && (nu->flagu & 1)) bezt= nu->bezt;
					
					if(prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) len++;
					else len+= nu->resolu;
					
					if(a==0 && (nu->flagu & 1)==0) len++;
					
					prevbezt= bezt;
					bezt++;
				}
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListbez");
				/* len+1 because of 'forward_diff_bezier' function */
				dl->verts= MEM_callocN( (len+1)*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx= nu->charidx;

				data= dl->verts;

				if(nu->flagu & 1) {
					dl->type= DL_POLY;
					a= nu->pntsu;
				}
				else {
					dl->type= DL_SEGM;
					a= nu->pntsu-1;
				}
				
				prevbezt= nu->bezt;
				bezt= prevbezt+1;
				
				while(a--) {
					if(a==0 && dl->type== DL_POLY) bezt= nu->bezt;
					
					if(prevbezt->h2==HD_VECT && bezt->h1==HD_VECT) {
						VECCOPY(data, prevbezt->vec[1]);
						data+= 3;
					}
					else {
						v1= prevbezt->vec[1];
						v2= bezt->vec[0];
						forward_diff_bezier(v1[0], v1[3], v2[0], v2[3], data, nu->resolu, 3);
						forward_diff_bezier(v1[1], v1[4], v2[1], v2[4], data+1, nu->resolu, 3);
						if((nu->type & 8)==0)
							forward_diff_bezier(v1[2], v1[5], v2[2], v2[5], data+2, nu->resolu, 3);
						data+= 3*nu->resolu;
					}
					
					if(a==0 && dl->type==DL_SEGM) {
						VECCOPY(data, bezt->vec[1]);
					}
					
					prevbezt= bezt;
					bezt++;
				}
			}
			else if((nu->type & 7)==CU_NURBS) {
				len= nu->pntsu*nu->resolu;
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;

				data= dl->verts;
				if(nu->flagu & 1) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				makeNurbcurve(nu, data, 3);
			}
			else if((nu->type & 7)==CU_POLY) {
				len= nu->pntsu;
				dl= MEM_callocN(sizeof(DispList), "makeDispListpoly");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->charidx = nu->charidx;

				data= dl->verts;
				if(nu->flagu & 1) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				
				a= len;
				bp= nu->bp;
				while(a--) {
					VECCOPY(data, bp->vec);
					bp++;
					data+= 3;
				}
			}
		}
		nu= nu->next;
	}
}


void filldisplist(ListBase *dispbase, ListBase *to)
{
	EditVert *eve, *v1, *vlast;
	EditFace *efa;
	DispList *dlnew=0, *dl;
	float *f1;
	int colnr=0, charidx=0, cont=1, tot, a, *index;
	long totvert;
	
	if(dispbase==0) return;
	if(dispbase->first==0) return;

	while(cont) {
		cont= 0;
		totvert=0;
		
		dl= dispbase->first;
		while(dl) {
	
			if(dl->type==DL_POLY) {
				if(charidx<dl->charidx) cont= 1;
				else if(charidx==dl->charidx) {
			
					colnr= dl->col;
					charidx= dl->charidx;
		
					/* make editverts and edges */
					f1= dl->verts;
					a= dl->nr;
					eve= v1= 0;
					
					while(a--) {
						vlast= eve;
						
						eve= BLI_addfillvert(f1);
						totvert++;
						
						if(vlast==0) v1= eve;
						else {
							BLI_addfilledge(vlast, eve);
						}
						f1+=3;
					}
				
					if(eve!=0 && v1!=0) {
						BLI_addfilledge(eve, v1);
					}
				}
			}
			dl= dl->next;
		}
		
		if(totvert && BLI_edgefill(0, (G.obedit && G.obedit->actcol)?(G.obedit->actcol-1):0)) {

			/* count faces  */
			tot= 0;
			efa= fillfacebase.first;
			while(efa) {
				tot++;
				efa= efa->next;
			}

			if(tot) {
				dlnew= MEM_callocN(sizeof(DispList), "filldisplist");
				dlnew->type= DL_INDEX3;
				dlnew->col= colnr;
				dlnew->nr= totvert;
				dlnew->parts= tot;

				dlnew->index= MEM_mallocN(tot*3*sizeof(int), "dlindex");
				dlnew->verts= MEM_mallocN(totvert*3*sizeof(float), "dlverts");
				
				/* vert data */
				f1= dlnew->verts;
				totvert= 0;
				eve= fillvertbase.first;
				while(eve) {
					VECCOPY(f1, eve->co);
					f1+= 3;
	
					/* index number */
					eve->vn= (EditVert *)totvert;
					totvert++;
					
					eve= eve->next;
				}
				
				/* index data */
				efa= fillfacebase.first;
				index= dlnew->index;
				while(efa) {
					index[0]= (long)efa->v1->vn;
					index[1]= (long)efa->v2->vn;
					index[2]= (long)efa->v3->vn;
					
					index+= 3;
					efa= efa->next;
				}
			}

			BLI_addhead(to, dlnew);
			
		}
		BLI_end_edgefill();

		charidx++;
	}
	
	/* do not free polys, needed for wireframe display */
	
}

static void bevels_to_filledpoly(Curve *cu, ListBase *dispbase)
{
	ListBase front, back;
	DispList *dl, *dlnew;
	float *fp, *fp1;
	int a, dpoly;
	
	front.first= front.last= back.first= back.last= 0;
	
	dl= dispbase->first;
	while(dl) {
		if(dl->type==DL_SURF) {
			if( (dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U)==0 ) {
				if( (cu->flag & CU_BACK) && (dl->flag & DL_BACK_CURVE) ) {
					dlnew= MEM_callocN(sizeof(DispList), "filldisp");
					BLI_addtail(&front, dlnew);
					dlnew->verts= fp1= MEM_mallocN(sizeof(float)*3*dl->parts, "filldisp1");
					dlnew->nr= dl->parts;
					dlnew->parts= 1;
					dlnew->type= DL_POLY;
					dlnew->col= dl->col;
					
					fp= dl->verts;
					dpoly= 3*dl->nr;
					
					a= dl->parts;
					while(a--) {
						VECCOPY(fp1, fp);
						fp1+= 3;
						fp+= dpoly;
					}
				}
				if( (cu->flag & CU_FRONT) && (dl->flag & DL_FRONT_CURVE) ) {
					dlnew= MEM_callocN(sizeof(DispList), "filldisp");
					BLI_addtail(&back, dlnew);
					dlnew->verts= fp1= MEM_mallocN(sizeof(float)*3*dl->parts, "filldisp1");
					dlnew->nr= dl->parts;
					dlnew->parts= 1;
					dlnew->type= DL_POLY;
					dlnew->col= dl->col;
					
					fp= dl->verts+3*(dl->nr-1);
					dpoly= 3*dl->nr;
					
					a= dl->parts;
					while(a--) {
						VECCOPY(fp1, fp);
						fp1+= 3;
						fp+= dpoly;
					}
				}
			}
		}
		dl= dl->next;
	}

	filldisplist(&front, dispbase);
	filldisplist(&back, dispbase);
	
	freedisplist(&front);
	freedisplist(&back);

	filldisplist(dispbase, dispbase);
	
}

void curve_to_filledpoly(Curve *cu, ListBase *nurb, ListBase *dispbase)
{
	if(cu->flag & CU_3D) return;

	if(dispbase->first && ((DispList*) dispbase->first)->type==DL_SURF) {
		bevels_to_filledpoly(cu, dispbase);
	}
	else {
		filldisplist(dispbase, dispbase);
	}
}


/* taper rules:
  - only 1 curve
  - first point left, last point right
  - based on subdivided points in original curve, not on points in taper curve (still)
*/
static float calc_taper(Object *taperobj, int cur, int tot)
{
	Curve *cu;
	DispList *dl;
	
	if(taperobj==NULL) return 1.0;
	
	cu= taperobj->data;
	dl= cu->disp.first;
	if(dl==NULL) {
		makeDispListCurveTypes(taperobj, 0);
		dl= cu->disp.first;
	}
	if(dl) {
		float fac= ((float)cur)/(float)(tot-1);
		float minx, dx, *fp;
		int a;
		
		/* horizontal size */
		minx= dl->verts[0];
		dx= dl->verts[3*(dl->nr-1)] - minx;
		if(dx>0.0) {
		
			fp= dl->verts;
			for(a=0; a<dl->nr; a++, fp+=3) {
				if( (fp[0]-minx)/dx >= fac) {
					/* interpolate with prev */
					if(a>0) {
						float fac1= (fp[-3]-minx)/dx;
						float fac2= (fp[0]-minx)/dx;
						if(fac1!=fac2)
							return fp[1]*(fac1-fac)/(fac1-fac2) + fp[-2]*(fac-fac2)/(fac1-fac2);
					}
					return fp[1];
				}
			}
			return fp[-2];	// last y coord
		}
	}
	
	return 1.0;
}

void makeDispListMBall(Object *ob)
{
	if(!ob || (ob->flag&OB_FROMDUPLI) || ob->type!=OB_MBALL) return;

	freedisplist(&(ob->disp));
	
	if(ob->type==OB_MBALL) {
		if(ob==find_basis_mball(ob)) {
			metaball_polygonize(ob);
			tex_space_mball(ob);

			object_deform_mball(ob);
		}
	}
	
	boundbox_displist(ob);
}

static ModifierData *curve_get_tesselate_point(Object *ob, int forRender, int editmode)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint;

	preTesselatePoint = NULL;
	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!(md->mode&(1<<forRender))) continue;
		if (editmode && !(md->mode&eModifierMode_Editmode)) continue;
		if (mti->isDisabled && mti->isDisabled(md)) continue;

		if (md->type==eModifierType_Hook) {
			preTesselatePoint  = md;
		}
	}

	return preTesselatePoint;
}

void curve_calc_modifiers_pre(Object *ob, ListBase *nurb, int forRender, float (**originalVerts_r)[3], float (**deformedVerts_r)[3], int *numVerts_r)
{
	int editmode = (!forRender && ob==G.obedit);
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint = curve_get_tesselate_point(ob, forRender, editmode);
	int numVerts = 0;
	float (*originalVerts)[3] = NULL;
	float (*deformedVerts)[3] = NULL;

	if(ob!=G.obedit && do_ob_key(ob)) {
		deformedVerts = curve_getVertexCos(ob->data, nurb, &numVerts);
		originalVerts = MEM_dupallocN(deformedVerts);
	}
	
	if (preTesselatePoint) {
		for (; md; md=md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			if (!(md->mode&(1<<forRender))) continue;
			if (editmode && !(md->mode&eModifierMode_Editmode)) continue;
			if (mti->isDisabled && mti->isDisabled(md)) continue;
			if (mti->type!=eModifierTypeType_OnlyDeform) continue;

			if (!deformedVerts) {
				deformedVerts = curve_getVertexCos(ob->data, nurb, &numVerts);
				originalVerts = MEM_dupallocN(deformedVerts);
			}
			mti->deformVerts(md, ob, NULL, deformedVerts, numVerts);

			if (md==preTesselatePoint)
				break;
		}
	}

	if (deformedVerts) {
		curve_applyVertexCos(ob->data, nurb, deformedVerts);
	}

	*originalVerts_r = originalVerts;
	*deformedVerts_r = deformedVerts;
	*numVerts_r = numVerts;
}

void curve_calc_modifiers_post(Object *ob, ListBase *nurb, ListBase *dispbase, int forRender, float (*originalVerts)[3], float (*deformedVerts)[3])
{
	int editmode = (!forRender && ob==G.obedit);
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ModifierData *preTesselatePoint = curve_get_tesselate_point(ob, forRender, editmode);
	DispList *dl;

	if (preTesselatePoint) {
		md = preTesselatePoint->next;
	}

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!(md->mode&(1<<forRender))) continue;
		if (editmode && !(md->mode&eModifierMode_Editmode)) continue;
		if (mti->isDisabled && mti->isDisabled(md)) continue;
		if (mti->type!=eModifierTypeType_OnlyDeform) continue;

		for (dl=dispbase->first; dl; dl=dl->next) {
			mti->deformVerts(md, ob, NULL, (float(*)[3]) dl->verts, (dl->type==DL_INDEX3)?dl->nr:dl->parts*dl->nr);
		}
	}

	if (deformedVerts) {
		curve_applyVertexCos(ob->data, nurb, originalVerts);
		MEM_freeN(originalVerts);
		MEM_freeN(deformedVerts);
	}
}

void makeDispListSurf(Object *ob, ListBase *dispbase, int forRender)
{
	ListBase *nubase;
	Nurb *nu;
	Curve *cu = ob->data;
	DispList *dl;
	float *data;
	int len;
	int numVerts;
	float (*originalVerts)[3];
	float (*deformedVerts)[3];
		
	if(!forRender && ob==G.obedit) {
		nubase= &editNurb;
	}
	else {
		nubase= &cu->nurb;
	}

	curve_calc_modifiers_pre(ob, nubase, forRender, &originalVerts, &deformedVerts, &numVerts);

	for (nu=nubase->first; nu; nu=nu->next) {
		if(forRender || nu->hide==0) {
			if(nu->pntsv==1) {
				len= nu->pntsu*nu->resolu;
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				
				BLI_addtail(dispbase, dl);
				dl->parts= 1;
				dl->nr= len;
				dl->col= nu->mat_nr;
				dl->rt= nu->flag;
				
				data= dl->verts;
				if(nu->flagu & 1) dl->type= DL_POLY;
				else dl->type= DL_SEGM;
				
				makeNurbcurve(nu, data, 3);
			}
			else {
				len= nu->resolu*nu->resolv;
				
				dl= MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts= MEM_callocN(len*3*sizeof(float), "dlverts");
				BLI_addtail(dispbase, dl);

				dl->col= nu->mat_nr;
				dl->rt= nu->flag;
				
				data= dl->verts;
				dl->type= DL_SURF;

				dl->parts= nu->resolu;	/* in reverse, because makeNurbfaces works that way */
				dl->nr= nu->resolv;
				if(nu->flagv & CU_CYCLIC) dl->flag|= DL_CYCL_U;	/* reverse too! */
				if(nu->flagu & CU_CYCLIC) dl->flag|= DL_CYCL_V;

				makeNurbfaces(nu, data, 0);
			}
		}
	}

	if (!forRender) {
		tex_space_curve(cu);
	}

	curve_calc_modifiers_post(ob, nubase, dispbase, forRender, originalVerts, deformedVerts);
}

void makeDispListCurveTypes(Object *ob, int forOrco)
{
	Curve *cu = ob->data;
	ListBase *dispbase;

	if((ob->flag&OB_FROMDUPLI) || !ELEM3(ob->type, OB_SURF, OB_CURVE, OB_FONT)) return;
	if(ob->flag & OB_FROMDUPLI) return;

	freedisplist(&(ob->disp));
	dispbase= &(cu->disp);
	freedisplist(dispbase);
	
	if(ob->type==OB_SURF) {
		makeDispListSurf(ob, dispbase, 0);
	}
	else if ELEM(ob->type, OB_CURVE, OB_FONT) {
		ListBase dlbev;
		float (*originalVerts)[3];
		float (*deformedVerts)[3];
		int obedit= (G.obedit && G.obedit->data==ob->data);
		ListBase *nubase = obedit?&editNurb:&cu->nurb;
		int numVerts;

		BLI_freelistN(&(cu->bev));
		
		if(cu->path) free_path(cu->path);
		cu->path= NULL;
		
		if(ob->type==OB_FONT) text_to_curve(ob, 0);
		
		if(!forOrco) curve_calc_modifiers_pre(ob, nubase, 0, &originalVerts, &deformedVerts, &numVerts);

		makeBevelList(ob);

			/* If curve has no bevel will return nothing */
		makebevelcurve(ob, &dlbev);

		if (!dlbev.first) {
			curve_to_displist(cu, nubase, dispbase);
		} else {
			float widfac= cu->width-1.0;
			BevList *bl= cu->bev.first;
			Nurb *nu= nubase->first;

			for (; bl && nu; bl=bl->next,nu=nu->next) {
				DispList *dlb;
				
				for (dlb=dlbev.first; dlb; dlb=dlb->next) {
					DispList *dl;
					float *fp1, *data;
					BevPoint *bevp;
					int a,b;

						/* for each part of the bevel use a separate displblock */
					dl= MEM_callocN(sizeof(DispList), "makeDispListbev1");
					dl->verts= data= MEM_callocN(3*sizeof(float)*dlb->nr*bl->nr, "dlverts");
					BLI_addtail(dispbase, dl);

					dl->type= DL_SURF;
					
					dl->flag= dlb->flag & (DL_FRONT_CURVE|DL_BACK_CURVE);
					if(dlb->type==DL_POLY) dl->flag |= DL_CYCL_U;
					if(bl->poly>=0) dl->flag |= DL_CYCL_V;
					
					dl->parts= bl->nr;
					dl->nr= dlb->nr;
					dl->col= nu->mat_nr;
					dl->rt= nu->flag;
					dl->bevelSplitFlag= MEM_callocN(sizeof(*dl->col2)*((bl->nr+0x1F)>>5), "col2");
					bevp= (BevPoint *)(bl+1);

						/* for each point of poly make a bevel piece */
					bevp= (BevPoint *)(bl+1);
					for(a=0; a<bl->nr; a++,bevp++) {
						float fac = calc_taper(cu->taperobj, a, bl->nr);
						
						if (bevp->f1) {
							dl->bevelSplitFlag[a>>5] |= 1<<(a&0x1F);
						}

							/* rotate bevel piece and write in data */
						fp1= dlb->verts;
						for (b=0; b<dlb->nr; b++,fp1+=3,data+=3) {
							if(cu->flag & CU_3D) {
								float vec[3];

								vec[0]= fp1[1]+widfac;
								vec[1]= fp1[2];
								vec[2]= 0.0;
								
								Mat3MulVecfl(bevp->mat, vec);
								
								data[0]= bevp->x+ fac*vec[0];
								data[1]= bevp->y+ fac*vec[1];
								data[2]= bevp->z+ fac*vec[2];
							}
							else {
								data[0]= bevp->x+ fac*(widfac+fp1[1])*bevp->sina;
								data[1]= bevp->y+ fac*(widfac+fp1[1])*bevp->cosa;
								data[2]= bevp->z+ fac*fp1[2];
							}
						}
					}
				}
			}

			freedisplist(&dlbev);
		}

		curve_to_filledpoly(cu, nubase, dispbase);

		if(cu->flag & CU_PATH) calc_curvepath(ob);

		if(!forOrco) curve_calc_modifiers_post(ob, nubase, &cu->disp, 0, originalVerts, deformedVerts);
		tex_space_curve(cu);
	}
	
	boundbox_displist(ob);
}

/*******************************/
/*****       OUTLINE       *****/
/*******************************/

typedef struct Sample{
	short x, y;
} Sample;

typedef struct Segment{
	/* coordinates */
	struct Segment * next, * prev;
	float co[2];
} Segment;



static int dflt_in_out(struct ImBuf * ibuf, int x, int y)
{
	unsigned char * rect;
	
	if (ibuf == 0) return (0);
	if (x < 0 || y < 0 || x >= ibuf->x || y >= ibuf->y || ibuf->rect == 0) return (-1);
	
	rect = (unsigned char *) (ibuf->rect + (y * ibuf->x) + x);
	if (rect[0] > 0x81) return (1);
	return(0);
}


static Sample * outline(struct ImBuf * ibuf,
				 int (*in_or_out)(struct ImBuf *, int, int))
{
	static int dirs[8][2] = {
		{-1,  0}, {-1,  1},	{0,  1}, {1,  1}, 
		{1,  0}, {1, -1}, {0, -1}, {-1, -1}
	};
	
	int dir, x, y, in, i;
	int count, sampcount;
	int startx = 0, starty = 0;
	Sample * samp, * oldsamp;
	
	/* input:
	 * 1 - image 
	 * 2 - pointer to function that defines which pixel 'in' or 'out' is
	 */
	
	if (ibuf == 0) return (0);
	if (ibuf->rect == 0) return (0);
	
	if (in_or_out == 0) in_or_out = dflt_in_out;
	in = in_or_out(ibuf, 0, 0);
	
	/* search for first transition, and continue from there */	
	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			if (in_or_out(ibuf, x, y) != in) {
				/* found first 'other' point !! */
				
				if (x != startx) dir = 0;
				else dir = 6;
				
				startx = x; starty = y;
				count = 1;
				sampcount = 2000;
				samp = MEM_mallocN(sampcount * sizeof(Sample), "wire_samples");
				
				do{
					samp[count].x = x; samp[count].y = y;
					count++;
					
					if (count >= sampcount) {
						oldsamp = samp;
						samp = MEM_mallocN(2 * sampcount * sizeof(Sample), "wire_samples");
						memcpy(samp, oldsamp, sampcount * sizeof(Sample));
						sampcount *= 2;
						MEM_freeN(oldsamp);
					}
					
					i = 0;
					while(in_or_out(ibuf, x + dirs[dir][0], y + dirs[dir][1]) == in) {
						dir = (dir + 1) & 0x7;
						if (i++ == 9) break;
					}
					
					if (i >= 8) {
						/* this has to be a loose point */
						break;
					}
					
					x += dirs[dir][0];
					y += dirs[dir][1];
					dir = (dir - 3) & 0x7;
				} while(x != startx || y != starty);
				
				if (i >= 8) {
					/* patch for loose points */
					MEM_freeN(samp);
				} else {
					count = count - 1;
					samp[0].x = count >> 16;
					samp[0].y = count;
					return(samp);
				}
			}
		}
	}
	/* printf("no transition \n"); */
	return(0);
}



/*******************************/
/*****      WIREFRAME      *****/
/*******************************/


static float DistToLine2D(short *v1, short *v2, short *v3)   /* using Hesse formula :NO LINE PIECE! */
{
	float a[2],deler;

	a[0] = v2[1]-v3[1];
	a[1] = v3[0]-v2[0];
	deler = sqrt(a[0]*a[0]+a[1]*a[1]);
	if(deler == 0.0) return 0;

	return fabs((v1[0]-v2[0])*a[0]+(v1[1]-v2[1])*a[1])/deler;

}

static float ComputeMaxShpError(Sample *samp, int first, int last, int *splitPoint)
    /* samp:  Array of digitized points	*/
    /* first, last:  Indices defining region	*/
    /* splitpoint:  Point of maximum error	*/
{
    int		i;
    float	maxDist;				/*  Maximum error		*/
    float	dist;					/*  Current error		*/
 
    *splitPoint = (last - first + 1) / 2;
    maxDist = 0.0;
	
    for (i = first + 1; i < last; i++) {				
		dist = DistToLine2D((short *)(samp+i), (short *)(samp+first), (short *)(samp+last));

		if (dist >= maxDist) {
	    	maxDist = dist;
	    	*splitPoint = i;
		}
    }

    return (maxDist);
}


static void FitPoly(Sample *samp, int first, int last, float shperr, ListBase *seglist)
    /* Samp: Array of digitized points */
    /* first,last: Indices of first and last pts in region */
    /* spherr: User-defined error squared	   */
{
    Segment	* seg;				/* Control points segment*/
    float	maxError;			/*  Maximum fitting error	 */
    int		splitPoint;			/*  Point to split point set at	 */
    int		nPts;				/*  Number of points in subset  */
	
    nPts = last - first + 1;

    /*  Use heuristic if region only has two points in it */

	seg = MEM_mallocN(sizeof(Segment), "wure_segment");

	seg->co[0] = samp[first].x;
	seg->co[1] = samp[first].y;
	
    if (nPts == 2) {
		BLI_addtail(seglist, seg);
		return;
    }

	maxError = ComputeMaxShpError(samp, first, last, &splitPoint);
	if (maxError < shperr) {
		BLI_addtail(seglist, seg);
		return;
	}
 	
    /* Fitting failed -- split at max error point and fit recursively */
	
    FitPoly(samp, first, splitPoint, shperr, seglist);
    FitPoly(samp, splitPoint, last, shperr, seglist);
	
	MEM_freeN(seg);
}


static void ibuf2wire(ListBase * wireframe, struct ImBuf * ibuf)
{
	int count;
	Sample * samp;
	
	/* first make a list of samples */
	
	samp = outline(ibuf, 0);
	if (samp == 0) return;
	
	count = (samp[0].x << 16) + samp[0].y;
	if (count) FitPoly(samp, 1, count, 1.0, wireframe); /* was 3.0. Frank */

	MEM_freeN(samp);
}



void imagestodisplist(void)
{
	Base *base;
	Object *ob;
	Material *ma;
	Tex *tex;
	Mesh *me;
	ListBase _wireframe, *wireframe;
	DispList *dl;
	Segment *seg;
	float *data, xfac, yfac, xsi, ysi, vec[3], dum;
	int tot;
	
	_wireframe.first= 0;
	_wireframe.last= 0;
	wireframe = &_wireframe;
	
	init_render_textures();
	
	base= G.scene->base.first;
	while(base) {
		if(( (base->flag & SELECT) && (base->lay & G.scene->lay) ) ) {
			if( base->object->type==OB_MESH) {
				ob= base->object;
				me= ob->data;
				
				ma= give_current_material(ob, 1);
	
				if(ma && ma->mtex[0] && ma->mtex[0]->tex) {
					tex= ma->mtex[0]->tex;
					
					/* this takes care of correct loading of new imbufs */
					externtex(ma->mtex[0], vec, &dum, &dum, &dum, &dum, &dum);
					
					if(tex->type==TEX_IMAGE && tex->ima && tex->ima->ibuf) {				
						
						ob->dtx |= OB_DRAWIMAGE;
						
						ibuf2wire(wireframe, tex->ima->ibuf);

						tot= 0;
						seg = wireframe->first;
						while (seg) {
							tot++;
							seg = seg->next;
						}
	
						if(tot) {
							float size[3];

							freedisplist(&(ob->disp));

							dl= MEM_callocN(sizeof(DispList), "makeDispListimage");
							dl->verts= MEM_callocN(3*sizeof(float)*tot, "dlverts");
							
							BLI_addtail(&(ob->disp), dl);
							dl->type= DL_POLY;
							dl->parts= 1;
							dl->nr= tot;
							
							xsi= 0.5*(tex->ima->ibuf->x);
							ysi= 0.5*(tex->ima->ibuf->y);

							mesh_get_texspace(me, NULL, NULL, size);
							xfac= size[0]/xsi;
							yfac= size[1]/ysi;
												
							data= dl->verts;
							seg = wireframe->first;
							while (seg) {
								data[0]= xfac*(seg->co[0]-xsi);
								data[1]= yfac*(seg->co[1]-ysi);
								data+= 3;
								seg = seg->next;
							}
							BLI_freelistN(wireframe);
						}
					}
				}
			}
		}
		base= base->next;
	}
	
	end_render_textures();
	
	allqueue(REDRAWVIEW3D, 0);
}

static void boundbox_displist(Object *ob)
{
	BoundBox *bb=0;
	float min[3], max[3];
	DispList *dl;
	float *vert;
	int a, tot=0;
	
	INIT_MINMAX(min, max);

	if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		Curve *cu= ob->data;

		if(cu->bb==0) cu->bb= MEM_callocN(sizeof(BoundBox), "boundbox");	
		bb= cu->bb;
		
		dl= cu->disp.first;

		while (dl) {
			if(dl->type==DL_INDEX3) tot= dl->nr;
			else tot= dl->nr*dl->parts;
			
			vert= dl->verts;
			for(a=0; a<tot; a++, vert+=3) {
				DO_MINMAX(vert, min, max);
			}

			dl= dl->next;
		}
	}
	
	if(bb) {
		boundbox_set_from_min_max(bb, min, max);
	}
}

void displistmesh_add_edges(DispListMesh *dlm)
{
	EdgeHash *eh = BLI_edgehash_new();
	EdgeHashIterator *ehi;
	int i;

	for (i=0; i<dlm->totface; i++) {
		MFace *mf = &dlm->mface[i];

		if (!BLI_edgehash_haskey(eh, mf->v1, mf->v2))
			BLI_edgehash_insert(eh, mf->v1, mf->v2, NULL);
		if (!BLI_edgehash_haskey(eh, mf->v2, mf->v3))
			BLI_edgehash_insert(eh, mf->v2, mf->v3, NULL);
		
		if (mf->v4) {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v4))
				BLI_edgehash_insert(eh, mf->v3, mf->v4, NULL);
			if (!BLI_edgehash_haskey(eh, mf->v4, mf->v1))
				BLI_edgehash_insert(eh, mf->v4, mf->v1, NULL);
		} else {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v1))
				BLI_edgehash_insert(eh, mf->v3, mf->v1, NULL);
		}
	}

	dlm->totedge = BLI_edgehash_size(eh);
	dlm->medge = MEM_callocN(dlm->totedge*sizeof(*dlm->medge), "medge");

	ehi = BLI_edgehashIterator_new(eh);
	for (i=0; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		MEdge *med = &dlm->medge[i++];

		BLI_edgehashIterator_getKey(ehi, &med->v1, &med->v2);

		med->flag = ME_EDGEDRAW|ME_EDGERENDER;
	}
	BLI_edgehashIterator_free(ehi);

	BLI_edgehash_free(eh, NULL);
}
