/* ipo.c
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "DNA_ika_types.h"
#include "DNA_sequence_types.h"
#include "DNA_camera_types.h"
#include "DNA_sound_types.h"
#include "DNA_lamp_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_action_types.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_curve.h"
#include "BKE_object.h"
#include "BKE_blender.h"
#include "BKE_ipo.h"
#include "BKE_constraint.h"
#include "BKE_mesh.h"

#define SMALL -1.0e-10

/* This array concept was meant to make sure that defines such as OB_LOC_X
   don't have to be enumerated, also for backward compatibility, future changes,
   and to enable it all can be accessed with a for-next loop.
*/

int co_ar[CO_TOTIPO]= {
	CO_ENFORCE
};

int ob_ar[OB_TOTIPO]= {
	OB_LOC_X, OB_LOC_Y, OB_LOC_Z, OB_DLOC_X, OB_DLOC_Y, OB_DLOC_Z, 
	OB_ROT_X, OB_ROT_Y, OB_ROT_Z, OB_DROT_X, OB_DROT_Y, OB_DROT_Z, 
	OB_SIZE_X, OB_SIZE_Y, OB_SIZE_Z, OB_DSIZE_X, OB_DSIZE_Y, OB_DSIZE_Z, 
	OB_LAY, OB_TIME, OB_EFF_X, OB_EFF_Y, OB_EFF_Z, OB_COL_A,
	OB_PD_FSTR, OB_PD_FFALL, OB_PD_SDAMP, OB_PD_RDAMP, OB_PD_PERM
};

int ac_ar[AC_TOTIPO]= {
	AC_LOC_X, AC_LOC_Y, AC_LOC_Z,  
	 AC_QUAT_W, AC_QUAT_X, AC_QUAT_Y, AC_QUAT_Z,
	AC_SIZE_X, AC_SIZE_Y, AC_SIZE_Z
};

int ma_ar[MA_TOTIPO]= {
	MA_COL_R, MA_COL_G, MA_COL_B, 
	MA_SPEC_R, MA_SPEC_G, MA_SPEC_B, 
	MA_MIR_R, MA_MIR_G, MA_MIR_B,
	MA_REF, MA_ALPHA, MA_EMIT, MA_AMB, 
	MA_SPEC, MA_HARD, MA_SPTR, MA_ANG, 
	MA_MODE, MA_HASIZE, 
	
	MA_MAP1+MAP_OFS_X, MA_MAP1+MAP_OFS_Y, MA_MAP1+MAP_OFS_Z, 
	MA_MAP1+MAP_SIZE_X, MA_MAP1+MAP_SIZE_Y, MA_MAP1+MAP_SIZE_Z, 
	MA_MAP1+MAP_R, MA_MAP1+MAP_G, MA_MAP1+MAP_B,
	MA_MAP1+MAP_DVAR, MA_MAP1+MAP_COLF, MA_MAP1+MAP_NORF, MA_MAP1+MAP_VARF
};

int seq_ar[SEQ_TOTIPO]= {
	SEQ_FAC1
};

int cu_ar[CU_TOTIPO]= {
	CU_SPEED
};

int key_ar[KEY_TOTIPO]= {
	KEY_SPEED, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

int wo_ar[WO_TOTIPO]= {
	WO_HOR_R, WO_HOR_G, WO_HOR_B, WO_ZEN_R, WO_ZEN_G, WO_ZEN_B, 
	WO_EXPOS, WO_MISI, WO_MISTDI, WO_MISTSTA, WO_MISTHI,
	WO_STAR_R, WO_STAR_G, WO_STAR_B, WO_STARDIST, WO_STARSIZE, 

	MA_MAP1+MAP_OFS_X, MA_MAP1+MAP_OFS_Y, MA_MAP1+MAP_OFS_Z, 
	MA_MAP1+MAP_SIZE_X, MA_MAP1+MAP_SIZE_Y, MA_MAP1+MAP_SIZE_Z, 
	MA_MAP1+MAP_R, MA_MAP1+MAP_G, MA_MAP1+MAP_B,
	MA_MAP1+MAP_DVAR, MA_MAP1+MAP_COLF, MA_MAP1+MAP_NORF, MA_MAP1+MAP_VARF
};

int la_ar[LA_TOTIPO]= {
	LA_ENERGY,  LA_COL_R, LA_COL_G,  LA_COL_B, 
	LA_DIST, LA_SPOTSI, LA_SPOTBL, 
	LA_QUAD1,  LA_QUAD2,  LA_HALOINT,  

	MA_MAP1+MAP_OFS_X, MA_MAP1+MAP_OFS_Y, MA_MAP1+MAP_OFS_Z, 
	MA_MAP1+MAP_SIZE_X, MA_MAP1+MAP_SIZE_Y, MA_MAP1+MAP_SIZE_Z, 
	MA_MAP1+MAP_R, MA_MAP1+MAP_G, MA_MAP1+MAP_B,
	MA_MAP1+MAP_DVAR, MA_MAP1+MAP_COLF, MA_MAP1+MAP_NORF, MA_MAP1+MAP_VARF
};

int cam_ar[CAM_TOTIPO]= {
	CAM_LENS, CAM_STA, CAM_END
};

int snd_ar[SND_TOTIPO]= {
	SND_VOLUME, SND_PITCH, SND_PANNING, SND_ATTEN
};



float frame_to_float(int cfra)		/* see also bsystem_time in object.c */
{
	extern float bluroffs;	/* object.c */
	float ctime;
	
	ctime= (float)cfra;
	if(R.flag & R_SEC_FIELD) {
		if((R.r.mode & R_FIELDSTILL)==0) ctime+= 0.5;
	}
	ctime+= bluroffs;
	ctime*= G.scene->r.framelen;
	
	return ctime;
}

/* do not free ipo itself */
void free_ipo(Ipo *ipo)
{
	IpoCurve *icu;
	
	icu= ipo->curve.first;
	while(icu) {
		if(icu->bezt) MEM_freeN(icu->bezt);
		icu= icu->next;
	}
	BLI_freelistN(&ipo->curve);
}

Ipo *add_ipo(char *name, int idcode)
{
	Ipo *ipo;
	
	ipo= alloc_libblock(&G.main->ipo, ID_IP, name);
	ipo->blocktype= idcode;
	
	return ipo;
}

Ipo *copy_ipo(Ipo *ipo)
{
	Ipo *ipon;
	IpoCurve *icu;
	
	if(ipo==0) return 0;
	
	ipon= copy_libblock(ipo);
	
	duplicatelist(&(ipon->curve), &(ipo->curve));

	icu= ipon->curve.first;
	while(icu) {
		icu->bezt= MEM_dupallocN(icu->bezt);
		icu= icu->next;
	}
	
	return ipon;
}

void make_local_obipo(Ipo *ipo)
{
	Object *ob;
	Ipo *ipon;
	int local=0, lib=0;
	
	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	ob= G.main->object.first;
	while(ob) {
		if(ob->ipo==ipo) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		ipo->id.lib= 0;
		ipo->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ipo, 0);
	}
	else if(local && lib) {
		ipon= copy_ipo(ipo);
		ipon->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->ipo==ipo) {
				
				if(ob->id.lib==0) {
					ob->ipo= ipon;
					ipon->id.us++;
					ipo->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

void make_local_matipo(Ipo *ipo)
{
	Material *ma;
	Ipo *ipon;
	int local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	*/
	
	ma= G.main->mat.first;
	while(ma) {
		if(ma->ipo==ipo) {
			if(ma->id.lib) lib= 1;
			else local= 1;
		}
		ma= ma->id.next;
	}
	
	if(local && lib==0) {
		ipo->id.lib= 0;
		ipo->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ipo, 0);
	}
	else if(local && lib) {
		ipon= copy_ipo(ipo);
		ipon->id.us= 0;
		
		ma= G.main->mat.first;
		while(ma) {
			if(ma->ipo==ipo) {
				
				if(ma->id.lib==0) {
					ma->ipo= ipon;
					ipon->id.us++;
					ipo->id.us--;
				}
			}
			ma= ma->id.next;
		}
	}
}

void make_local_keyipo(Ipo *ipo)
{
	Key *key;
	Ipo *ipon;
	int local=0, lib=0;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	key= G.main->key.first;
	while(key) {
		if(key->ipo==ipo) {
			if(key->id.lib) lib= 1;
			else local= 1;
		}
		key= key->id.next;
	}
	
	if(local && lib==0) {
		ipo->id.lib= 0;
		ipo->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ipo, 0);
	}
	else if(local && lib) {
		ipon= copy_ipo(ipo);
		ipon->id.us= 0;
		
		key= G.main->key.first;
		while(key) {
			if(key->ipo==ipo) {
				
				if(key->id.lib==0) {
					key->ipo= ipon;
					ipon->id.us++;
					ipo->id.us--;
				}
			}
			key= key->id.next;
		}
	}
}


void make_local_ipo(Ipo *ipo)
{
	
	if(ipo->id.lib==0) return;
	if(ipo->id.us==1) {
		ipo->id.lib= 0;
		ipo->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ipo, 0);
		return;
	}
	
	if(ipo->blocktype==ID_OB) make_local_obipo(ipo);
	else if(ipo->blocktype==ID_MA) make_local_matipo(ipo);
	else if(ipo->blocktype==ID_KE) make_local_keyipo(ipo);

}


void calchandles_ipocurve(IpoCurve *icu)
{
	BezTriple *bezt, *prev, *next;
	int a;

	a= icu->totvert;
	if(a<2) return;
	
	bezt= icu->bezt;
	prev= 0;
	next= bezt+1;

	while(a--) {

		if(bezt->vec[0][0]>bezt->vec[1][0]) bezt->vec[0][0]= bezt->vec[1][0];
		if(bezt->vec[2][0]<bezt->vec[1][0]) bezt->vec[2][0]= bezt->vec[1][0];

		calchandleNurb(bezt, prev, next, 1);	/* 1==special autohandle */

		prev= bezt;
		if(a==1) {
			next= 0;
		}
		else next++;
			
		/* for automatic ease in and out */
		if(bezt->h1==HD_AUTO && bezt->h2==HD_AUTO) {
			if(a==0 || a==icu->totvert-1) {
				if(icu->extrap==IPO_HORIZ) {
					bezt->vec[0][1]= bezt->vec[2][1]= bezt->vec[1][1];
				}
			}
		}
		
		bezt++;
	}
}

void testhandles_ipocurve(IpoCurve *icu)
{
    /* use when something has changed with handles.
    it treats all BezTriples with the following rules:
    PHASE 1: do types have to be altered?
     Auto handles: become aligned when selection status is NOT(000 || 111)
     Vector handles: become 'nothing' when (one half selected AND other not)
    PHASE 2: recalculate handles
    */
    BezTriple *bezt;
	int flag, a;

	bezt= icu->bezt;
	if(bezt==0) return;
	
	a= icu->totvert;
	while(a--) {
		flag= 0;
		if(bezt->f1 & 1) flag++;
		if(bezt->f2 & 1) flag += 2;
		if(bezt->f3 & 1) flag += 4;

		if( !(flag==0 || flag==7) ) {
			if(bezt->h1==HD_AUTO) {   /* auto */
				bezt->h1= HD_ALIGN;
			}
			if(bezt->h2==HD_AUTO) {   /* auto */
				bezt->h2= HD_ALIGN;
			}

			if(bezt->h1==HD_VECT) {   /* vector */
				if(flag < 4) bezt->h1= 0;
			}
			if(bezt->h2==HD_VECT) {   /* vector */
				if( flag > 3) bezt->h2= 0;
			}
		}
		bezt++;
	}

	calchandles_ipocurve(icu);
}


void sort_time_ipocurve(IpoCurve *icu)
{
	BezTriple *bezt;
	int a, ok= 1;
	
	while(ok) {
		ok= 0;

		if(icu->bezt) {
			bezt= icu->bezt;
			a= icu->totvert;
			while(a--) {
				if(a>0) {
					if( bezt->vec[1][0] > (bezt+1)->vec[1][0]) {
						SWAP(BezTriple, *bezt, *(bezt+1));
						ok= 1;
					}
				}
				if(bezt->vec[0][0]>=bezt->vec[1][0] && bezt->vec[2][0]<=bezt->vec[1][0]) {
					SWAP(float, bezt->vec[0][0], bezt->vec[2][0]);
					SWAP(float, bezt->vec[0][1], bezt->vec[2][1]);
				}
				else {
					if(bezt->vec[0][0]>bezt->vec[1][0]) bezt->vec[0][0]= bezt->vec[1][0];
					if(bezt->vec[2][0]<bezt->vec[1][0]) bezt->vec[2][0]= bezt->vec[1][0];
				}
				bezt++;
			}
		}
		else {
			
		}
	}
}

int test_time_ipocurve(IpoCurve *icu)
{
	BezTriple *bezt;
	int a;
	
	if(icu->bezt) {
		bezt= icu->bezt;
		a= icu->totvert-1;
		while(a--) {
			if( bezt->vec[1][0] > (bezt+1)->vec[1][0]) {
				return 1;
			}
			bezt++;
		}	
	}
	else {
		
	}

	return 0;
}

void correct_bezpart(float *v1, float *v2, float *v3, float *v4)
{
	/* the total length of the handles is not allowed to be more
	 * than the horizontal distance between (v1-v4)
         * this to prevent curve loops
	 */
	float h1[2], h2[2], len1, len2, len, fac;
	
	h1[0]= v1[0]-v2[0];
	h1[1]= v1[1]-v2[1];
	h2[0]= v4[0]-v3[0];
	h2[1]= v4[1]-v3[1];
	
	len= v4[0]- v1[0];
	len1= (float)fabs(h1[0]);
	len2= (float)fabs(h2[0]);
	
	if(len1+len2==0.0) return;
	if(len1+len2 > len) {
		fac= len/(len1+len2);
		
		v2[0]= (v1[0]-fac*h1[0]);
		v2[1]= (v1[1]-fac*h1[1]);
		
		v3[0]= (v4[0]-fac*h2[0]);
		v3[1]= (v4[1]-fac*h2[1]);
		
	}
}

/* *********************** ARITH *********************** */

int findzero(float x, float q0, float q1, float q2, float q3, float *o)
{
	double c0, c1, c2, c3, a, b, c, p, q, d, t, phi;
	int nr= 0;

	c0= q0-x;
	c1= 3*(q1-q0);
	c2= 3*(q0-2*q1+q2);
	c3= q3-q0+3*(q1-q2);
	
	if(c3!=0.0) {
		a= c2/c3;
		b= c1/c3;
		c= c0/c3;
		a= a/3;

		p= b/3-a*a;
		q= (2*a*a*a-a*b+c)/2;
		d= q*q+p*p*p;

		if(d>0.0) {
			t= sqrt(d);
			o[0]= (float)(Sqrt3d(-q+t)+Sqrt3d(-q-t)-a);
			if(o[0]>= SMALL && o[0]<=1.000001) return 1;
			else return 0;
		}
		else if(d==0.0) {
			t= Sqrt3d(-q);
			o[0]= (float)(2*t-a);
			if(o[0]>=SMALL && o[0]<=1.000001) nr++;
			o[nr]= (float)(-t-a);
			if(o[nr]>=SMALL && o[nr]<=1.000001) return nr+1;
			else return nr;
		}
		else {
			phi= acos(-q/sqrt(-(p*p*p)));
			t= sqrt(-p);
			p= cos(phi/3);
			q= sqrt(3-3*p*p);
			o[0]= (float)(2*t*p-a);
			if(o[0]>=SMALL && o[0]<=1.000001) nr++;
			o[nr]= (float)(-t*(p+q)-a);
			if(o[nr]>=SMALL && o[nr]<=1.000001) nr++;
			o[nr]= (float)(-t*(p-q)-a);
			if(o[nr]>=SMALL && o[nr]<=1.000001) return nr+1;
			else return nr;
		}
	}
	else {
		a=c2;
		b=c1;
		c=c0;
		
		if(a!=0.0) {
			p=b*b-4*a*c;
			if(p>0) {
				p= sqrt(p);
				o[0]= (float)((-b-p)/(2*a));
				if(o[0]>=SMALL && o[0]<=1.000001) nr++;
				o[nr]= (float)((-b+p)/(2*a));
				if(o[nr]>=SMALL && o[nr]<=1.000001) return nr+1;
				else return nr;
			}
			else if(p==0) {
				o[0]= (float)(-b/(2*a));
				if(o[0]>=SMALL && o[0]<=1.000001) return 1;
				else return 0;
			}
		}
		else if(b!=0.0) {
			o[0]= (float)(-c/b);
			if(o[0]>=SMALL && o[0]<=1.000001) return 1;
			else return 0;
		}
		else if(c==0.0) {
			o[0]= 0.0;
			return 1;
		}
		return 0;	
	}
}

void berekeny(float f1, float f2, float f3, float f4, float *o, int b)
{
	float t, c0, c1, c2, c3;
	int a;

	c0= f1;
	c1= 3.0f*(f2 - f1);
	c2= 3.0f*(f1 - 2.0f*f2 + f3);
	c3= f4 - f1 + 3.0f*(f2-f3);
	
	for(a=0; a<b; a++) {
		t= o[a];
		o[a]= c0+t*c1+t*t*c2+t*t*t*c3;
	}
}
void berekenx(float *f, float *o, int b)
{
	float t, c0, c1, c2, c3;
	int a;

	c0= f[0];
	c1= 3*(f[3]-f[0]);
	c2= 3*(f[0]-2*f[3]+f[6]);
	c3= f[9]-f[0]+3*(f[3]-f[6]);
	for(a=0; a<b; a++) {
		t= o[a];
		o[a]= c0+t*c1+t*t*c2+t*t*t*c3;
	}
}

float eval_icu(IpoCurve *icu, float ipotime) 
{
	BezTriple *bezt, *prevbezt;
	float v1[2], v2[2], v3[2], v4[2], opl[32], dx, fac;
	float cycdx, cycdy, ofs, cycyofs, cvalue = 0.0;
	int a, b;
	
	cycyofs= 0.0;
	
	if(icu->bezt) {
		prevbezt= icu->bezt;
		bezt= prevbezt+1;
		a= icu->totvert-1;
		
		/* cyclic? */
		if(icu->extrap & IPO_CYCL) {
			ofs= icu->bezt->vec[1][0];
			cycdx= (icu->bezt+icu->totvert-1)->vec[1][0] - ofs;
			cycdy= (icu->bezt+icu->totvert-1)->vec[1][1] - icu->bezt->vec[1][1];
			if(cycdx!=0.0) {
				
				if(icu->extrap & IPO_DIR) {
					cycyofs= (float)floor((ipotime-ofs)/cycdx);
					cycyofs*= cycdy;
				}

				ipotime= (float)(fmod(ipotime-ofs, cycdx)+ofs);
				if(ipotime<ofs) ipotime+= cycdx;
			}
		}
		
		/* endpoints? */
	
		if(prevbezt->vec[1][0]>=ipotime) {
			if( (icu->extrap & IPO_DIR) && icu->ipo!=IPO_CONST) {
				dx= prevbezt->vec[1][0]-ipotime;
				fac= prevbezt->vec[1][0]-prevbezt->vec[0][0];
				if(fac!=0.0) {
					fac= (prevbezt->vec[1][1]-prevbezt->vec[0][1])/fac;
					cvalue= prevbezt->vec[1][1]-fac*dx;
				}
				else cvalue= prevbezt->vec[1][1];
			}
			else cvalue= prevbezt->vec[1][1];
			
			cvalue+= cycyofs;
		}
		else if( (prevbezt+a)->vec[1][0]<=ipotime) {
			if( (icu->extrap & IPO_DIR) && icu->ipo!=IPO_CONST) {
				prevbezt+= a;
				dx= ipotime-prevbezt->vec[1][0];
				fac= prevbezt->vec[2][0]-prevbezt->vec[1][0];

				if(fac!=0) {
					fac= (prevbezt->vec[2][1]-prevbezt->vec[1][1])/fac;
					cvalue= prevbezt->vec[1][1]+fac*dx;
				}
				else cvalue= prevbezt->vec[1][1];
			}
			else cvalue= (prevbezt+a)->vec[1][1];
			
			cvalue+= cycyofs;
		}
		else {
			while(a--) {
				if(prevbezt->vec[1][0]<=ipotime && bezt->vec[1][0]>=ipotime) {
					if(icu->ipo==IPO_CONST) {
						cvalue= prevbezt->vec[1][1]+cycyofs;
					}
					else if(icu->ipo==IPO_LIN) {
						fac= bezt->vec[1][0]-prevbezt->vec[1][0];
						if(fac==0) cvalue= cycyofs+prevbezt->vec[1][1];
						else {
							fac= (ipotime-prevbezt->vec[1][0])/fac;
							cvalue= cycyofs+prevbezt->vec[1][1]+ fac*(bezt->vec[1][1]-prevbezt->vec[1][1]);
						}
					}
					else {
						v1[0]= prevbezt->vec[1][0];
						v1[1]= prevbezt->vec[1][1];
						v2[0]= prevbezt->vec[2][0];
						v2[1]= prevbezt->vec[2][1];
						
						v3[0]= bezt->vec[0][0];
						v3[1]= bezt->vec[0][1];
						v4[0]= bezt->vec[1][0];
						v4[1]= bezt->vec[1][1];

						correct_bezpart(v1, v2, v3, v4);
						
						b= findzero(ipotime, v1[0], v2[0], v3[0], v4[0], opl);
						if(b) {
							berekeny(v1[1], v2[1], v3[1], v4[1], opl, 1);
							cvalue= opl[0]+cycyofs;
							break;
						}
					}
				}
				prevbezt= bezt;
				bezt++;
			}
		}
	}

	if(icu->ymin < icu->ymax) {
		if(cvalue < icu->ymin) cvalue= icu->ymin;
		else if(cvalue > icu->ymax) cvalue= icu->ymax;
	}
	
	return cvalue;
}

void calc_icu(IpoCurve *icu, float ctime)
{
	icu->curval= eval_icu(icu, ctime);
}

float calc_ipo_time(Ipo *ipo, float ctime)
{

	if(ipo && ipo->blocktype==ID_OB) {
		IpoCurve *icu= ipo->curve.first;

		while(icu) {
			if (icu->adrcode==OB_TIME) {
				calc_icu(icu, ctime);
				return 10.0f*icu->curval;
			}
			icu= icu->next;
		}	
	}
	
	return ctime;
}

void calc_ipo(Ipo *ipo, float ctime)
{
	IpoCurve *icu;
	
	icu= ipo->curve.first;
	while(icu) {
		
		if( (icu->flag & IPO_LOCK)==0) calc_icu(icu, ctime);
		
		icu= icu->next;
	}
}

/* ************************************** */
/*		DO THE IPO!						  */
/* ************************************** */

void write_ipo_poin(void *poin, int type, float val)
{

	switch(type) {
	case IPO_FLOAT:
		*( (float *)poin)= val;
		break;
	case IPO_FLOAT_DEGR:
		*( (float *)poin)= (float)(val*M_PI_2/9.0);
		break;
	case IPO_INT:
	case IPO_INT_BIT:
	case IPO_LONG:
		*( (int *)poin)= (int)val;
		break;
	case IPO_SHORT:
	case IPO_SHORT_BIT:
		*( (short *)poin)= (short)val;
		break;
	case IPO_CHAR:
	case IPO_CHAR_BIT:
		*( (char *)poin)= (char)val;
		break;
	}
}

float read_ipo_poin(void *poin, int type)
{
	float val = 0.0;
	
	switch(type) {
	case IPO_FLOAT:
		val= *( (float *)poin);
		break;
	case IPO_FLOAT_DEGR:
		val= *( (float *)poin);
		val = (float)(val/(M_PI_2/9.0));
		break;
	case IPO_INT:
	case IPO_INT_BIT:
	case IPO_LONG:
		val= (float)(*( (int *)poin));
		break;
	case IPO_SHORT:
	case IPO_SHORT_BIT:
		val= *( (short *)poin);
		break;
	case IPO_CHAR:
	case IPO_CHAR_BIT:
		val= *( (char *)poin);
		break;
	}
	return val;
}

void *give_mtex_poin(MTex *mtex, int adrcode )
{
	void *poin=0;
		
	switch(adrcode) {
	case MAP_OFS_X:
		poin= &(mtex->ofs[0]); break;
	case MAP_OFS_Y:
		poin= &(mtex->ofs[1]); break;
	case MAP_OFS_Z:
		poin= &(mtex->ofs[2]); break;
	case MAP_SIZE_X:
		poin= &(mtex->size[0]); break;
	case MAP_SIZE_Y:
		poin= &(mtex->size[1]); break;
	case MAP_SIZE_Z:
		poin= &(mtex->size[2]); break;
	case MAP_R:
		poin= &(mtex->r); break;
	case MAP_G:
		poin= &(mtex->g); break;
	case MAP_B:
		poin= &(mtex->b); break;
	case MAP_DVAR:
		poin= &(mtex->def_var); break;
	case MAP_COLF:
		poin= &(mtex->colfac); break;
	case MAP_NORF:
		poin= &(mtex->norfac); break;
	case MAP_VARF:
		poin= &(mtex->varfac); break;
	}
	
	return poin;
}

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */

void *get_ipo_poin(ID *id, IpoCurve *icu, int *type)
{
	void *poin= NULL;
	Object *ob;
	Material *ma;
	MTex *mtex;
	Lamp *la;
	Sequence *seq;
	World *wo;
	bAction			*act;
	bActionChannel	*achan;
	bPoseChannel	*pchan;

	*type= IPO_FLOAT;

	if( GS(id->name)==ID_OB) {
	
		ob= (Object *)id;

		switch(icu->adrcode) {
		case OB_LOC_X:
			poin= &(ob->loc[0]); break;
		case OB_LOC_Y:
			poin= &(ob->loc[1]); break;
		case OB_LOC_Z:
			poin= &(ob->loc[2]); break;
		case OB_DLOC_X:
			poin= &(ob->dloc[0]); break;
		case OB_DLOC_Y:
			poin= &(ob->dloc[1]); break;
		case OB_DLOC_Z:
			poin= &(ob->dloc[2]); break;
	
		case OB_ROT_X:
			poin= &(ob->rot[0]); *type= IPO_FLOAT_DEGR; break;
		case OB_ROT_Y:
			poin= &(ob->rot[1]); *type= IPO_FLOAT_DEGR; break;
		case OB_ROT_Z:
			poin= &(ob->rot[2]); *type= IPO_FLOAT_DEGR; break;
		case OB_DROT_X:
			poin= &(ob->drot[0]); *type= IPO_FLOAT_DEGR; break;
		case OB_DROT_Y:
			poin= &(ob->drot[1]); *type= IPO_FLOAT_DEGR; break;
		case OB_DROT_Z:
			poin= &(ob->drot[2]); *type= IPO_FLOAT_DEGR; break;
			
		case OB_SIZE_X:
			poin= &(ob->size[0]); break;
		case OB_SIZE_Y:
			poin= &(ob->size[1]); break;
		case OB_SIZE_Z:
			poin= &(ob->size[2]); break;
		case OB_DSIZE_X:
			poin= &(ob->dsize[0]); break;
		case OB_DSIZE_Y:
			poin= &(ob->dsize[1]); break;
		case OB_DSIZE_Z:
			poin= &(ob->dsize[2]); break;

		case OB_LAY:
			poin= &(ob->lay); *type= IPO_INT_BIT; break;
			
		case OB_COL_R:	
			poin= &(ob->col[0]);
			break;
		case OB_COL_G:
			poin= &(ob->col[1]);
			break;
		case OB_COL_B:
			poin= &(ob->col[2]);
			break;
		case OB_COL_A:
			poin= &(ob->col[3]);
			break;
		case OB_PD_FSTR:
			if(ob->pd) poin= &(ob->pd->f_strength);
			break;
		case OB_PD_FFALL:
			if(ob->pd) poin= &(ob->pd->f_power);
			break;
		case OB_PD_SDAMP:
			if(ob->pd) poin= &(ob->pd->pdef_damp);
			break;
		case OB_PD_RDAMP:
			if(ob->pd) poin= &(ob->pd->pdef_rdamp);
			break;
		case OB_PD_PERM:
			if(ob->pd) poin= &(ob->pd->pdef_perm);
			break;
		}
	}
	else if (GS(id->name)==ID_AC){
		act= (bAction *)id;
		achan = act->achan;
		pchan = act->pchan;
		if (!pchan || !achan)
			return NULL;
		switch (icu->adrcode){
		case AC_QUAT_W:
			poin= &(pchan->quat[0]); break;
		case AC_QUAT_X:
			poin= &(pchan->quat[1]); break;
		case AC_QUAT_Y:
			poin= &(pchan->quat[2]); break;
		case AC_QUAT_Z:
			poin= &(pchan->quat[3]); break;
		case AC_LOC_X:
			poin= &(pchan->loc[0]); break;
		case AC_LOC_Y:
			poin= &(pchan->loc[1]); break;
		case AC_LOC_Z:
			poin= &(pchan->loc[2]); break;			
		case AC_SIZE_X:
			poin= &(pchan->size[0]); break;
		case AC_SIZE_Y:
			poin= &(pchan->size[1]); break;
		case AC_SIZE_Z:
			poin= &(pchan->size[2]); break;
		};
	}

	else if( GS(id->name)==ID_MA) {
		
		ma= (Material *)id;
		
		switch(icu->adrcode) {
		case MA_COL_R:
			poin= &(ma->r); break;
		case MA_COL_G:
			poin= &(ma->g); break;
		case MA_COL_B:
			poin= &(ma->b); break;
		case MA_SPEC_R:
			poin= &(ma->specr); break;
		case MA_SPEC_G:
			poin= &(ma->specg); break;
		case MA_SPEC_B:
			poin= &(ma->specb); break;
		case MA_MIR_R:
			poin= &(ma->mirr); break;
		case MA_MIR_G:
			poin= &(ma->mirg); break;
		case MA_MIR_B:
			poin= &(ma->mirb); break;
		case MA_REF:
			poin= &(ma->ref); break;
		case MA_ALPHA:
			poin= &(ma->alpha); break;
		case MA_EMIT:
			poin= &(ma->emit); break;
		case MA_AMB:
			poin= &(ma->amb); break;
		case MA_SPEC:
			poin= &(ma->spec); break;
		case MA_HARD:
			poin= &(ma->har); *type= IPO_SHORT; break;
		case MA_SPTR:
			poin= &(ma->spectra); break;
		case MA_ANG:
			poin= &(ma->ang); break;
		case MA_MODE:
			poin= &(ma->mode); *type= IPO_INT_BIT; break;
		case MA_HASIZE:
			poin= &(ma->hasize); break;
		}
		
		if(poin==0) {
			mtex= 0;
			if(icu->adrcode & MA_MAP1) mtex= ma->mtex[0];
			else if(icu->adrcode & MA_MAP2) mtex= ma->mtex[1];
			else if(icu->adrcode & MA_MAP3) mtex= ma->mtex[2];
			else if(icu->adrcode & MA_MAP4) mtex= ma->mtex[3];
			else if(icu->adrcode & MA_MAP5) mtex= ma->mtex[4];
			else if(icu->adrcode & MA_MAP6) mtex= ma->mtex[5];
			else if(icu->adrcode & MA_MAP7) mtex= ma->mtex[6];
			else if(icu->adrcode & MA_MAP8) mtex= ma->mtex[7];
			
			if(mtex) {
				poin= give_mtex_poin(mtex, icu->adrcode & (MA_MAP1-1) );
			}
		}
	}
	else if( GS(id->name)==ID_SEQ) {
		seq= (Sequence *)id;
		
		switch(icu->adrcode) {
		case SEQ_FAC1:
			poin= &(seq->facf0); break;
		}
	}
	else if( GS(id->name)==ID_CU) {
		
		poin= &(icu->curval);
		
	}
	else if( GS(id->name)==ID_KE) {
		
		poin= &(icu->curval);
		
	}
	else if(GS(id->name)==ID_WO) {
		
		wo= (World *)id;
		
		switch(icu->adrcode) {
		case WO_HOR_R:
			poin= &(wo->horr); break;
		case WO_HOR_G:
			poin= &(wo->horg); break;
		case WO_HOR_B:
			poin= &(wo->horb); break;
		case WO_ZEN_R:
			poin= &(wo->zenr); break;
		case WO_ZEN_G:
			poin= &(wo->zeng); break;
		case WO_ZEN_B:
			poin= &(wo->zenb); break;

		case WO_EXPOS:
			poin= &(wo->exposure); break;

		case WO_MISI:
			poin= &(wo->misi); break;
		case WO_MISTDI:
			poin= &(wo->mistdist); break;
		case WO_MISTSTA:
			poin= &(wo->miststa); break;
		case WO_MISTHI:
			poin= &(wo->misthi); break;

		case WO_STAR_R:
			poin= &(wo->starr); break;
		case WO_STAR_G:
			poin= &(wo->starg); break;
		case WO_STAR_B:
			poin= &(wo->starb); break;

		case WO_STARDIST:
			poin= &(wo->stardist); break;
		case WO_STARSIZE:
			poin= &(wo->starsize); break;
		}

		if(poin==0) {
			mtex= 0;
			if(icu->adrcode & MA_MAP1) mtex= wo->mtex[0];
			else if(icu->adrcode & MA_MAP2) mtex= wo->mtex[1];
			else if(icu->adrcode & MA_MAP3) mtex= wo->mtex[2];
			else if(icu->adrcode & MA_MAP4) mtex= wo->mtex[3];
			else if(icu->adrcode & MA_MAP5) mtex= wo->mtex[4];
			else if(icu->adrcode & MA_MAP6) mtex= wo->mtex[5];
			else if(icu->adrcode & MA_MAP7) mtex= wo->mtex[6];
			else if(icu->adrcode & MA_MAP8) mtex= wo->mtex[7];
			
			if(mtex) {
				poin= give_mtex_poin(mtex, icu->adrcode & (MA_MAP1-1) );
			}
		}
	}
	else if( GS(id->name)==ID_LA) {
		
		la= (Lamp *)id;
	
		switch(icu->adrcode) {
		case LA_ENERGY:
			poin= &(la->energy); break;		
		case LA_COL_R:
			poin= &(la->r); break;
		case LA_COL_G:
			poin= &(la->g); break;
		case LA_COL_B:
			poin= &(la->b); break;
		case LA_DIST:
			poin= &(la->dist); break;		
		case LA_SPOTSI:
			poin= &(la->spotsize); break;
		case LA_SPOTBL:
			poin= &(la->spotblend); break;
		case LA_QUAD1:
			poin= &(la->att1); break;
		case LA_QUAD2:
			poin= &(la->att2); break;
		case LA_HALOINT:
			poin= &(la->haint); break;
		}
		
		if(poin==0) {
			mtex= 0;
			if(icu->adrcode & MA_MAP1) mtex= la->mtex[0];
			else if(icu->adrcode & MA_MAP2) mtex= la->mtex[1];
			else if(icu->adrcode & MA_MAP3) mtex= la->mtex[2];
			else if(icu->adrcode & MA_MAP4) mtex= la->mtex[3];
			else if(icu->adrcode & MA_MAP5) mtex= la->mtex[4];
			else if(icu->adrcode & MA_MAP6) mtex= la->mtex[5];
			else if(icu->adrcode & MA_MAP7) mtex= la->mtex[6];
			else if(icu->adrcode & MA_MAP8) mtex= la->mtex[7];
			
			if(mtex) {
				poin= give_mtex_poin(mtex, icu->adrcode & (MA_MAP1-1) );
			}
		}
	}
	else if(GS(id->name)==ID_CA) {
		Camera *ca= (Camera *)id;
		
		switch(icu->adrcode) {
		case CAM_LENS:
			poin= &(ca->lens); break;
		case CAM_STA:
			poin= &(ca->clipsta); break;
		case CAM_END:
			poin= &(ca->clipend); break;
		}
	}
	else if(GS(id->name)==ID_SO) {
		bSound *snd= (bSound *)id;
		
		switch(icu->adrcode) {
		case SND_VOLUME:
			poin= &(snd->volume); break;
		case SND_PITCH:
			poin= &(snd->pitch); break;
		case SND_PANNING:
			poin= &(snd->panning); break;
		case SND_ATTEN:
			poin= &(snd->attenuation); break;
		}
	}
	
	return poin;
}

void set_icu_vars(IpoCurve *icu)
{
	
	icu->ymin= icu->ymax= 0.0;
	icu->ipo= IPO_BEZ;
	
	if(icu->blocktype==ID_OB) {
	
		if(icu->adrcode==OB_LAY) {
			icu->ipo= IPO_CONST;
			icu->vartype= IPO_BITS;
		}
		
	}
	else if(icu->blocktype==ID_MA) {
		
		if(icu->adrcode < MA_MAP1) {
			switch(icu->adrcode) {
			case MA_HASIZE:
				icu->ymax= 10000.0; break;
			case MA_HARD:
				icu->ymax= 128.0; break;
			case MA_SPEC:
				icu->ymax= 2.0; break;
			case MA_MODE:
				icu->ipo= IPO_CONST;
				icu->vartype= IPO_BITS;
				break;
				
			default:
				icu->ymax= 1.0;
				break;
			}
		}
		else {
			switch(icu->adrcode & (MA_MAP1-1)) {
			case MAP_OFS_X:
			case MAP_OFS_Y:
			case MAP_OFS_Z:
			case MAP_SIZE_X:
			case MAP_SIZE_Y:
			case MAP_SIZE_Z:
				icu->ymax= 1000.0;
				icu->ymin= -1000.0;
			
				break;
			case MAP_R:
			case MAP_G:
			case MAP_B:
			case MAP_DVAR:
			case MAP_COLF:
			case MAP_VARF:
				icu->ymax= 1.0;
				break;
			case MAP_NORF:
				icu->ymax= 5.0;
				break;
			}
		}
	}
	else if(icu->blocktype==ID_SEQ) {
	
		icu->ymax= 1.0;
		
	}
	else if(icu->blocktype==ID_CU) {
	
		icu->ymax= 1.0;
		
	}
	else if(icu->blocktype==ID_WO) {
		
		if(icu->adrcode < MA_MAP1) {
			switch(icu->adrcode) {
			case WO_EXPOS:
				icu->ymax= 5.0; break;
			case WO_MISTDI:
			case WO_MISTSTA:
			case WO_MISTHI:
			case WO_STARDIST:
			case WO_STARSIZE:
				break;
				
			default:
				icu->ymax= 1.0;
				break;
			}
		}
		else {
			switch(icu->adrcode & (MA_MAP1-1)) {
			case MAP_OFS_X:
			case MAP_OFS_Y:
			case MAP_OFS_Z:
			case MAP_SIZE_X:
			case MAP_SIZE_Y:
			case MAP_SIZE_Z:
				icu->ymax= 100.0;
				icu->ymin= -100.0;
			
				break;
			case MAP_R:
			case MAP_G:
			case MAP_B:
			case MAP_DVAR:
			case MAP_COLF:
			case MAP_NORF:
			case MAP_VARF:
				icu->ymax= 1.0;
			}
		}
	}
	else if(icu->blocktype==ID_LA) {
		if(icu->adrcode < MA_MAP1) {
			switch(icu->adrcode) {
			case LA_ENERGY:
			case LA_DIST:
				break;		
	
			case LA_COL_R:
			case LA_COL_G:
			case LA_COL_B:
			case LA_SPOTBL:
			case LA_QUAD1:
			case LA_QUAD2:
				icu->ymax= 1.0; break;
			case LA_SPOTSI:
				icu->ymax= 180.0; break;
			case LA_HALOINT:
				icu->ymax= 5.0; break;
			}
		}
		else {
			switch(icu->adrcode & (MA_MAP1-1)) {
			case MAP_OFS_X:
			case MAP_OFS_Y:
			case MAP_OFS_Z:
			case MAP_SIZE_X:
			case MAP_SIZE_Y:
			case MAP_SIZE_Z:
				icu->ymax= 100.0;
				icu->ymin= -100.0;
				break;
			case MAP_R:
			case MAP_G:
			case MAP_B:
			case MAP_DVAR:
			case MAP_COLF:
			case MAP_NORF:
			case MAP_VARF:
				icu->ymax= 1.0;
			}
		}
	}	
	else if(icu->blocktype==ID_CA) {

		switch(icu->adrcode) {
		case CAM_LENS:
			icu->ymin= 5.0;
			icu->ymax= 1000.0; break;
		case CAM_STA:
			icu->ymin= 0.001f;
			break;
		case CAM_END:
			icu->ymin= 0.1f;
		}
	}
	else if(icu->blocktype==ID_SO) {

		switch(icu->adrcode) {
		case SND_VOLUME:
			icu->ymin= 0.0;
			icu->ymax= 1.0;
			break;
		case SND_PITCH:
			icu->ymin= -12.0;
			icu->ymin= 12.0;
			break;
		case SND_PANNING:
			icu->ymin= 0.0;
			icu->ymax= 1.0;
			break;
		case SND_ATTEN:
			icu->ymin= 0.0;
			icu->ymin= 1.0;
			break;
		}
	}
}


void execute_ipo(ID *id, Ipo *ipo)
{
	IpoCurve *icu;
	void *poin;
	int type;
	
	if(ipo==0) return;
	
	icu= ipo->curve.first;
	while(icu) {
		poin= get_ipo_poin(id, icu, &type);
		if(poin) write_ipo_poin(poin, type, icu->curval);
		icu= icu->next;
	}
}

/* exception: it does calc for objects...
 * now find out why this routine was used anyway!
 */
void do_ipo_nocalc(Ipo *ipo)
{
	Object *ob;
	Material *ma;
	World *wo;
	Lamp *la;
	Camera *ca;
	bSound *snd;
	
	if(ipo==0) return;
	
	switch(ipo->blocktype) {
	case ID_OB:
		ob= G.main->object.first;
		while(ob) {
			if(ob->ipo==ipo) {
				do_ob_ipo(ob);
				/* execute_ipo((ID *)ob, ipo); */
			}
			ob= ob->id.next;
		}
		break;
	case ID_MA:
		ma= G.main->mat.first;
		while(ma) {
			if(ma->ipo==ipo) execute_ipo((ID *)ma, ipo);
			ma= ma->id.next;
		}
		break;
	case ID_WO:
		wo= G.main->world.first;
		while(wo) {
			if(wo->ipo==ipo) execute_ipo((ID *)wo, ipo);
			wo= wo->id.next;
		}
		break;
	case ID_LA:
		la= G.main->lamp.first;
		while(la) {
			if(la->ipo==ipo) execute_ipo((ID *)la, ipo);
			la= la->id.next;
		}
		break;
	case ID_CA:
		ca= G.main->camera.first;
		while(ca) {
			if(ca->ipo==ipo) execute_ipo((ID *)ca, ipo);
			ca= ca->id.next;
		}
		break;
	case ID_SO:
		snd= G.main->sound.first;
		while(snd) {
			if(snd->ipo==ipo) execute_ipo((ID *)snd, ipo);
			snd= snd->id.next;
		}
		break;
	}
}

void do_ipo(Ipo *ipo)
{
	if(ipo) {
		float ctime= frame_to_float(G.scene->r.cfra);
		calc_ipo(ipo, ctime);
	
		do_ipo_nocalc(ipo);
	}
}



void do_mat_ipo(Material *ma)
{
	float ctime;
	
	if(ma==0 || ma->ipo==0) return;
	
	ctime= frame_to_float(G.scene->r.cfra);
	/* if(ob->ipoflag & OB_OFFS_OB) ctime-= ob->sf; */

	calc_ipo(ma->ipo, ctime);
	
	execute_ipo((ID *)ma, ma->ipo);
}

void do_ob_ipo(Object *ob)
{
	float ctime;
	unsigned int lay;
	
	if(ob->ipo==0) return;

	/* do not set ob->ctime here: for example when parent in invisible layer */
	
	ctime= bsystem_time(ob, 0, (float) G.scene->r.cfra, 0.0);

	calc_ipo(ob->ipo, ctime);

	/* Patch: remember localview */
	lay= ob->lay & 0xFF000000;
	
	execute_ipo((ID *)ob, ob->ipo);

	ob->lay |= lay;
	if(ob->id.name[2]=='S' && ob->id.name[3]=='C' && ob->id.name[4]=='E') {
		if(strcmp(G.scene->id.name+2, ob->id.name+6)==0) {
			G.scene->lay= ob->lay;
			copy_view3d_lock(0);
			/* no redraw here! creates too many calls */
		}
	}
}

void do_seq_ipo(Sequence *seq)
{
	float ctime, div;
	
	/* seq_ipo has an exception: calc both fields immediately */
	
	if(seq->ipo) {
		ctime= frame_to_float(G.scene->r.cfra - seq->startdisp);
		div= (seq->enddisp - seq->startdisp)/100.0f;
		if(div==0) return;
		
		/* 2nd field */
		calc_ipo(seq->ipo, (ctime+0.5f)/div);
		execute_ipo((ID *)seq, seq->ipo);
		seq->facf1= seq->facf0;

		/* 1st field */
		calc_ipo(seq->ipo, ctime/div);
		execute_ipo((ID *)seq, seq->ipo);

	}
	else seq->facf1= seq->facf0= 1.0f;
}

int has_ipo_code(Ipo *ipo, int code)
{
	IpoCurve *icu;
	
	if(ipo==0) return 0;
	
	icu= ipo->curve.first;
	while(icu) {
	
		if(icu->adrcode==code) return 1;
		
		icu= icu->next;
	}
	return 0;
}

void do_all_ipos()
{
	Base *base;
	Material *ma;
	World *wo;
	Ipo *ipo;
	Lamp *la;
	Camera *ca;
	bSound *snd;
	Sequence *seq;
	Editing *ed;
	float ctime;
	int set;

	ctime= frame_to_float(G.scene->r.cfra);
	
	ipo= G.main->ipo.first;
	while(ipo) {
		if(ipo->id.us && ipo->blocktype!=ID_OB) {
			calc_ipo(ipo, ctime);
		}
		ipo= ipo->id.next;
	}

	/* NEW: current scene ob ipo's */
	base= G.scene->base.first;
	set= 0;
	while(base) {

		/* Do object ipos */
		do_constraint_channels(&base->object->constraints, &base->object->constraintChannels, ctime);

		if(base->object->ipo) {
			/* do per object ipo the calc_ipo: because of possible timeoffs */
			do_ob_ipo(base->object);
			if(base->object->type==OB_MBALL) where_is_object(base->object);
		}
		base= base->next;
		
		if(base==0 && set==0 && G.scene->set) {
			set= 1;
			base= G.scene->set->base.first;
		}
	}

	ma= G.main->mat.first;
	while(ma) {
		if(ma->ipo) execute_ipo((ID *)ma, ma->ipo);
		ma= ma->id.next;
	}

	wo= G.main->world.first;
	while(wo) {
		if(wo->ipo) execute_ipo((ID *)wo, wo->ipo);
		wo= wo->id.next;
	}

	la= G.main->lamp.first;
	while(la) {
		if(la->ipo) execute_ipo((ID *)la, la->ipo);
		la= la->id.next;
	}

	ca= G.main->camera.first;
	while(ca) {
		if(ca->ipo) execute_ipo((ID *)ca, ca->ipo);
		ca= ca->id.next;
	}

	snd= G.main->sound.first;
	while(snd) {
		if(snd->ipo) execute_ipo((ID *)snd, snd->ipo);
		snd= snd->id.next;
	}

	/*just in case of...  WATCH IT: 2x */
	base= G.scene->base.first;
	while(base) {
		
		/* only update layer when an ipo */
		if( has_ipo_code(base->object->ipo, OB_LAY) ) {
			base->lay= base->object->lay;
		}
		
		base= base->next;
	}
	
	/* just in case...*/
	if(G.scene->set) {
		base= G.scene->set->base.first;
		while(base) {
			
			/* only update layer when an ipo */
			if( has_ipo_code(base->object->ipo, OB_LAY) ) {
				base->lay= base->object->lay;
			}
			
			base= base->next;
		}
	}

	/* intrr: process FAC Ipos used as volume envelopes */
	ed= G.scene->ed;
	if (ed) {
		seq= ed->seqbasep->first;
		while(seq) {
			if ((seq->type == SEQ_SOUND) && (seq->ipo)
			  &&(seq->startdisp<=G.scene->r.cfra+2) && (seq->enddisp>G.scene->r.cfra)) do_seq_ipo(seq);
			seq= seq->next;
		}
	}

}


int calc_ipo_spec(Ipo *ipo, int adrcode, float *ctime)
{
	IpoCurve *icu;

	if(ipo==0) return 0;

	icu= ipo->curve.first;
	while(icu) {
		if(icu->adrcode == adrcode) {
			if(icu->flag & IPO_LOCK);
			else calc_icu(icu, *ctime);
			
			*ctime= icu->curval;
			return 1;
		}
		icu= icu->next;
	}
	
	return 0;
}


/* ************************** */

void clear_delta_obipo(Ipo *ipo)
{
	Object *ob;
	
	if(ipo==0) return;
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->id.lib==0) {
			if(ob->ipo==ipo) {
				memset(&ob->dloc, 0, 12);
				memset(&ob->drot, 0, 12);
				memset(&ob->dsize, 0, 12);
			}
		}
		ob= ob->id.next;
	}
}

void add_to_cfra_elem(ListBase *lb, BezTriple *bezt)
{
	CfraElem *ce, *cen;
	
	ce= lb->first;
	while(ce) {
		
		if( ce->cfra==bezt->vec[1][0] ) {
			/* do because of double keys */
			if(bezt->f2 & 1) ce->sel= bezt->f2;
			return;
		}
		else if(ce->cfra > bezt->vec[1][0]) break;
		
		ce= ce->next;
	}	
	
	cen= MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");	
	if(ce) BLI_insertlinkbefore(lb, ce, cen);
	else BLI_addtail(lb, cen);

	cen->cfra= bezt->vec[1][0];
	cen->sel= bezt->f2;
}



void make_cfra_list(Ipo *ipo, ListBase *elems)
{
	IpoCurve *icu;
	CfraElem *ce;
	BezTriple *bezt;
	int a;
	
	if(ipo->blocktype==ID_OB) {
		icu= ipo->curve.first;
		while(icu) {
			if(icu->flag & IPO_VISIBLE) {
				switch(icu->adrcode) {
				case OB_DLOC_X:
				case OB_DLOC_Y:
				case OB_DLOC_Z:
				case OB_DROT_X:
				case OB_DROT_Y:
				case OB_DROT_Z:
				case OB_DSIZE_X:
				case OB_DSIZE_Y:
				case OB_DSIZE_Z:

				case OB_LOC_X:
				case OB_LOC_Y:
				case OB_LOC_Z:
				case OB_ROT_X:
				case OB_ROT_Y:
				case OB_ROT_Z:
				case OB_SIZE_X:
				case OB_SIZE_Y:
				case OB_SIZE_Z:
				case OB_PD_FSTR:
				case OB_PD_FFALL:
				case OB_PD_SDAMP:
				case OB_PD_RDAMP:
				case OB_PD_PERM:
					bezt= icu->bezt;
					if(bezt) {
						a= icu->totvert;
						while(a--) {
							add_to_cfra_elem(elems, bezt);
							bezt++;
						}
					}
					break;
				}
			}
			icu= icu->next;
		}
	}
	if(ipo->showkey==0) {
		/* deselect all keys */
		ce= elems->first;
		while(ce) {
			ce->sel= 0;
			ce= ce->next;
		}
	}
}

/* *********************** INTERFACE FOR KETSJI ********** */


int IPO_GetChannels(Ipo *ipo, IPO_Channel *channels)
{
	/* channels is max 32 items, allocated by calling function */	

	IpoCurve *icu;
	int total=0;
	
	if(ipo==NULL) return 0;
	
	icu= ipo->curve.first;
	while(icu) {
		
		channels[total]= icu->adrcode;
		total++;
		if(total>31) break;
		
		icu= icu->next;
	}
	
	return total;
}



/* Get the float value for channel 'channel' at time 'ctime' */

float IPO_GetFloatValue(Ipo *ipo, IPO_Channel channel, float ctime)
{
	if(ipo==NULL) return 0;
	
	calc_ipo_spec(ipo, channel, &ctime);
	
	if (OB_ROT_X <= channel && channel <= OB_DROT_Z) {
		ctime *= (float)(M_PI_2/9.0); 
	}

	return ctime;
}


void test_ipo_get()
{
	Object *ob;
	int tot;
	IPO_Channel chan[32];

	ob = (G.scene->basact ? G.scene->basact->object : 0);
	
	if(ob==NULL) return;
	if(ob->ipo==NULL) return;
	
	tot= IPO_GetChannels(ob->ipo, chan);
	printf("tot %d \n", tot);
	
	while(tot--) {
		printf("var1 %d \n", chan[tot]);
	}
	
	printf("var1 %f \n", IPO_GetFloatValue(ob->ipo, chan[0], 10.0));
}
