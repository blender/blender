/**
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

#include <string.h>
#include <math.h>

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "IMB_imbuf_types.h"

#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_bmfont.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_image.h"
#include "BKE_property.h"
#include "BKE_global.h"
#include "BKE_displist.h"
#include "BKE_object.h"
#include "BKE_material.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"

#include "BDR_editface.h"
#include "BDR_vpaint.h"
#include "BDR_drawmesh.h"

#include "BSE_drawview.h"

#include "blendef.h"
#include "nla.h"

//#include "glext.h"
/* some local functions */
static void draw_hide_tfaces(Object *ob, Mesh *me);

#if defined(GL_EXT_texture_object) && !defined(__sun__) && !defined(__APPLE__)

	/* exception for mesa... not according th opengl specs */
	#ifndef __linux__
		#define glBindTexture(A,B)     glBindTextureEXT(A,B)
	#endif

	#define glGenTextures(A,B)     glGenTexturesEXT(A,B)
	#define glDeleteTextures(A,B)  glDeleteTexturesEXT(A,B)
	#define glPolygonOffset(A,B)  glPolygonOffsetEXT(A,B)

#else

/* #define GL_FUNC_ADD_EXT					GL_FUNC_ADD */
/* #define GL_FUNC_REVERSE_SUBTRACT_EXT	GL_FUNC_REVERSE_SUBTRACT */
/* #define GL_POLYGON_OFFSET_EXT			GL_POLYGON_OFFSET */

#endif

	/* (n&(n-1)) zeros the least significant bit of n */
static int is_pow2(int num) {
	return ((num)&(num-1))==0;
}
static int smaller_pow2(int num) {
	while (!is_pow2(num))
		num= num&(num-1);
	return num;	
}

static int fCurtile=0, fCurmode=0,fCurtileXRep=0,fCurtileYRep=0;
static Image *fCurpage=0;
static short fTexwindx, fTexwindy, fTexwinsx, fTexwinsy;
static int fDoMipMap = 1;
static int fLinearMipMap = 0;

/*  static int source, dest; also not used */

/**
 * Enables or disable mipmapping for realtime images.
 * @param mipmap Turn mipmapping on (mipmap!=0) or off (mipmap==0).
 */
void set_mipmap(int mipmap)
{
	if (fDoMipMap != (mipmap != 0)) {
		free_all_realtime_images();
		fDoMipMap = mipmap != 0;
	}
}


/**
 * Returns the current setting for mipmapping.
 */
int get_mipmap(void)
{
	return fDoMipMap;
}

/**
 * Enables or disable linear mipmap setting for realtime images (textures).
 * Note that this will will destroy all texture bindings in OpenGL.
 * @see free_realtime_image()
 * @param mipmap Turn linear mipmapping on (linear!=0) or off (linear==0).
 */
void set_linear_mipmap(int linear)
{
	if (fLinearMipMap != (linear != 0)) {
		free_all_realtime_images();
		fLinearMipMap = linear != 0;
	}
}

/**
 * Returns the current setting for linear mipmapping.
 */
int get_linear_mipmap(void)
{
	return fLinearMipMap;
}


/**
 * Resets the realtime image cache variables.
 */
void clear_realtime_image_cache()
{
	fCurpage = NULL;
	fCurtile = 0;
	fCurmode = 0;
	fCurtileXRep = 0;
	fCurtileYRep = 0;
}

/* REMEMBER!  Changes here must go into my_set_tpage() as well */
int set_tpage(TFace *tface)
{	
	static int alphamode= -1;
	static TFace *lasttface= 0;
	Image *ima;
	unsigned int *rect, *bind;
	int tpx, tpy, tilemode, tileXRep,tileYRep;
	
	/* afschakelen */
	if(tface==0) {
		if(lasttface==0) return 0;
		
		lasttface= 0;
		fCurtile= 0;
		fCurpage= 0;
		if(fCurmode!=0) {
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
		}
		fCurmode= 0;
		fCurtileXRep=0;
		fCurtileYRep=0;
		alphamode= -1;
		
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);

		return 0;
	}
	lasttface= tface;

	if( alphamode != tface->transp) {
		alphamode= tface->transp;

		if(alphamode) {
			glEnable(GL_BLEND);
			
			if(alphamode==TF_ADD) {
				glBlendFunc(GL_ONE, GL_ONE);
			/* 	glBlendEquationEXT(GL_FUNC_ADD_EXT); */
			}
			else if(alphamode==TF_ALPHA) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			/* 	glBlendEquationEXT(GL_FUNC_ADD_EXT); */
			}
			/* else { */
			/* 	glBlendFunc(GL_ONE, GL_ONE); */
			/* 	glBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT); */
			/* } */
		}
		else glDisable(GL_BLEND);
	}

	ima= tface->tpage;

	/* Enable or disable reflection mapping */
	if (ima && (ima->flag & IMA_REFLECT)){

//		glActiveTextureARB(GL_TEXTURE0_ARB);
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);

		/* Handle multitexturing here */
	}
	else{
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
	}

	tilemode= tface->mode & TF_TILES;
	tileXRep = 0;
	tileYRep = 0;
	if (ima)
	{
		tileXRep = ima->xrep;
		tileYRep = ima->yrep;
	}


	if(ima==fCurpage && fCurtile==tface->tile && tilemode==fCurmode && fCurtileXRep==tileXRep && fCurtileYRep == tileYRep) return ima!=0;

	if(tilemode!=fCurmode || fCurtileXRep!=tileXRep || fCurtileYRep != tileYRep)
	{
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		
		if(tilemode && ima!=0)
			glScalef(ima->xrep, ima->yrep, 1.0);

		glMatrixMode(GL_MODELVIEW);
	}

	if(ima==0 || ima->ok==0) {
		glDisable(GL_TEXTURE_2D);
		
		fCurtile= tface->tile;
		fCurpage= 0;
		fCurmode= tilemode;
		fCurtileXRep = tileXRep;
		fCurtileYRep = tileYRep;

		return 0;
	}

	if(ima->ibuf==0) {
		load_image(ima, IB_rect, G.sce, G.scene->r.cfra);
		
		if(ima->ibuf==0) {
			ima->ok= 0;

			fCurtile= tface->tile;
			fCurpage= 0;
			fCurmode= tilemode;
			fCurtileXRep = tileXRep;
			fCurtileYRep = tileYRep;
			
			glDisable(GL_TEXTURE_2D);
			return 0;
		}
		
	}

	if(ima->tpageflag & IMA_TWINANIM) fCurtile= ima->lastframe;
	else fCurtile= tface->tile;

	if(tilemode) {

		if(ima->repbind==0) make_repbind(ima);
		
		if(fCurtile>=ima->totbind) fCurtile= 0;
		
		/* this happens when you change repeat buttons */
		if(ima->repbind) bind= ima->repbind+fCurtile;
		else bind= &ima->bindcode;
		
		if(*bind==0) {
			
			fTexwindx= ima->ibuf->x/ima->xrep;
			fTexwindy= ima->ibuf->y/ima->yrep;
			
			if(fCurtile>=ima->xrep*ima->yrep) fCurtile= ima->xrep*ima->yrep-1;
	
			fTexwinsy= fCurtile / ima->xrep;
			fTexwinsx= fCurtile - fTexwinsy*ima->xrep;
	
			fTexwinsx*= fTexwindx;
			fTexwinsy*= fTexwindy;
	
			tpx= fTexwindx;
			tpy= fTexwindy;

			rect= ima->ibuf->rect + fTexwinsy*ima->ibuf->x + fTexwinsx;
		}
	}
	else {
		bind= &ima->bindcode;
		
		if(*bind==0) {
			tpx= ima->ibuf->x;
			tpy= ima->ibuf->y;
			rect= ima->ibuf->rect;
		}
	}

	if(*bind==0) {
		int rectw= tpx, recth= tpy;
		unsigned int *tilerect= NULL, *scalerect= NULL;

		/*
		 * Maarten:
		 * According to Ton this code is not needed anymore. It was used only
		 * in really old Blenders.
		 * Reevan:
		 * Actually it is needed for backwards compatibility.  Simpledemo 6 does not display correctly without it.
		 */
#if 1
		if (tilemode) {
			int y;
				
			tilerect= MEM_mallocN(rectw*recth*sizeof(*tilerect), "tilerect");
			for (y=0; y<recth; y++) {
				unsigned int *rectrow= &rect[y*ima->ibuf->x];
				unsigned int *tilerectrow= &tilerect[y*rectw];
					
				memcpy(tilerectrow, rectrow, tpx*sizeof(*rectrow));
			}
				
			rect= tilerect;
		}
#endif
		if (!is_pow2(rectw) || !is_pow2(recth)) {
			rectw= smaller_pow2(rectw);
			recth= smaller_pow2(recth);
			
			scalerect= MEM_mallocN(rectw*recth*sizeof(*scalerect), "scalerect");
			gluScaleImage(GL_RGBA, tpx, tpy, GL_UNSIGNED_BYTE, rect, rectw, recth, GL_UNSIGNED_BYTE, scalerect);
			rect= scalerect;
		}

		glGenTextures(1, bind);
		
		if(G.f & G_DEBUG) {
			printf("var1: %s\n", ima->id.name+2);
			printf("var1: %d, var2: %d\n", *bind, tpx);
			printf("var1: %d, var2: %d\n", fCurtile, tilemode);
		}
		glBindTexture( GL_TEXTURE_2D, *bind);

		if (!fDoMipMap)
		{
			glTexImage2D(GL_TEXTURE_2D, 0,  GL_RGBA,  rectw, recth, 0, GL_RGBA, GL_UNSIGNED_BYTE, rect);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		} else
		{
			int minfilter= fLinearMipMap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_NEAREST;
			
			gluBuild2DMipmaps(GL_TEXTURE_2D, 4, rectw, recth, GL_RGBA, GL_UNSIGNED_BYTE, rect);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			
		if (tilerect)
			MEM_freeN(tilerect);
		if (scalerect)
			MEM_freeN(scalerect);
	}
	else glBindTexture( GL_TEXTURE_2D, *bind);
	
	

	glEnable(GL_TEXTURE_2D);

	fCurpage= ima;
	fCurmode= tilemode;
	fCurtileXRep = tileXRep;
	fCurtileYRep = tileYRep;

	return 1;
}

void free_realtime_image(Image *ima)
{
	if(ima->bindcode) {
		glDeleteTextures(1, &ima->bindcode);
		ima->bindcode= 0;
	}
	if(ima->repbind) {
		glDeleteTextures(ima->totbind, ima->repbind);
	
		MEM_freeN(ima->repbind);
		ima->repbind= 0;
	}
}

void free_all_realtime_images(void)
{
	Image* ima;

	ima= G.main->image.first;
	while(ima) {
		free_realtime_image(ima);
		ima= ima->id.next;
	}
}

void make_repbind(Image *ima)
{
	if(ima==0 || ima->ibuf==0) return;

	if(ima->repbind) {
		glDeleteTextures(ima->totbind, ima->repbind);
		MEM_freeN(ima->repbind);
		ima->repbind= 0;
	}
	ima->totbind= ima->xrep*ima->yrep;
	if(ima->totbind>1) {
		ima->repbind= MEM_callocN(sizeof(int)*ima->totbind, "repbind");
	}
}

void update_realtime_textures()
{
	Image *ima;
	
	ima= G.main->image.first;
	while(ima) {
		if(ima->tpageflag & IMA_TWINANIM) {
			if(ima->twend >= ima->xrep*ima->yrep) ima->twend= ima->xrep*ima->yrep-1;
		
			/* check: zit de bindcode niet het array? Vrijgeven. (nog doen) */
			
			ima->lastframe++;
			if(ima->lastframe > ima->twend) ima->lastframe= ima->twsta;
			
		}
		ima= ima->id.next;
	}
}


/* XXX, this routine should die but
 * the great wisdom of NaN has inspired
 * its progation.
 */
void spack(unsigned int ucol)
{
	char *cp= (char *)&ucol;
	
	glColor3ub(cp[3], cp[2], cp[1]);
}

static void draw_hide_tfaces(Object *ob, Mesh *me)
{
	TFace *tface;
	MFace *mface;
	float *v1, *v2, *v3, *v4;
	int a;
	
	if(me==0 || me->tface==0) return;

	mface= me->mface;
	tface= me->tface;

	cpack(0x0);
	setlinestyle(1);
	for(a=me->totface; a>0; a--, mface++, tface++) {
		if(mface->v3==0) continue;
		
		if( (tface->flag & TF_HIDE)) {

			v1= (me->mvert+mface->v1)->co;
			v2= (me->mvert+mface->v2)->co;
			v3= (me->mvert+mface->v3)->co;
			if(mface->v4) v4= (me->mvert+mface->v4)->co; else v4= 0;
		
			glBegin(GL_LINE_LOOP);
				glVertex3fv( v1 );
				glVertex3fv( v2 );
				glVertex3fv( v3 );
				if(mface->v4) glVertex3fv( v4 );
			glEnd();			
		}
	}
	setlinestyle(0);
}


void draw_tfaces3D(Object *ob, Mesh *me)
{
	MFace *mface;
	TFace *tface;
	DispList *dl;
	float *v1, *v2, *v3, *v4, *extverts= NULL;
	int a;
	
	if(me==0 || me->tface==0) return;

	glDisable(GL_DEPTH_TEST);

	mface= me->mface;
	tface= me->tface;

	dl= find_displist(&ob->disp, DL_VERTS);
	if (dl) extverts= dl->verts;
	
	/* SELECT faces */
	for(a=me->totface; a>0; a--, mface++, tface++) {
		if(mface->v3==0) continue;
		if(tface->flag & TF_HIDE) continue;
		
		if( tface->flag & (TF_ACTIVE|TF_SELECT) ) {
			if (extverts) {
				v1= extverts+3*mface->v1;
				v2= extverts+3*mface->v2;
				v3= extverts+3*mface->v3;
				v4= mface->v4?(extverts+3*mface->v4):NULL;
			} else {
				v1= (me->mvert+mface->v1)->co;
				v2= (me->mvert+mface->v2)->co;
				v3= (me->mvert+mface->v3)->co;
				v4= mface->v4?(me->mvert+mface->v4)->co:NULL;
			}

			if(tface->flag & TF_ACTIVE) {
				/* kleuren: R=x G=y */
				cpack(0xFF);
				glBegin(GL_LINE_STRIP); glVertex3fv(v1); if(v4) glVertex3fv(v4); else glVertex3fv(v3); glEnd();
				cpack(0xFF00);
				glBegin(GL_LINE_STRIP); glVertex3fv(v1); glVertex3fv(v2); glEnd();
				cpack(0x0);
				glBegin(GL_LINE_STRIP); glVertex3fv(v2); glVertex3fv(v3); if(v4) glVertex3fv(v4); glEnd();
			}
			else {
				cpack(0x0);
				glBegin(GL_LINE_LOOP);
					glVertex3fv( v1 );
					glVertex3fv( v2 );
					glVertex3fv( v3 );
					if(v4) glVertex3fv( v4 );
				glEnd();
			}
			
			if(tface->flag & TF_SELECT) {
				cpack(0xFFFFFF);
				setlinestyle(1);
				glBegin(GL_LINE_LOOP);
					glVertex3fv( v1 );
					glVertex3fv( v2 );
					glVertex3fv( v3 );
					if(v4) glVertex3fv( v4 );
				glEnd();
				setlinestyle(0);
			}
		}
	}

	glEnable(GL_DEPTH_TEST);
}

static int set_gl_light(Object *ob)
{
	Base *base;
	Lamp *la;
	int count;
	/* float zero[4]= {0.0, 0.0, 0.0, 0.0};  */
	float vec[4];
	
	vec[3]= 1.0;
	
	for(count=0; count<8; count++) glDisable(GL_LIGHT0+count);
	
	count= 0;
	
	base= FIRSTBASE;
	while(base) {
		if(base->object->type==OB_LAMP ) {
			if(base->lay & G.vd->lay) {
				if(base->lay & ob->lay) 
				{
					la= base->object->data;
					
					glPushMatrix();
					glLoadMatrixf((float *)G.vd->viewmat);
					
					where_is_object_simul(base->object);
					VECCOPY(vec, base->object->obmat[3]);
					
					if(la->type==LA_SUN) {
						vec[0]= base->object->obmat[2][0];
						vec[1]= base->object->obmat[2][1];
						vec[2]= base->object->obmat[2][2];
						vec[3]= 0.0;
						glLightfv(GL_LIGHT0+count, GL_POSITION, vec); 
					}
					else {
						vec[3]= 1.0;
						glLightfv(GL_LIGHT0+count, GL_POSITION, vec); 
						glLightf(GL_LIGHT0+count, GL_CONSTANT_ATTENUATION, 1.0);
						glLightf(GL_LIGHT0+count, GL_LINEAR_ATTENUATION, la->att1/la->dist);
						/* without this next line it looks backward compatible. attennuation still is acceptable */
						/* glLightf(GL_LIGHT0+count, GL_QUADRATIC_ATTENUATION, la->att2/(la->dist*la->dist)); */
						
						if(la->type==LA_SPOT) {
							vec[0]= -base->object->obmat[2][0];
							vec[1]= -base->object->obmat[2][1];
							vec[2]= -base->object->obmat[2][2];
							glLightfv(GL_LIGHT0+count, GL_SPOT_DIRECTION, vec);
							glLightf(GL_LIGHT0+count, GL_SPOT_CUTOFF, la->spotsize/2.0);
							glLightf(GL_LIGHT0+count, GL_SPOT_EXPONENT, 128.0*la->spotblend);
						}
						else glLightf(GL_LIGHT0+count, GL_SPOT_CUTOFF, 180.0);
					}
					
					vec[0]= la->energy*la->r;
					vec[1]= la->energy*la->g;
					vec[2]= la->energy*la->b;
					vec[3]= 1.0;
					glLightfv(GL_LIGHT0+count, GL_DIFFUSE, vec); 
					glLightfv(GL_LIGHT0+count, GL_SPECULAR, vec);//zero); 
					glEnable(GL_LIGHT0+count);
					
					glPopMatrix();					
					
					count++;
					if(count>7) break;
				}
			}
		}
		base= base->next;
	}

	return count;
}

static Material *give_current_material_or_def(Object *ob, int matnr)
{
	extern Material defmaterial;
	Material *ma= give_current_material(ob, matnr);

	return ma?ma:&defmaterial;
}

static int set_draw_settings_cached(int clearcache, int textured, TFace *texface, int lit, Object *litob, int litmatnr, int doublesided)
{
	static int c_textured;
	static int c_lit;
	static int c_doublesided;
	static TFace *c_texface;
	static Object *c_litob;
	static int c_litmatnr;
	static int c_badtex;

	if (clearcache) {
		c_textured= c_lit= c_doublesided= -1;
		c_texface= (TFace*) -1;
		c_litob= (Object*) -1;
		c_litmatnr= -1;
		c_badtex= 0;
	}

	if (doublesided!=c_doublesided) {
		if (doublesided) glDisable(GL_CULL_FACE);
		else glEnable(GL_CULL_FACE);

		c_doublesided= doublesided;
	}

	if (textured!=c_textured || texface!=c_texface) {
		if (textured) {
			c_badtex= !set_tpage(texface);
		} else {
			set_tpage(0);
			c_badtex= 0;
		}
		c_textured= textured;
		c_texface= texface;
	}

	if (c_badtex) lit= 0;
	if (lit!=c_lit || litob!=c_litob || litmatnr!=c_litmatnr) {
		if (lit) {
			Material *ma= give_current_material_or_def(litob, litmatnr);
			float spec[4];

			spec[0]= ma->spec*ma->specr;
			spec[1]= ma->spec*ma->specg;
			spec[2]= ma->spec*ma->specb;
			spec[3]= 1.0;

			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glEnable(GL_LIGHTING);
			glEnable(GL_COLOR_MATERIAL);
		}
		else {
			glDisable(GL_LIGHTING); 
			glDisable(GL_COLOR_MATERIAL);
		}
		c_lit= lit;
		c_litob= litob;
		c_litmatnr= litmatnr;
	}

	return c_badtex;
}

void draw_tface_mesh(Object *ob, Mesh *me, int dt)	/* maximum dt: precies volgens ingestelde waardes */
{
	TFace *tface;
	MFace *mface;
	float *extverts= NULL;
	unsigned char obcol[4];
	int a, mode;
	short islight, istex;
	
	if (!me || !me->tface) return;


	glShadeModel(GL_SMOOTH);

	islight= set_gl_light(ob);
	
	obcol[0]= CLAMPIS(ob->col[0]*255, 0, 255);
	obcol[1]= CLAMPIS(ob->col[1]*255, 0, 255);
	obcol[2]= CLAMPIS(ob->col[2]*255, 0, 255);
	obcol[3]= CLAMPIS(ob->col[3]*255, 0, 255);
	
	/* eerst alle texture polys */
	
	glCullFace(GL_BACK); glEnable(GL_CULL_FACE);
	if(G.vd->drawtype==OB_TEXTURE) istex= 1;
	else istex= 0;

	set_draw_settings_cached(1, 0, 0, 0, 0, 0, 0);

	if(dt > OB_SOLID) {
		bProperty *prop = get_property(ob, "Text");
		MFaceInt *mfaceint= NULL;
		int editing= (G.f & (G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT+G_WEIGHTPAINT)) && (ob==((G.scene->basact) ? (G.scene->basact->object) : 0));
		MVert *mvert;
		int totface;

		if (mesh_uses_displist(me) && !editing) {
			DispList *dl= find_displist(&me->disp, DL_MESH);
			DispListMesh *dlm= dl->mesh;
			
			totface= dlm->totface;
			
			if (!dl)
				totface= 0;
			else {
				totface= dlm->totface;
				mvert= dlm->mvert;
				mfaceint= dlm->mface;
				tface= dlm->tface;
			}
		} else {
			DispList *dl= find_displist(&ob->disp, DL_VERTS);
			if (dl) extverts= dl->verts;

			totface= me->totface;
			mvert= me->mvert;
			mface= me->mface;
			tface= me->tface;
		}
		
		for (a=0; a<totface; a++, tface++) {
			int v1idx, v2idx, v3idx, v4idx, mf_smooth, matnr, badtex;
			float *v1, *v2, *v3, *v4;

			if (mfaceint) {
				MFaceInt *mf= &mfaceint[a];

				v1idx= mf->v1;
				v2idx= mf->v2;
				v3idx= mf->v3;
				v4idx= mf->v4;
				mf_smooth= mf->flag & ME_SMOOTH;
				matnr= mf->mat_nr;
			} else {
				MFace *mf= &mface[a];

				v1idx= mf->v1;
				v2idx= mf->v2;
				v3idx= mf->v3;
				v4idx= mf->v4;
				mf_smooth= mf->flag & ME_SMOOTH;
				matnr= mf->mat_nr;
			}
			
			if(v3idx==0) continue;
			if(tface->flag & TF_HIDE) continue;
			if(tface->mode & TF_INVISIBLE) continue;
			
			mode= tface->mode;

			if (extverts) {
				v1= extverts+3*v1idx;
				v2= extverts+3*v2idx;
				v3= extverts+3*v3idx;
				v4= v4idx?(extverts+3*v4idx):NULL;
			} else {
				v1= (mvert+v1idx)->co;
				v2= (mvert+v2idx)->co;
				v3= (mvert+v3idx)->co;
				v4= v4idx?(mvert+v4idx)->co:NULL;
			}

			badtex= set_draw_settings_cached(0, istex && (mode&TF_TEX), tface, islight && (mode&TF_LIGHT), ob, matnr, mode&TF_TWOSIDE);

			if (prop && !badtex && !editing && (mode & TF_BMFONT)) {
				char string[MAX_PROPSTRING];
				int characters, index;
				Image *ima;
				float curpos;

				// The BM_FONT handling code is duplicated in the gameengine
				// Search for 'Frank van Beek' ;-)

				// string = "Frank van Beek";

				set_property_valstr(prop, string);
				characters = strlen(string);
				
				ima = tface->tpage;
				if (ima == NULL) {
					characters = 0;
				}

				if (1 || !mf_smooth) {
					float nor[3];

					CalcNormFloat(v1, v2, v3, nor);

					glNormal3fv(nor);
				}

				curpos= 0.0;
				glBegin(v4?GL_QUADS:GL_TRIANGLES);
				for (index = 0; index < characters; index++) {
					float centerx, centery, sizex, sizey, transx, transy, movex, movey, advance;
					int character = string[index];
					char *cp= NULL;

					// lets calculate offset stuff
					// space starts at offset 1
					// character = character - ' ' + 1;
					
					matrixGlyph(ima->ibuf, character, & centerx, &centery, &sizex, &sizey, &transx, &transy, &movex, &movey, &advance);
					movex+= curpos;

					if (mode & TF_OBCOL) glColor3ubv(obcol);
					else cp= (char *)&(tface->col[0]);

					glTexCoord2f((tface->uv[0][0] - centerx) * sizex + transx, (tface->uv[0][1] - centery) * sizey + transy);
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glVertex3f(sizex * v1[0] + movex, sizey * v1[1] + movey, v1[2]);
					
					glTexCoord2f((tface->uv[1][0] - centerx) * sizex + transx, (tface->uv[1][1] - centery) * sizey + transy);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glVertex3f(sizex * v2[0] + movex, sizey * v2[1] + movey, v2[2]);
		
					glTexCoord2f((tface->uv[2][0] - centerx) * sizex + transx, (tface->uv[2][1] - centery) * sizey + transy);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glVertex3f(sizex * v3[0] + movex, sizey * v3[1] + movey, v3[2]);
		
					if(v4) {
						glTexCoord2f((tface->uv[3][0] - centerx) * sizex + transx, (tface->uv[3][1] - centery) * sizey + transy);
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3f(sizex * v4[0] + movex, sizey * v4[1] + movey, v4[2]);
					}

					curpos+= advance;
				}
				glEnd();
			}
			else {
				char *cp= NULL;
				
				if (badtex) glColor3ub(0xFF, 0x00, 0xFF);
				else if (mode & TF_OBCOL) glColor3ubv(obcol);
				else cp= (char *)&(tface->col[0]);

				if (!mf_smooth) {
					float nor[3];

					CalcNormFloat(v1, v2, v3, nor);

					glNormal3fv(nor);
				}

				glBegin(v4?GL_QUADS:GL_TRIANGLES);

				glTexCoord2fv(tface->uv[0]);
				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				if (mf_smooth) glNormal3sv(mvert[v1idx].no);
				glVertex3fv(v1);
				
				glTexCoord2fv(tface->uv[1]);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				if (mf_smooth) glNormal3sv(mvert[v2idx].no);
				glVertex3fv(v2);

				glTexCoord2fv(tface->uv[2]);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				if (mf_smooth) glNormal3sv(mvert[v3idx].no);
				glVertex3fv(v3);
	
				if(v4) {
					glTexCoord2fv(tface->uv[3]);
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					if (mf_smooth) glNormal3sv(mvert[v4idx].no);
					glVertex3fv(v4);
				}
				glEnd();
			}
		}
		
		/* textures uitzetten */
		set_tpage(0);
	}

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	
	draw_hide_tfaces(ob, me);

	if(ob==OBACT && (G.f & G_FACESELECT)) {
		draw_tfaces3D(ob, me);
	}
	
		/* XXX, bad patch - default_gl_light() calls
		 * glLightfv(GL_LIGHT_POSITION, ...) which
		 * is transformed by the current matrix... we
		 * need to make sure that matrix is identity.
		 * 
		 * It would be better if drawmesh.c kept track
		 * of and restored the light settings it changed.
		 *  - zr
		 */
	glPushMatrix();
	glLoadIdentity();	
	default_gl_light();
	glPopMatrix();
}

void init_realtime_GL(void)
{		
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

}

