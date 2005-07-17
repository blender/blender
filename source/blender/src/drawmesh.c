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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"

#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_bmfont.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_property.h"
#include "BKE_utildefines.h"

#include "BIF_resources.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"

#include "BDR_editface.h"
#include "BDR_vpaint.h"
#include "BDR_drawmesh.h"

#include "BSE_drawview.h"

#include "blendef.h"
#include "nla.h"

//#include "glext.h"
/* some local functions */
#if defined(GL_EXT_texture_object) && (!defined(__sun__) || (!defined(__sun))) && !defined(__APPLE__) && !defined(__linux__) && !defined(WIN32)
	#define glBindTexture(A,B)     glBindTextureEXT(A,B)
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

/* local prototypes --------------- */
void update_realtime_textures(void);


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
	unsigned int *rect=NULL, *bind;
	int tpx=0, tpy=0, tilemode, tileXRep,tileYRep;
	
	/* disable */
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

		glGenTextures(1, (GLuint *)bind);
		
		if((G.f & G_DEBUG) || !*bind) {
			GLenum error = glGetError();
			printf("Texture: %s\n", ima->id.name+2);
			printf("name: %d, tpx: %d\n", *bind, tpx);
			printf("tile: %d, mode: %d\n", fCurtile, tilemode);
			if (error)
				printf("error: %s\n", gluErrorString(error));
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
			
			gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, rectw, recth, GL_RGBA, GL_UNSIGNED_BYTE, rect);
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
	
	tag_image_time(ima);

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
		glDeleteTextures(1, (GLuint *)&ima->bindcode);
		ima->bindcode= 0;
	}
	if(ima->repbind) {
		glDeleteTextures(ima->totbind, (GLuint *)ima->repbind);
	
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
		glDeleteTextures(ima->totbind, (GLuint *)ima->repbind);
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
		
			/* check: is bindcode not in the array? Free. (to do) */
			
			ima->lastframe++;
			if(ima->lastframe > ima->twend) ima->lastframe= ima->twsta;
			
		}
		ima= ima->id.next;
	}
}


void spack(unsigned int ucol)
{
	char *cp= (char *)&ucol;
	
	glColor3ub(cp[3], cp[2], cp[1]);
}

void draw_tfaces3D(Object *ob, Mesh *me)
{
	MFace *mface, *activeFace;
	TFace *tface;
	float co[3];
	int a, activeFaceInSelection= 0;
	DerivedMesh *dm;
	int dmNeedsFree;
	
	if(me==0 || me->tface==0) return;

	dm = mesh_get_derived_deform(ob, &dmNeedsFree);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	bglPolygonOffset(1.0);

#define PASSVERT(index)		{ dm->getVertCo(dm, index, co); glVertex3fv(co); }

	/* Draw (Hidden) Edges */
	if(G.f & G_DRAWEDGES || G.f & G_HIDDENEDGES){ 
		BIF_ThemeColor(TH_EDGE_FACESEL);

		mface= me->mface;
		tface= me->tface;
		for(a=me->totface; a>0; a--, mface++, tface++) {
			if(mface->v3 && (G.f&G_DRAWEDGES)) {
				if ((tface->flag&TF_HIDE) && !(G.f&G_HIDDENEDGES))
					continue;
			
				glBegin(GL_LINE_LOOP);
					PASSVERT(mface->v1);
					PASSVERT(mface->v2);
					PASSVERT(mface->v3);
					if(mface->v4) PASSVERT(mface->v4);
				glEnd();
			}
		}
	}

	if(G.f & G_DRAWSEAMS) {
		BIF_ThemeColor(TH_EDGE_SEAM);
		glLineWidth(2);

		glBegin(GL_LINES);
		mface= me->mface;
		tface= me->tface;
		for(a=me->totface; a>0; a--, mface++, tface++) {
			if (mface->v3 && !(tface->flag&TF_HIDE)) {
				if(tface->unwrap & TF_SEAM1) {
					PASSVERT(mface->v1);
					PASSVERT(mface->v2);
				}

				if(tface->unwrap & TF_SEAM2) {
					PASSVERT(mface->v2);
					PASSVERT(mface->v3);
				}

				if(tface->unwrap & TF_SEAM3) {
					PASSVERT(mface->v3);
					PASSVERT(mface->v4?mface->v4:mface->v1);
				}

				if(mface->v4 && (tface->unwrap & TF_SEAM4)) {
					PASSVERT(mface->v4);
					PASSVERT(mface->v1);
				}
			}
		}
		glEnd();

		glLineWidth(1);
	}

	/* Draw Selected Faces in transparent purple */
	if(G.f & G_DRAWFACES) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		BIF_ThemeColor4(TH_FACE_SELECT);

		mface= me->mface;
		tface= me->tface;
		for(a=me->totface; a>0; a--, mface++, tface++) {
			if(mface->v3 && !(tface->flag&TF_HIDE) && (tface->flag&TF_SELECT)) {
				glBegin(mface->v4?GL_QUADS:GL_TRIANGLES);
					PASSVERT(mface->v1);
					PASSVERT(mface->v2);
					PASSVERT(mface->v3);
					if(mface->v4) PASSVERT(mface->v4);
				glEnd();
			}
		}
		glDisable(GL_BLEND);
	}
	
	/* Draw Stippled Outline for selected faces */
	activeFace = NULL;
	mface= me->mface;
	tface= me->tface;
	bglPolygonOffset(1.0);
	for(a=me->totface; a>0; a--, mface++, tface++) {
		if(mface->v3 && !(tface->flag&TF_HIDE) && (tface->flag & (TF_ACTIVE|TF_SELECT))) {
			if(tface->flag & TF_ACTIVE) {
				activeFace = mface;
				activeFaceInSelection= (tface->flag & TF_SELECT);
			}
			else {
				cpack(0x0);
				glBegin(GL_LINE_LOOP);
					PASSVERT(mface->v1);
					PASSVERT(mface->v2);
					PASSVERT(mface->v3);
					if(mface->v4) PASSVERT(mface->v4);
				glEnd();
			}

			if(tface->flag & TF_SELECT) {
				cpack(0xFFFFFF);
				setlinestyle(1);
				glBegin(GL_LINE_LOOP);
					PASSVERT(mface->v1);
					PASSVERT(mface->v2);
					PASSVERT(mface->v3);
					if(mface->v4) PASSVERT(mface->v4);
				glEnd();
				setlinestyle(0);
			}
		}
	}

	/* Draw Active Face on top */
	/* colors: R=x G=y */
	if(activeFace) {
		cpack(0xFF);
		glBegin(GL_LINE_STRIP);
			PASSVERT(activeFace->v1);
			PASSVERT(activeFace->v4?activeFace->v4:activeFace->v3);
		glEnd();

		cpack(0xFF00);
		glBegin(GL_LINE_STRIP);
			PASSVERT(activeFace->v1);
			PASSVERT(activeFace->v2);
		glEnd();

		if(activeFaceInSelection) cpack(0x00FFFF);
		else cpack(0xFF00FF);

		glBegin(GL_LINE_STRIP);
			PASSVERT(activeFace->v2);
			PASSVERT(activeFace->v3);
			if(activeFace->v4) PASSVERT(activeFace->v4);
		glEnd();
		setlinestyle(0);
	}

	bglPolygonOffset(0.0);	// resets correctly now, even after calling accumulated offsets

	if (dmNeedsFree)
		dm->release(dm);
#undef PASSVERT
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
						/* post 2.25 engine supports quad lights */
						glLightf(GL_LIGHT0+count, GL_QUADRATIC_ATTENUATION, la->att2/(la->dist*la->dist));
						
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
	extern Material defmaterial;	// render module abuse...
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

	if (texface) {
		lit = lit && (texface->mode&TF_LIGHT);
		textured = textured && (texface->mode&TF_TEX);
		doublesided = texface->mode&TF_TWOSIDE;
	} else {
		textured = 0;
	}

	if (doublesided!=c_doublesided) {
		if (doublesided) glDisable(GL_CULL_FACE);
		else glEnable(GL_CULL_FACE);

		c_doublesided= doublesided;
	}

	if (textured!=c_textured || texface!=c_texface) {
		if (textured ) {
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

/* Icky globals, fix with userdata parameter */

static Object *g_draw_tface_mesh_ob = NULL;
static int g_draw_tface_mesh_islight = 0;
static int g_draw_tface_mesh_istex = 0;
static unsigned char g_draw_tface_mesh_obcol[4];
static int draw_tface_mesh__set_draw(TFace *tface, int matnr)
{
	if (set_draw_settings_cached(0, g_draw_tface_mesh_istex, tface, g_draw_tface_mesh_islight, g_draw_tface_mesh_ob, matnr, TF_TWOSIDE)) {
		glColor3ub(0xFF, 0x00, 0xFF);
		return 0; /* Don't set color */
	} else if (tface && tface->mode&TF_OBCOL) {
		glColor3ubv(g_draw_tface_mesh_obcol);
		return 0; /* Don't set color */
	} else if (!tface) {
		Material *ma= give_current_material(g_draw_tface_mesh_ob, matnr);
		if(ma) glColor3f(ma->r, ma->g, ma->b);
		else glColor3f(0.5, 0.5, 0.5);
		return 1; /* Set color from mcol if available */
	} else {
		return 1; /* Set color from tface */
	}
}
void draw_tface_mesh(Object *ob, Mesh *me, int dt)
/* maximum dt (drawtype): exactly according values that have been set */
{
	TFace *tface=NULL;
	MFace *mface=NULL;
	float *extverts= NULL;
	unsigned char obcol[4];
	int a;
	short islight, istex;
	
	if(me==NULL) return;


	glShadeModel(GL_SMOOTH);

	islight= set_gl_light(ob);
	
	obcol[0]= CLAMPIS(ob->col[0]*255, 0, 255);
	obcol[1]= CLAMPIS(ob->col[1]*255, 0, 255);
	obcol[2]= CLAMPIS(ob->col[2]*255, 0, 255);
	obcol[3]= CLAMPIS(ob->col[3]*255, 0, 255);
	
	/* first all texture polys */
	
	if(ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	glCullFace(GL_BACK); glEnable(GL_CULL_FACE);
	if(G.vd->drawtype==OB_TEXTURE) istex= 1;
	else istex= 0;

	g_draw_tface_mesh_ob = ob;
	g_draw_tface_mesh_islight = islight;
	g_draw_tface_mesh_istex = istex;
	memcpy(g_draw_tface_mesh_obcol, obcol, sizeof(obcol));
	set_draw_settings_cached(1, 0, 0, 0, 0, 0, 0);

	if(dt > OB_SOLID) {
		bProperty *prop = get_property(ob, "Text");
		int editing= (G.f & (G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT+G_WEIGHTPAINT)) && (ob==((G.scene->basact) ? (G.scene->basact->object) : 0));
		DerivedMesh *dm;
		int start, totface;

		if(mesh_uses_displist(me) && editing==0) {
			dm = mesh_get_derived(ob);
			dm->drawFacesTex(dm, draw_tface_mesh__set_draw);
		} else {
			dm = mesh_get_base_derived(ob);
			dm->drawFacesTex(dm, draw_tface_mesh__set_draw);
			dm->release(dm);
		}

		start = 0;
		totface = me->totface;
		set_buildvars(ob, &start, &totface);

		if (!editing && !mesh_uses_displist(me) && prop && tface) {
			tface+= start;
			for (a=start; a<totface; a++, tface++) {
				MFace *mf= &mface[a];
				int mode= tface->mode;
				int matnr= mf->mat_nr;
				int mf_smooth= mf->flag & ME_SMOOTH;

				if (mf->v3 && !(tface->flag&TF_HIDE) && !(mode&TF_INVISIBLE) && (mode&TF_BMFONT)) {
					int badtex= set_draw_settings_cached(0, g_draw_tface_mesh_istex, tface, g_draw_tface_mesh_islight, g_draw_tface_mesh_ob, matnr, TF_TWOSIDE);
					float *v1, *v2, *v3, *v4;
					char string[MAX_PROPSTRING];
					int characters, index;
					Image *ima;
					float curpos;

					if (badtex)
						continue;

					if (extverts) {
						v1= extverts+3*mf->v1;
						v2= extverts+3*mf->v2;
						v3= extverts+3*mf->v3;
						v4= mf->v4?(extverts+3*mf->v4):NULL;
					} else {
						v1= (me->mvert+mf->v1)->co;
						v2= (me->mvert+mf->v2)->co;
						v3= (me->mvert+mf->v3)->co;
						v4= mf->v4?(me->mvert+mf->v4)->co:NULL;
					}

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

						if (tface->mode & TF_OBCOL) glColor3ubv(obcol);
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
			}
		}

		/* switch off textures */
		set_tpage(0);
	}
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	
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
	
	glFrontFace(GL_CCW);

}

void init_realtime_GL(void)
{		
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

}

