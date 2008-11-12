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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "GL/glew.h"

#include "GPC_PolygonMaterial.h"
#include "MT_Vector3.h"
#include "RAS_IRasterizer.h"
#include "RAS_GLExtensionManager.h"

/* This list includes only data type definitions */
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_property_types.h"
#include "DNA_text_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_mesh.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
/* end of blender include block */

static Image *fCurpage=0;
static int fCurtile=0, fCurmode=0, fCurTileXRep=0, fCurTileYRep=0;
static short fTexWindx, fTexWindy, fTexWinsx, fTexWinsy;
static int fDoMipMap = 1;
static int fLinearMipMap=1;
static int fAlphamode= -1;

	/* (n&(n-1)) zeros the least significant bit of n */
static int is_pow2(int num) {
	return ((num)&(num-1))==0;
}
static int smaller_pow2(int num) {
	while (!is_pow2(num))
		num= num&(num-1);
	return num;	
}

static void my_make_repbind(Image *ima)
{
	if(ima==0 || ima->ibufs.first==0) return;

	if(ima->repbind) {
		glDeleteTextures(ima->totbind, (GLuint*)ima->repbind);
		delete (ima->repbind);
		ima->repbind= 0;
	}
	ima->totbind= ima->xrep*ima->yrep;
	if(ima->totbind>1) {
		ima->repbind= (unsigned int *) malloc(sizeof(int)*ima->totbind);
		for (int i=0;i<ima->totbind;i++)
			((int*)ima->repbind)[i] = 0;
	}
}

extern "C" int set_tpage(MTFace *tface);

int set_tpage(MTFace *tface)
{	
	static MTFace *lasttface= 0;
	Image *ima;
	unsigned int *rect, *bind;
	int tpx, tpy, tilemode, tileXRep,tileYRep;

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
		fCurTileXRep=0;
		fCurTileYRep=0;
		fAlphamode= -1;
		
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		return 0;
	}
	lasttface= tface;

	if( fAlphamode != tface->transp) {
		fAlphamode= tface->transp;

		if(fAlphamode) {
			if(fAlphamode==TF_ADD) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glDisable ( GL_ALPHA_TEST );
			/* 	glBlendEquationEXT(GL_FUNC_ADD_EXT); */
			}
			else if(fAlphamode==TF_ALPHA) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable ( GL_ALPHA_TEST );
			/* 	glBlendEquationEXT(GL_FUNC_ADD_EXT); */
			}
			else if (fAlphamode==TF_CLIP){		
				glDisable(GL_BLEND); 
				glEnable ( GL_ALPHA_TEST );
				glAlphaFunc(GL_GREATER, 0.5f);
			}
			/* else { */
			/* 	glBlendFunc(GL_ONE, GL_ONE); */
			/* 	glBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT); */
			/* } */
		}
		else glDisable(GL_BLEND);
	}

	ima= (struct Image *) tface->tpage;

	/* Enable or disable environment mapping */
	if (ima && (ima->flag & IMA_REFLECT)){

		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
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


	if(ima==fCurpage && fCurtile==tface->tile && tilemode==fCurmode && fCurTileXRep==tileXRep && fCurTileYRep == tileYRep) return ima!=0;

	if(tilemode!=fCurmode || fCurTileXRep!=tileXRep || fCurTileYRep != tileYRep)
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
		fCurTileXRep = tileXRep;
		fCurTileYRep = tileYRep;

		return 0;
	}
	
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	
	if(ibuf==0) {
		ima->ok= 0;

		fCurtile= tface->tile;
		fCurpage= 0;
		fCurmode= tilemode;
		fCurTileXRep = tileXRep;
		fCurTileYRep = tileYRep;
		
		glDisable(GL_TEXTURE_2D);
		return 0;
	}

	if(ima->tpageflag & IMA_TWINANIM)	fCurtile= ima->lastframe;
	else fCurtile= tface->tile;

	if(tilemode) {
		
		if(ima->repbind==0) my_make_repbind(ima);
		
		if(fCurtile>=ima->totbind) fCurtile= 0;
		
		/* this happens when you change repeat buttons */
		if(ima->repbind) bind= ima->repbind+fCurtile;
		else bind= &ima->bindcode;
		
		if(*bind==0) {
			
			fTexWindx= ibuf->x/ima->xrep;
			fTexWindy= ibuf->y/ima->yrep;
			
			if(fCurtile>=ima->xrep*ima->yrep) fCurtile= ima->xrep*ima->yrep-1;
	
			fTexWinsy= fCurtile / ima->xrep;
			fTexWinsx= fCurtile - fTexWinsy*ima->xrep;
	
			fTexWinsx*= fTexWindx;
			fTexWinsy*= fTexWindy;
	
			tpx= fTexWindx;
			tpy= fTexWindy;

			rect= ibuf->rect + fTexWinsy*ibuf->x + fTexWinsx;
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
				
			tilerect= (unsigned int*)MEM_mallocN(rectw*recth*sizeof(*tilerect), "tilerect");
			for (y=0; y<recth; y++) {
				unsigned int *rectrow= &rect[y*ibuf->x];
				unsigned int *tilerectrow= &tilerect[y*rectw];
					
				memcpy(tilerectrow, rectrow, tpx*sizeof(*rectrow));
			}
				
			rect= tilerect;
		}
#endif
		if (!is_pow2(rectw) || !is_pow2(recth)) {
			rectw= smaller_pow2(rectw);
			recth= smaller_pow2(recth);
			
			scalerect= (unsigned int *)MEM_mallocN(rectw*recth*sizeof(*scalerect), "scalerect");
			gluScaleImage(GL_RGBA, tpx, tpy, GL_UNSIGNED_BYTE, rect, rectw, recth, GL_UNSIGNED_BYTE, scalerect);
			rect= scalerect;
		}

		glGenTextures(1, (GLuint*)bind);

		/*
		if(G.f & G_DEBUG) {
			printf("var1: %s\n", ima->id.name+2);
			printf("var1: %d, var2: %d\n", *bind, tpx);
			printf("var1: %d, var2: %d\n", fCurtile, tilemode);
		}
		*/
		glBindTexture( GL_TEXTURE_2D, *bind);
		
		if (!fDoMipMap)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  rectw, recth, 0, GL_RGBA, GL_UNSIGNED_BYTE, rect);
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
	
	

	glEnable(GL_TEXTURE_2D);

	fCurpage= ima;
	fCurmode= tilemode;
	fCurTileXRep = tileXRep;
	fCurTileYRep = tileYRep;

	return 1;
}

#if 0
GPC_PolygonMaterial::GPC_PolygonMaterial(const STR_String& texname, bool ba, const STR_String& matname,
			int tile, int tileXrep, int tileYrep, int mode, bool transparant, bool zsort,
			int lightlayer, bool bIsTriangle, void* clientobject, void* tpage) :
			RAS_IPolyMaterial(texname, ba, matname, tile, tileXrep, tileYrep, mode,
			transparant, zsort, lightlayer, bIsTriangle, clientobject), m_tface((struct MTFace*)tpage)
{
	// clear local caching info
	my_set_tpage(0);
}


GPC_PolygonMaterial::~GPC_PolygonMaterial(void)
{
}


void GPC_PolygonMaterial::Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const
{
	if (GetCachingInfo() != cachingInfo)
	{
		if (!cachingInfo)
		{
			my_set_tpage(0);
		}
		cachingInfo = GetCachingInfo();

		if ((m_drawingmode & 4)&& (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) )
		{
			update_realtime_texture((struct MTFace*) m_tface, rasty->GetTime());
			my_set_tpage(m_tface);
			rasty->EnableTextures(true);
		} else
		{
			my_set_tpage(0);
			rasty->EnableTextures(false);
		}
	
		//TF_TWOSIDE == 512, todo, make this a ketsji enum
		if(m_drawingmode & 512) {
			rasty->SetCullFace(false);
		}
			
		else
		{
			rasty->SetCullFace(true);//glEnable(GL_CULL_FACE);
			//else glDisable(GL_CULL_FACE);
		}
	}
	rasty->SetSpecularity(m_specular[0],m_specular[1],m_specular[2],m_specularity);
	rasty->SetShinyness(m_shininess);
	rasty->SetDiffuse(m_diffuse[0], m_diffuse[1],m_diffuse[2], 1.0);
}

#endif
void GPC_PolygonMaterial::SetMipMappingEnabled(bool enabled)
{
	fDoMipMap = enabled ? 1 : 0;
}
