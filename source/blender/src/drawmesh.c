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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "IMB_imbuf.h"
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
#include "DNA_userdef_types.h"

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

#include "BIF_editmesh.h"
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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                        0x812F
#endif

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

/* These are used to enable texture clamping */
static int is_pow2_limit(int num) {
	if (U.glreslimit != 0 && num > U.glreslimit) return 0;
	return ((num)&(num-1))==0;
}

static int smaller_pow2_limit(int num) {
	if (U.glreslimit != 0 && num > U.glreslimit)
		return U.glreslimit;
	return smaller_pow2(num);
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
static int get_mipmap(void)
{
	return fDoMipMap && (!(G.f & G_TEXTUREPAINT));
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
int set_tpage(MTFace *tface)
{	
	static int alphamode= -1;
	static MTFace *lasttface= 0;
	Image *ima;
	ImBuf *ibuf;
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
		glDisable ( GL_ALPHA_TEST );
		return 0;
	}
	lasttface= tface;

	if( alphamode != tface->transp) {
		alphamode= tface->transp;

		if(alphamode) {
			if(alphamode==TF_ADD) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glDisable ( GL_ALPHA_TEST );
			/* 	glBlendEquationEXT(GL_FUNC_ADD_EXT); */
			}
			else if(alphamode==TF_ALPHA) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				
				/* added after 2.45 to clip alpha */
				
				/*if U.glalphaclip == 1.0, some cards go bonkers... turn off alpha test in this case*/
				if(U.glalphaclip == 1.0) glDisable(GL_ALPHA_TEST);
				else{
					glEnable ( GL_ALPHA_TEST );
					glAlphaFunc ( GL_GREATER, U.glalphaclip );
				}
			} else if (alphamode==TF_CLIP){		
				glDisable(GL_BLEND); 
				glEnable ( GL_ALPHA_TEST );
				glAlphaFunc(GL_GREATER, 0.5f);
			}
			/* 	glBlendEquationEXT(GL_FUNC_ADD_EXT); */
			/* else { */
			/* 	glBlendFunc(GL_ONE, GL_ONE); */
			/* 	glBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT); */
			/* } */
		} else {
			glDisable(GL_BLEND);
			glDisable ( GL_ALPHA_TEST );
		}
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
	if (ima) {
		tileXRep = ima->xrep;
		tileYRep = ima->yrep;
	}


	if(ima==fCurpage && fCurtile==tface->tile && tilemode==fCurmode && fCurtileXRep==tileXRep && fCurtileYRep == tileYRep) return ima!=0;

	if(tilemode!=fCurmode || fCurtileXRep!=tileXRep || fCurtileYRep != tileYRep) {
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		
		if(tilemode && ima!=NULL)
			glScalef(ima->xrep, ima->yrep, 1.0);

		glMatrixMode(GL_MODELVIEW);
	}

	if(ima==NULL || ima->ok==0) {
		glDisable(GL_TEXTURE_2D);
		
		fCurtile= tface->tile;
		fCurpage= 0;
		fCurmode= tilemode;
		fCurtileXRep = tileXRep;
		fCurtileYRep = tileYRep;

		return 0;
	}

	ibuf= BKE_image_get_ibuf(ima, NULL);
	if(ibuf==NULL) {

		fCurtile= tface->tile;
		fCurpage= 0;
		fCurmode= tilemode;
		fCurtileXRep = tileXRep;
		fCurtileYRep = tileYRep;
		
		glDisable(GL_TEXTURE_2D);
		return 0;
	}

	if ((ibuf->rect==NULL) && ibuf->rect_float)
		IMB_rect_from_float(ibuf);

	if(ima->tpageflag & IMA_TWINANIM) fCurtile= ima->lastframe;
	else fCurtile= tface->tile;

	if(tilemode) {
		if(ima->repbind==0) make_repbind(ima);
		
		if(fCurtile>=ima->totbind) fCurtile= 0;
		
		/* this happens when you change repeat buttons */
		if(ima->repbind) bind= ima->repbind+fCurtile;
		else bind= &ima->bindcode;
		
		if(*bind==0) {
			
			fTexwindx= ibuf->x/ima->xrep;
			fTexwindy= ibuf->y/ima->yrep;
			
			if(fCurtile>=ima->xrep*ima->yrep) fCurtile= ima->xrep*ima->yrep-1;
	
			fTexwinsy= fCurtile / ima->xrep;
			fTexwinsx= fCurtile - fTexwinsy*ima->xrep;
	
			fTexwinsx*= fTexwindx;
			fTexwinsy*= fTexwindy;
	
			tpx= fTexwindx;
			tpy= fTexwindy;

			rect= ibuf->rect + fTexwinsy*ibuf->x + fTexwinsx;
		}
	}
	else {
		bind= &ima->bindcode;
		
		if(*bind==0) {
			tpx= ibuf->x;
			tpy= ibuf->y;
			rect= ibuf->rect;
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
				unsigned int *rectrow= &rect[y*ibuf->x];
				unsigned int *tilerectrow= &tilerect[y*rectw];
					
				memcpy(tilerectrow, rectrow, tpx*sizeof(*rectrow));
			}
				
			rect= tilerect;
		}
#endif
		if (!is_pow2_limit(rectw) || !is_pow2_limit(recth)) {
			rectw= smaller_pow2_limit(rectw);
			recth= smaller_pow2_limit(recth);
			
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

		if (!get_mipmap())
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

			ima->tpageflag |= IMA_MIPMAP_COMPLETE;
		}

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			
		if (tilerect)
			MEM_freeN(tilerect);
		if (scalerect)
			MEM_freeN(scalerect);
	}
	else glBindTexture( GL_TEXTURE_2D, *bind);
	
	/* dont tile x/y as set the the game properties */
	if (ima->tpageflag & IMA_CLAMP_U)
	   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	if (ima->tpageflag & IMA_CLAMP_V)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	/* tag_image_time(ima);*/  /* Did this get lost in the image recode? */
	
	glEnable(GL_TEXTURE_2D);

	fCurpage= ima;
	fCurmode= tilemode;
	fCurtileXRep = tileXRep;
	fCurtileYRep = tileYRep;

	return 1;
}

void update_realtime_image(Image *ima, int x, int y, int w, int h)
{
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	
	if (ima->repbind || get_mipmap() || !ima->bindcode || !ibuf ||
		(!is_pow2(ibuf->x) || !is_pow2(ibuf->y)) ||
		(w == 0) || (h == 0)) {
		/* these special cases require full reload still */
		free_realtime_image(ima);
	}
	else {
		int row_length = glaGetOneInteger(GL_UNPACK_ROW_LENGTH);
		int skip_pixels = glaGetOneInteger(GL_UNPACK_SKIP_PIXELS);
		int skip_rows = glaGetOneInteger(GL_UNPACK_SKIP_ROWS);

		if ((ibuf->rect==NULL) && ibuf->rect_float)
			IMB_rect_from_float(ibuf);

		glBindTexture(GL_TEXTURE_2D, ima->bindcode);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, ibuf->x);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA,
			GL_UNSIGNED_BYTE, ibuf->rect);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);

		if(ima->tpageflag & IMA_MIPMAP_COMPLETE)
			ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
	}
}

void free_realtime_image(Image *ima)
{
	if(ima->bindcode) {
		glDeleteTextures(1, (GLuint *)&ima->bindcode);
		ima->bindcode= 0;
		ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
	}
	if(ima->repbind) {
		glDeleteTextures(ima->totbind, (GLuint *)ima->repbind);
	
		MEM_freeN(ima->repbind);
		ima->repbind= NULL;
		ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
	}
}

void free_all_realtime_images(void)
{
	Image* ima;

	for(ima=G.main->image.first; ima; ima=ima->id.next)
		free_realtime_image(ima);
}

/* these two functions are called on entering and exiting texture paint mode,
   temporary disabling/enabling mipmapping on all images for quick texture
   updates with glTexSubImage2D. images that didn't change don't have to be
   re-uploaded to OpenGL */
void texpaint_disable_mipmap(void)
{
	Image* ima;
	
	if(!fDoMipMap)
		return;

	for(ima=G.main->image.first; ima; ima=ima->id.next) {
		if(ima->bindcode) {
			glBindTexture(GL_TEXTURE_2D, ima->bindcode);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
	}
}

void texpaint_enable_mipmap(void)
{
	Image* ima;

	if(!fDoMipMap)
		return;

	for(ima=G.main->image.first; ima; ima=ima->id.next) {
		if(ima->bindcode) {
			if(ima->tpageflag & IMA_MIPMAP_COMPLETE) {
				int minfilter= fLinearMipMap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_NEAREST;

				glBindTexture(GL_TEXTURE_2D, ima->bindcode);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			else
				free_realtime_image(ima);
		}
	}
}

void make_repbind(Image *ima)
{
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	
	if(ibuf==NULL) return;

	if(ima->repbind) {
		glDeleteTextures(ima->totbind, (GLuint *)ima->repbind);
		MEM_freeN(ima->repbind);
		ima->repbind= 0;
		ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
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

/***/

	/* Flags for marked edges */
enum {
	eEdge_Visible = (1<<0),
	eEdge_Select = (1<<1),
};

	/* Creates a hash of edges to flags indicating
	 * adjacent tface select/active/etc flags.
	 */
static void get_marked_edge_info__orFlags(EdgeHash *eh, int v0, int v1, int flags)
{
	int *flags_p;

	if (!BLI_edgehash_haskey(eh, v0, v1)) {
		BLI_edgehash_insert(eh, v0, v1, 0);
	}

	flags_p = (int*) BLI_edgehash_lookup_p(eh, v0, v1);
	*flags_p |= flags;
}

EdgeHash *get_tface_mesh_marked_edge_info(Mesh *me)
{
	EdgeHash *eh = BLI_edgehash_new();
	int i;
	MFace *mf;
	MTFace *tf = NULL;
	
	for (i=0; i<me->totface; i++) {
		mf = &me->mface[i];
		if (me->mtface)
			tf = &me->mtface[i];
		
		if (mf->v3) {
			if (!(mf->flag&ME_HIDE)) {
				unsigned int flags = eEdge_Visible;
				if (mf->flag&ME_FACE_SEL) flags |= eEdge_Select;

				get_marked_edge_info__orFlags(eh, mf->v1, mf->v2, flags);
				get_marked_edge_info__orFlags(eh, mf->v2, mf->v3, flags);
				if (mf->v4) {
					get_marked_edge_info__orFlags(eh, mf->v3, mf->v4, flags);
					get_marked_edge_info__orFlags(eh, mf->v4, mf->v1, flags);
				} else {
					get_marked_edge_info__orFlags(eh, mf->v3, mf->v1, flags);
				}
			}
		}
	}

	return eh;
}


static int draw_tfaces3D__setHiddenOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	MEdge *med = &data->me->medge[index];
	unsigned long flags = (long) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if((G.f & G_DRAWSEAMS) && (med->flag&ME_SEAM)) {
		return 0;
	} else if(G.f & G_DRAWEDGES){ 
		if (G.f&G_HIDDENEDGES) {
			return 1;
		} else {
			return (flags & eEdge_Visible);
		}
	} else {
		return (flags & eEdge_Select);
	}
}
static int draw_tfaces3D__setSeamOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	MEdge *med = &data->me->medge[index];
	unsigned long flags = (long) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if (med->flag&ME_SEAM) {
		if (G.f&G_HIDDENEDGES) {
			return 1;
		} else {
			return (flags & eEdge_Visible);
		}
	} else {
		return 0;
	}
}
static int draw_tfaces3D__setSelectOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	MEdge *med = &data->me->medge[index];
	unsigned long flags = (long) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	return flags & eEdge_Select;
}
static int draw_tfaces3D__setActiveOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; } *data = userData;
	MEdge *med = &data->me->medge[index];
	unsigned long flags = (long) BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if (flags & eEdge_Select) {
		return 1;
	} else {
		return 0;
	}
}
static int draw_tfaces3D__drawFaceOpts(void *userData, int index)
{
	Mesh *me = (Mesh*)userData;

	MFace *mface = &me->mface[index];
	if (!(mface->flag&ME_HIDE) && (mface->flag&ME_FACE_SEL))
		return 2; /* Don't set color */
	else
		return 0;
}
static void draw_tfaces3D(Object *ob, Mesh *me, DerivedMesh *dm)
{
	struct { Mesh *me; EdgeHash *eh; } data;

	data.me = me;
	data.eh = get_tface_mesh_marked_edge_info(me);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	bglPolygonOffset(1.0);

		/* Draw (Hidden) Edges */
	BIF_ThemeColor(TH_EDGE_FACESEL);
	dm->drawMappedEdges(dm, draw_tfaces3D__setHiddenOpts, &data);

		/* Draw Seams */
	if(G.f & G_DRAWSEAMS) {
		BIF_ThemeColor(TH_EDGE_SEAM);
		glLineWidth(2);

		dm->drawMappedEdges(dm, draw_tfaces3D__setSeamOpts, &data);

		glLineWidth(1);
	}

	/* Draw Selected Faces */
	if(G.f & G_DRAWFACES) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		BIF_ThemeColor4(TH_FACE_SELECT);

		dm->drawMappedFacesTex(dm, draw_tfaces3D__drawFaceOpts, (void*)me);

		glDisable(GL_BLEND);
	}
	
	bglPolygonOffset(1.0);

		/* Draw Stippled Outline for selected faces */
	glColor3ub(255, 255, 255);
	setlinestyle(1);
	dm->drawMappedEdges(dm, draw_tfaces3D__setSelectOpts, &data);
	setlinestyle(0);

	dm->drawMappedEdges(dm, draw_tfaces3D__setActiveOpts, &data);

	bglPolygonOffset(0.0);	// resets correctly now, even after calling accumulated offsets

	BLI_edgehash_free(data.eh, NULL);
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

static int set_draw_settings_cached(int clearcache, int textured, MTFace *texface, int lit, Object *litob, int litmatnr, int doublesided)
{
	static int c_textured;
	static int c_lit;
	static int c_doublesided;
	static MTFace *c_texface;
	static Object *c_litob;
	static int c_litmatnr;
	static int c_badtex;

	if (clearcache) {
		c_textured= c_lit= c_doublesided= -1;
		c_texface= (MTFace*) -1;
		c_litob= (Object*) -1;
		c_litmatnr= -1;
		c_badtex= 0;
	}

	if (texface) {
		lit = lit && (lit==-1 || texface->mode&TF_LIGHT);
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
			Material *ma= give_current_material_or_def(litob, litmatnr+1);
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

struct TextureDrawState {
	Object *ob;
	int islit, istex;
	unsigned char obcol[4];
} Gtexdraw = {NULL, 0, 0, {0, 0, 0, 0}};

static void draw_textured_begin(Object *ob)
{
	unsigned char obcol[4];
	int istex, solidtex= 0;

	if(G.vd->drawtype==OB_SOLID || (ob==G.obedit && G.vd->drawtype!=OB_TEXTURE)) {
		/* draw with default lights in solid draw mode and edit mode */
		solidtex= 1;
		Gtexdraw.islit= -1;
	}
	else
		/* draw with lights in the scene otherwise */
		Gtexdraw.islit= set_gl_light(ob);
	
	obcol[0]= CLAMPIS(ob->col[0]*255, 0, 255);
	obcol[1]= CLAMPIS(ob->col[1]*255, 0, 255);
	obcol[2]= CLAMPIS(ob->col[2]*255, 0, 255);
	obcol[3]= CLAMPIS(ob->col[3]*255, 0, 255);
	
	glCullFace(GL_BACK); glEnable(GL_CULL_FACE);
	if(solidtex || G.vd->drawtype==OB_TEXTURE) istex= 1;
	else istex= 0;

	Gtexdraw.ob = ob;
	Gtexdraw.istex = istex;
	memcpy(Gtexdraw.obcol, obcol, sizeof(obcol));
	set_draw_settings_cached(1, 0, 0, Gtexdraw.islit, 0, 0, 0);
	glShadeModel(GL_SMOOTH);
}

static void draw_textured_end()
{
	/* switch off textures */
	set_tpage(0);

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);

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


static int draw_tface__set_draw(MTFace *tface, MCol *mcol, int matnr)
{
	if (tface && (tface->mode&TF_INVISIBLE)) return 0;

	if (tface && set_draw_settings_cached(0, Gtexdraw.istex, tface, Gtexdraw.islit, Gtexdraw.ob, matnr, TF_TWOSIDE)) {
		glColor3ub(0xFF, 0x00, 0xFF);
		return 2; /* Don't set color */
	} else if (tface && tface->mode&TF_OBCOL) {
		glColor3ubv(Gtexdraw.obcol);
		return 2; /* Don't set color */
	} else if (!mcol) {
		if (tface) glColor3f(1.0, 1.0, 1.0);
		else {
			Material *ma= give_current_material(Gtexdraw.ob, matnr+1);
			if(ma) glColor3f(ma->r, ma->g, ma->b);
			else glColor3f(1.0, 1.0, 1.0);
		}
		return 2; /* Don't set color */
	} else {
		return 1; /* Set color from mcol */
	}
}

static int draw_tface_mapped__set_draw(void *userData, int index)
{
	Mesh *me = (Mesh*)userData;
	MTFace *tface = (me->mtface)? &me->mtface[index]: NULL;
	MFace *mface = (me->mface)? &me->mface[index]: NULL;
	MCol *mcol = (me->mcol)? &me->mcol[index]: NULL;
	int matnr = me->mface[index].mat_nr;
	if (mface && mface->flag&ME_HIDE) return 0;
	return draw_tface__set_draw(tface, mcol, matnr);
}

static int draw_em_tf_mapped__set_draw(void *userData, int index)
{
	EditMesh *em = userData;
	EditFace *efa = EM_get_face_for_index(index);
	MTFace *tface;
	MCol *mcol;
	int matnr;

	if (efa==NULL || efa->h)
		return 0;

	tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
	mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
	matnr = efa->mat_nr;

	return draw_tface__set_draw(tface, mcol, matnr);
}

static int wpaint__setSolidDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	Mesh *me = (Mesh*)userData;
	MTFace *tface = (me->mtface)? &me->mtface[index]: NULL;
	MFace *mface = (me->mface)? &me->mface[index]: NULL;
	
	if ((mface->flag&ME_HIDE) || (tface && (tface->mode&TF_INVISIBLE))) 
			return 0;
	
	*drawSmooth_r = 1;
	return 1;
}

static void draw_game_text_mesh(Object *ob, Mesh *me)
{
	DerivedMesh *ddm = mesh_get_derived_deform(ob, CD_MASK_BAREMESH);
	MFace *mface= me->mface;
	MTFace *tface= me->mtface;
	MCol *mcol= me->mcol;	/* why does mcol exist? */
	bProperty *prop = get_property(ob, "Text");
	int a, start= 0, totface= me->totface;

	tface+= start;
	mcol+= start*4;
	for (a=start; a<totface; a++, tface++, mcol+=4) {
		MFace *mf= &mface[a];
		int mode= tface->mode;
		int matnr= mf->mat_nr;
		int mf_smooth= mf->flag & ME_SMOOTH;

		if (!(mf->flag&ME_HIDE) && !(mode&TF_INVISIBLE) && (mode&TF_BMFONT)) {
			int badtex= set_draw_settings_cached(0, Gtexdraw.istex, tface, Gtexdraw.islit, Gtexdraw.ob, matnr, TF_TWOSIDE);
			float v1[3], v2[3], v3[3], v4[3];
			char string[MAX_PROPSTRING];
			int characters, index;
			ImBuf *ibuf;
			float curpos;

			if (badtex)
				continue;

			ddm->getVertCo(ddm, mf->v1, v1);
			ddm->getVertCo(ddm, mf->v2, v2);
			ddm->getVertCo(ddm, mf->v3, v3);
			if (mf->v4) ddm->getVertCo(ddm, mf->v4, v4);

			// The BM_FONT handling code is duplicated in the gameengine
			// Search for 'Frank van Beek' ;-)
			// string = "Frank van Beek";

			set_property_valstr(prop, string);
			characters = strlen(string);
			
			ibuf= BKE_image_get_ibuf(tface->tpage, NULL);
			if (ibuf == NULL) {
				characters = 0;
			}

			if (!mf_smooth) {
				float nor[3];

				CalcNormFloat(v1, v2, v3, nor);

				glNormal3fv(nor);
			}

			curpos= 0.0;
			glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);
			for (index = 0; index < characters; index++) {
				float centerx, centery, sizex, sizey, transx, transy, movex, movey, advance;
				int character = string[index];
				char *cp= NULL;

				// lets calculate offset stuff
				// space starts at offset 1
				// character = character - ' ' + 1;
				
				matrixGlyph(ibuf, character, & centerx, &centery, &sizex, &sizey, &transx, &transy, &movex, &movey, &advance);
				movex+= curpos;

				if (tface->mode & TF_OBCOL)
					glColor3ubv(Gtexdraw.obcol);
				else if (me->mcol) cp= (char *)mcol;
				else glColor3ub(255, 255, 255);

				glTexCoord2f((tface->uv[0][0] - centerx) * sizex + transx, (tface->uv[0][1] - centery) * sizey + transy);
				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				glVertex3f(sizex * v1[0] + movex, sizey * v1[1] + movey, v1[2]);
				
				glTexCoord2f((tface->uv[1][0] - centerx) * sizex + transx, (tface->uv[1][1] - centery) * sizey + transy);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				glVertex3f(sizex * v2[0] + movex, sizey * v2[1] + movey, v2[2]);
	
				glTexCoord2f((tface->uv[2][0] - centerx) * sizex + transx, (tface->uv[2][1] - centery) * sizey + transy);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				glVertex3f(sizex * v3[0] + movex, sizey * v3[1] + movey, v3[2]);
	
				if(mf->v4) {
					glTexCoord2f((tface->uv[3][0] - centerx) * sizex + transx, (tface->uv[3][1] - centery) * sizey + transy);
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					glVertex3f(sizex * v4[0] + movex, sizey * v4[1] + movey, v4[2]);
				}

				curpos+= advance;
			}
			glEnd();
		}
	}

	ddm->release(ddm);
}

void draw_mesh_textured(Object *ob, DerivedMesh *dm, int faceselect)
{
	Mesh *me= ob->data;
	int editing= 0;
	
	/* correct for negative scale */
	if(ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	/* draw the textured mesh */
	draw_textured_begin(ob);

#ifdef WITH_VERSE
	if(me->vnode) {
		/* verse-blender doesn't support uv mapping of textures yet */
		dm->drawFacesTex(dm, NULL);
	}
	else {
#endif
		if(ob==G.obedit) {
			dm->drawMappedFacesTex(dm, draw_em_tf_mapped__set_draw, G.editMesh);
		} else if(faceselect) {
			if(G.f & G_WEIGHTPAINT)
				dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, me, 1);
			else
				dm->drawMappedFacesTex(dm, draw_tface_mapped__set_draw, me);
		}
		else
			dm->drawFacesTex(dm, draw_tface__set_draw);
#ifdef WITH_VERSE
	}
#endif

	/* draw game engine text hack - but not if we are editing the mesh */
	if (me->mtface && get_property(ob, "Text")) {
		if(ob==G.obedit)
			editing= 1;
		else if(ob==OBACT)
			if(FACESEL_PAINT_TEST)
				editing= 1;

		if(!editing)
			draw_game_text_mesh(ob, me);
	}

	draw_textured_end();
	
	/* draw edges and selected faces over textured mesh */
	if(!G.obedit && faceselect)
		draw_tfaces3D(ob, me, dm);

	/* reset from negative scale correction */
	glFrontFace(GL_CCW);
	
	/* in editmode, the blend mode needs to be set incase it was ADD */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void init_realtime_GL(void)
{		
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

