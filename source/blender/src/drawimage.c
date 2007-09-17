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
 * Contributor(s): Blender Foundation, 2002-2006
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_image.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_object.h"

#include "BDR_editface.h"
#include "BDR_drawobject.h"
#include "BDR_drawmesh.h"
#include "BDR_imagepaint.h"

#include "BIF_cursors.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_mywindow.h"
#include "BIF_drawimage.h"
#include "BIF_resources.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_editsima.h"
#include "BIF_glutil.h"
#include "BIF_renderwin.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"

#include "BSE_drawipo.h"
#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "RE_pipeline.h"
#include "BMF_Api.h"

/* Modules used */
#include "mydevice.h"
#include "blendef.h"
#include "butspace.h"  // event codes
#include "winlay.h"

#include "interface.h"	/* bad.... but preview code needs UI info. Will solve... (ton) */

static unsigned char *alloc_alpha_clone_image(int *width, int *height)
{
	Brush *brush = G.scene->toolsettings->imapaint.brush;
	ImBuf *ibuf;
	unsigned int size, alpha;
	unsigned char *rect, *cp;

	if(!brush || !brush->clone.image)
		return NULL;
	
	ibuf= BKE_image_get_ibuf(brush->clone.image, NULL);

	if(!ibuf || !ibuf->rect)
		return NULL;

	rect= MEM_dupallocN(ibuf->rect);
	if(!rect)
		return NULL;

	*width= ibuf->x;
	*height= ibuf->y;

	size= (*width)*(*height);
	alpha= (unsigned char)255*brush->clone.alpha;
	cp= rect;

	while(size-- > 0) {
		cp[3]= alpha;
		cp += 4;
	}

	return rect;
}

static int image_preview_active(ScrArea *sa, float *xim, float *yim)
{
	SpaceImage *sima= sa->spacedata.first;
	
	/* only when compositor shows, and image handler set */
	if(sima->image && sima->image->type==IMA_TYPE_COMPOSITE) {
		short a;
	
		for(a=0; a<SPACE_MAXHANDLER; a+=2) {
			if(sima->blockhandler[a] == IMAGE_HANDLER_PREVIEW) {
				if(xim) *xim= (G.scene->r.size*G.scene->r.xsch)/100;
				if(yim) *yim= (G.scene->r.size*G.scene->r.ysch)/100;
				return 1;
			}
		}
	}
	return 0;
}


/**
 * Sets up the fields of the View2D member of the SpaceImage struct
 * This routine can be called in two modes:
 * mode == 'f': float mode (0.0 - 1.0)
 * mode == 'p': pixel mode (0 - size)
 *
 * @param     sima  the image space to update
 * @param     mode  the mode to use for the update
 * @return    void
 *   
 */
void calc_image_view(SpaceImage *sima, char mode)
{
	float xim=256, yim=256;
	float x1, y1;
	float zoom;
	
	if(image_preview_active(curarea, &xim, &yim));
	else if(sima->image) {
		ImBuf *ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
		
		if(ibuf) {
			xim= ibuf->x;
			yim= ibuf->y;
		}
		else if( sima->image->type==IMA_TYPE_R_RESULT ) {
			/* not very important, just nice */
			xim= (G.scene->r.xsch*G.scene->r.size)/100;
			yim= (G.scene->r.ysch*G.scene->r.size)/100;
		}
	}
		
	sima->v2d.tot.xmin= 0;
	sima->v2d.tot.ymin= 0;
	sima->v2d.tot.xmax= xim;
	sima->v2d.tot.ymax= yim;
	
	sima->v2d.mask.xmin= sima->v2d.mask.ymin= 0;
	sima->v2d.mask.xmax= curarea->winx;
	sima->v2d.mask.ymax= curarea->winy;


	/* Which part of the image space do we see? */
	/* Same calculation as in lrectwrite: area left and down*/
	x1= curarea->winrct.xmin+(curarea->winx-sima->zoom*xim)/2;
	y1= curarea->winrct.ymin+(curarea->winy-sima->zoom*yim)/2;

	x1-= sima->zoom*sima->xof;
	y1-= sima->zoom*sima->yof;

	/* float! */
	zoom= sima->zoom;
	
	/* relative display right */
	sima->v2d.cur.xmin= ((curarea->winrct.xmin - (float)x1)/zoom);
	sima->v2d.cur.xmax= sima->v2d.cur.xmin + ((float)curarea->winx/zoom);
	
	/* relative display left */
	sima->v2d.cur.ymin= ((curarea->winrct.ymin-(float)y1)/zoom);
	sima->v2d.cur.ymax= sima->v2d.cur.ymin + ((float)curarea->winy/zoom);
	
	if(mode=='f') {		
		sima->v2d.cur.xmin/= xim;
		sima->v2d.cur.xmax/= xim;
		sima->v2d.cur.ymin/= yim;
		sima->v2d.cur.ymax/= yim;
	}
}

/* check for facelesect, and set active image */
void what_image(SpaceImage *sima)
{
	MTFace *activetf;
		
	if(sima->mode==SI_TEXTURE) {
		
		/* viewer overrides faceselect */
		if(sima->image && sima->image->source==IMA_SRC_VIEWER) {}
		else if (G.obedit == OBACT) {
			sima->image= NULL;
			activetf = get_active_mtface(NULL, NULL, 1); /* partially selected face is ok */
			
			if(activetf && activetf->mode & TF_TEX) {
				sima->image= activetf->tpage;
				
				if(sima->flag & SI_EDITTILE);
				else sima->curtile= activetf->tile;
				
				if(sima->image) {
					if(activetf->mode & TF_TILES)
						sima->image->tpageflag |= IMA_TILES;
					else sima->image->tpageflag &= ~IMA_TILES;
				}
			}
		}
	}
}

/* after a what_image(), this call will give ibufs, includes the spare image */
ImBuf *imagewindow_get_ibuf(SpaceImage *sima)
{
	
	if(G.sima->image) {
		/* check for spare */
		if(sima->image->type==IMA_TYPE_R_RESULT && sima->showspare)
			return sima->spare;
		else
			return BKE_image_get_ibuf(sima->image, &sima->iuser);
	}
	return NULL;
}

extern int EM_texFaceCheck(void); /* from editmesh.c */
/* called to assign images to UV faces */
void image_changed(SpaceImage *sima, int dotile)
{
	MTFace *tface;
	EditMesh *em = G.editMesh;
	EditFace *efa;

	if(sima->image==NULL)
		sima->flag &= ~SI_DRAWTOOL;
	
	if(sima->mode!=SI_TEXTURE || !EM_texFaceCheck())
		return;
		
	/* skip assigning these procedural images... */
	if(sima->image) {
		if(sima->image->type==IMA_TYPE_R_RESULT)
			return;
		if(sima->image->type==IMA_TYPE_COMPOSITE)
			return;
	}

	for (efa= em->faces.first; efa; efa= efa->next) {
		/*if (efa->f & SELECT) {*/
		if (SIMA_FACEDRAW_CHECK(efa)) {
			tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		
			if(dotile==2) {
				tface->mode &= ~TF_TILES;
			} else {
				if (sima->image) {
					tface->tpage= sima->image;
					tface->mode |= TF_TEX;
					
					if(sima->image->tpageflag & IMA_TILES) tface->mode |= TF_TILES;
					else tface->mode &= ~TF_TILES;
					
					if(sima->image->id.us==0) id_us_plus(&sima->image->id);
					else id_lib_extern(&sima->image->id);
				} else {
					tface->tpage= NULL;
					tface->mode &= ~TF_TEX;
				}
			
				if(dotile) tface->tile= sima->curtile;
			}
		}
	}
	object_uvs_changed(OBACT);
	allqueue(REDRAWBUTSEDIT, 0);
}


void uvco_to_areaco(float *vec, short *mval)
{
	float x, y;

	mval[0]= IS_CLIPPED;
	
	x= (vec[0] - G.v2d->cur.xmin)/(G.v2d->cur.xmax-G.v2d->cur.xmin);
	y= (vec[1] - G.v2d->cur.ymin)/(G.v2d->cur.ymax-G.v2d->cur.ymin);

	if(x>=0.0 && x<=1.0) {
		if(y>=0.0 && y<=1.0) {		
			mval[0]= G.v2d->mask.xmin + x*(G.v2d->mask.xmax-G.v2d->mask.xmin);
			mval[1]= G.v2d->mask.ymin + y*(G.v2d->mask.ymax-G.v2d->mask.ymin);
		}
	}
}

void uvco_to_areaco_noclip(float *vec, int *mval)
{
	float x, y;

	mval[0]= IS_CLIPPED;
	
	x= (vec[0] - G.v2d->cur.xmin)/(G.v2d->cur.xmax-G.v2d->cur.xmin);
	y= (vec[1] - G.v2d->cur.ymin)/(G.v2d->cur.ymax-G.v2d->cur.ymin);

	x= G.v2d->mask.xmin + x*(G.v2d->mask.xmax-G.v2d->mask.xmin);
	y= G.v2d->mask.ymin + y*(G.v2d->mask.ymax-G.v2d->mask.ymin);
	
	mval[0]= x;
	mval[1]= y;
}

static void drawcursor_sima(void)
{
	int wi, hi;
	float w, h;
	
	transform_width_height_tface_uv(&wi, &hi);
	w = (((float)wi)/256.0f)*G.sima->zoom;
	h = (((float)hi)/256.0f)*G.sima->zoom;
	
	cpack(0xFFFFFF);
	glTranslatef(G.v2d->cursor[0], G.v2d->cursor[1], 0.0f);  
	fdrawline(-0.05/w, 0, 0, 0.05/h);
	fdrawline(0, 0.05/h, 0.05/w, 0);
	fdrawline(0.05/w, 0, 0, -0.05/h);
	fdrawline(0, -0.05/h, -0.05/w, 0);
	
	setlinestyle(4);
	cpack(0xFF);
	fdrawline(-0.05/w, 0, 0, 0.05/h);
	fdrawline(0, 0.05/h, 0.05/w, 0);
	fdrawline(0.05/w, 0, 0, -0.05/h);
	fdrawline(0, -0.05/h, -0.05/w, 0);
	
	
	setlinestyle(0);
	cpack(0x0);
	fdrawline(-0.020/w, 0, -0.1/w, 0);
	fdrawline(0.1/w, 0, .020/w, 0);
	fdrawline(0, -0.020/h, 0, -0.1/h);
	fdrawline(0, 0.1/h, 0, 0.020/h);
	
	setlinestyle(1);
	cpack(0xFFFFFF);
	fdrawline(-0.020/w, 0, -0.1/w, 0);
	fdrawline(0.1/w, 0, .020/w, 0);
	fdrawline(0, -0.020/h, 0, -0.1/h);
	fdrawline(0, 0.1/h, 0, 0.020/h);
	
	glTranslatef(-G.v2d->cursor[0], -G.v2d->cursor[1], 0.0f);
	setlinestyle(0);
}

void draw_tfaces(void)
{
	MTFace *tface,*activetface = NULL;
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	char col1[4], col2[4];
	float pointsize= BIF_GetThemeValuef(TH_VERTEX_SIZE);
 	
	if (!G.obedit || !CustomData_has_layer(&em->fdata, CD_MTFACE))
		return;
	
	
	calc_image_view(G.sima, 'f');	/* float */
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
	glLoadIdentity();
	
	/* draw shadow mesh */
	if ((G.sima->flag & SI_DRAWSHADOW) && (G.obedit==OBACT)) { /* TODO - editmesh */
		DerivedMesh *dm;

		/* draw final mesh with modifiers applied */
		/* should test - editmesh_get_derived_cage_and_final */
		dm = editmesh_get_derived_base();

		glColor3ub(112, 112, 112);
		if (dm->drawUVEdges) dm->drawUVEdges(dm);

		dm->release(dm);
	}
	else if((G.sima->flag & SI_DRAWTOOL) || (G.obedit==OBACT)) {
		/* draw mesh without modifiers applied */

		if (G.obedit) {		
			glColor3ub(112, 112, 112);
			for (efa= em->faces.first; efa; efa= efa->next) {
				/*if(!(mface->flag & ME_HIDE) && (mface->flag & ME_FACE_SEL)) {*/
				/*if(!(efa->flag & ME_HIDE) && (efa->f & SELECT)) {*/
				if (SIMA_FACEDRAW_CHECK(efa)) {
					tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					glBegin(GL_LINE_LOOP);
					glVertex2fv(tface->uv[0]);
					glVertex2fv(tface->uv[1]);
					glVertex2fv(tface->uv[2]);
					if(efa->v4) glVertex2fv(tface->uv[3]);
					glEnd();
				}					
			}
		}
	}

	if((G.sima->flag & SI_DRAWTOOL) || !(G.obedit==OBACT))
		return; /* only draw shadow mesh */
	
	/* draw transparent faces */
	if(G.f & G_DRAWFACES) {
		BIF_GetThemeColor4ubv(TH_FACE, col1);
		BIF_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
		for (efa= em->faces.first; efa; efa= efa->next) {
			/*if(efa->f & SELECT) {*/
			if (SIMA_FACEDRAW_CHECK(efa)) {
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if( SIMA_FACESEL_CHECK(efa, tface) )
					glColor4ubv((GLubyte *)col2);
				else
					glColor4ubv((GLubyte *)col1);
					
				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
					glVertex2fv(tface->uv[0]);
					glVertex2fv(tface->uv[1]);
					glVertex2fv(tface->uv[2]);
					if(efa->v4) glVertex2fv(tface->uv[3]);
				glEnd();
			}
		}
		glDisable(GL_BLEND);
	}
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		/*if (efa->f & SELECT) {*/
		if (SIMA_FACEDRAW_CHECK(efa)) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			
			cpack(0x0);
			glBegin(GL_LINE_LOOP);
				glVertex2fv(tface->uv[0]);
				glVertex2fv(tface->uv[1]);
				glVertex2fv(tface->uv[2]);
				if(efa->v4) glVertex2fv(tface->uv[3]);
			glEnd();
		
			setlinestyle(2);
			cpack(0xFFFFFF);
			glBegin(GL_LINE_STRIP);
				glVertex2fv(tface->uv[0]);
				glVertex2fv(tface->uv[1]);
			glEnd();

			glBegin(GL_LINE_STRIP);
				glVertex2fv(tface->uv[0]);
				if(efa->v4) glVertex2fv(tface->uv[3]);
				else glVertex2fv(tface->uv[2]);
			glEnd();

			glBegin(GL_LINE_STRIP);
				glVertex2fv(tface->uv[1]);
				glVertex2fv(tface->uv[2]);
				if(efa->v4) glVertex2fv(tface->uv[3]);
			glEnd();
			setlinestyle(0);
		}
	}

	/* draw active face edges */
	/*if (activetface){*/
		/* colors: R=u G=v */
	activetface = get_active_mtface(&efa, NULL, 0);
	if (activetface) {
		setlinestyle(2);
		tface=activetface; 
		/*mface=activemface;*/ 

		cpack(0x0);
		glBegin(GL_LINE_LOOP);
		glVertex2fv(tface->uv[0]);
			glVertex2fv(tface->uv[1]);
			glVertex2fv(tface->uv[2]);
			if(efa->v4) glVertex2fv(tface->uv[3]);
		glEnd();
			
		cpack(0xFF00);
		glBegin(GL_LINE_STRIP);
			glVertex2fv(tface->uv[0]);
			glVertex2fv(tface->uv[1]);
		glEnd();

		cpack(0xFF);
		glBegin(GL_LINE_STRIP);
			glVertex2fv(tface->uv[0]);
			if(efa->v4) glVertex2fv(tface->uv[3]);
			else glVertex2fv(tface->uv[2]);
		glEnd();

		cpack(0xFFFFFF);
		glBegin(GL_LINE_STRIP);
			glVertex2fv(tface->uv[1]);
			glVertex2fv(tface->uv[2]);
			if(efa->v4) glVertex2fv(tface->uv[3]);
		glEnd();
		
		setlinestyle(0);
	}

    /* unselected uv's */
	BIF_ThemeColor(TH_VERTEX);
	glPointSize(pointsize);

	bglBegin(GL_POINTS);
	for (efa= em->faces.first; efa; efa= efa->next) {
		/*if (efa->f & SELECT) {*/
		if (SIMA_FACEDRAW_CHECK(efa)) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(SIMA_UVSEL_CHECK(efa, tface, 0)); else bglVertex2fv(tface->uv[0]);
			if(SIMA_UVSEL_CHECK(efa, tface, 1)); else bglVertex2fv(tface->uv[1]);
			if(SIMA_UVSEL_CHECK(efa, tface, 2)); else bglVertex2fv(tface->uv[2]);
			if(efa->v4) {
				if(SIMA_UVSEL_CHECK(efa, tface, 3)); else bglVertex2fv(tface->uv[3]);
			}
		}
	}
	bglEnd();

	/* pinned uv's */
	/* give odd pointsizes odd pin pointsizes */
    glPointSize(pointsize*2 + (((int)pointsize % 2)? (-1): 0));
	cpack(0xFF);

	bglBegin(GL_POINTS);
	for (efa= em->faces.first; efa; efa= efa->next) {
		/*if (efa->f & SELECT) {*/
		if (SIMA_FACEDRAW_CHECK(efa)) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(tface->unwrap & TF_PIN1) bglVertex2fv(tface->uv[0]);
			if(tface->unwrap & TF_PIN2) bglVertex2fv(tface->uv[1]);
			if(tface->unwrap & TF_PIN3) bglVertex2fv(tface->uv[2]);
			if(efa->v4) {
				if(tface->unwrap & TF_PIN4) bglVertex2fv(tface->uv[3]);
			}
		}
	}
	bglEnd();

	/* selected uv's */
	BIF_ThemeColor(TH_VERTEX_SELECT);
    glPointSize(pointsize);

	bglBegin(GL_POINTS);
	for (efa= em->faces.first; efa; efa= efa->next) {
		/*if (efa->f & SELECT) {*/
		if (SIMA_FACEDRAW_CHECK(efa)) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(!SIMA_UVSEL_CHECK(efa, tface, 0)); else bglVertex2fv(tface->uv[0]);
			if(!SIMA_UVSEL_CHECK(efa, tface, 1)); else bglVertex2fv(tface->uv[1]);
			if(!SIMA_UVSEL_CHECK(efa, tface, 2)); else bglVertex2fv(tface->uv[2]);
			if(efa->v4) {
				if(!SIMA_UVSEL_CHECK(efa, tface, 3)); else bglVertex2fv(tface->uv[3]);
			}
		}
	}
	bglEnd();

	
	/* Draw the cursor here, this should be in its own function really but it relys on the previous calls to set the view matrix */
	drawcursor_sima();
	
	glPointSize(1.0);
}

static unsigned int *get_part_from_ibuf(ImBuf *ibuf, short startx, short starty, short endx, short endy)
{
	unsigned int *rt, *rp, *rectmain;
	short y, heigth, len;

	/* the right offset in rectot */

	rt= ibuf->rect+ (starty*ibuf->x+ startx);

	len= (endx-startx);
	heigth= (endy-starty);

	rp=rectmain= MEM_mallocN(heigth*len*sizeof(int), "rect");
	
	for(y=0; y<heigth; y++) {
		memcpy(rp, rt, len*4);
		rt+= ibuf->x;
		rp+= len;
	}
	return rectmain;
}

static void draw_image_transform(ImBuf *ibuf)
{
	if(G.moving) {
		float aspx, aspy, center[3];

        BIF_drawConstraint();

		if(ibuf==0 || ibuf->rect==0 || ibuf->x==0 || ibuf->y==0) {
			aspx= aspy= 1.0;
		}
		else {
			aspx= 256.0/ibuf->x;
			aspy= 256.0/ibuf->y;
		}

		BIF_getPropCenter(center);

		/* scale and translate the circle into place and draw it */
		glPushMatrix();
		glScalef(aspx, aspy, 1.0);
		glTranslatef((1/aspx)*center[0] - center[0],
		             (1/aspy)*center[1] - center[1], 0.0);

		BIF_drawPropCircle();

		glPopMatrix();
	}
}

static void draw_image_view_icon(void)
{
	float xPos = 5.0;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	
	
	if (G.sima->flag & SI_SYNC_UVSEL) {
		/* take settings from the editmesh */
		if (G.scene->selectmode == SCE_SELECT_FACE) {
			BIF_icon_draw_aspect(xPos, 5.0, ICON_STICKY_UVS, 1.0f);
		} else {
			BIF_icon_draw_aspect(xPos, 5.0, ICON_STICKY2_UVS, 1.0f);
		}
		xPos = 25.0;
		
		if (G.scene->selectmode == SCE_SELECT_FACE || G.sima->flag & SI_SELACTFACE) {
			BIF_icon_draw_aspect(xPos, 5.0, ICON_DRAW_UVFACES, 1.0f);
		}
		
	} else {
		/* use the flags for UV mode - normal operation */	
		if(G.sima->flag & SI_STICKYUVS) {
			BIF_icon_draw_aspect(xPos, 5.0, ICON_STICKY2_UVS, 1.0f);
			xPos = 25.0;
		}
		else if(!(G.sima->flag & SI_LOCALSTICKY)) {
			BIF_icon_draw_aspect(xPos, 5.0, ICON_STICKY_UVS, 1.0f);
			xPos = 25.0;
		}
	
		if(G.sima->flag & SI_SELACTFACE) {
			BIF_icon_draw_aspect(xPos, 5.0, ICON_DRAW_UVFACES, 1.0f);
		}
	}
	
	glBlendFunc(GL_ONE,  GL_ZERO); 
	glDisable(GL_BLEND);
}

static void draw_image_view_tool(void)
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;
	short mval[2];
	float radius;
	int draw= 0;

	if(brush) {
		if(settings->imapaint.flag & IMAGEPAINT_DRAWING) {
			if(settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL_DRAWING)
				draw= 1;
		}
		else if(settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL)
			draw= 1;
		
		if(draw) {
			getmouseco_areawin(mval);

			radius= brush->size*G.sima->zoom/2;
			fdrawXORcirc(mval[0], mval[1], radius);

			if (brush->innerradius != 1.0) {
				radius *= brush->innerradius;
				fdrawXORcirc(mval[0], mval[1], radius);
			}
		}
	}
}

/* ************ panel stuff ************* */

/* this function gets the values for cursor and vertex number buttons */
static void image_transform_but_attr(int *imx, int *imy, int *step, int *digits) /*, float *xcoord, float *ycoord)*/
{
	ImBuf *ibuf= imagewindow_get_ibuf(G.sima);
	if(ibuf) {
		*imx= ibuf->x;
		*imy= ibuf->y;
	}
	
	if (G.sima->flag & SI_COORDFLOATS) {
		*step= 1;
		*digits= 3;
	}
	else {
		*step= 100;
		*digits= 2;
	}
}


/* is used for both read and write... */
void image_editvertex_buts(uiBlock *block)
{
	static float ocent[2];
	float cent[2]= {0.0, 0.0};
	int imx= 256, imy= 256;
	int nactive= 0, step, digits;
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tf;
	
	if( is_uv_tface_editing_allowed_silent()==0 ) return;
	
	image_transform_but_attr(&imx, &imy, &step, &digits);
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		/*if ((efa->f & SELECT)) { */
		if (SIMA_FACEDRAW_CHECK(efa)) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			
			if (SIMA_UVSEL_CHECK(efa, tf, 0)) {
				cent[0]+= tf->uv[0][0];
				cent[1]+= tf->uv[0][1];
				nactive++;
			}
			if (SIMA_UVSEL_CHECK(efa, tf, 1)) {
				cent[0]+= tf->uv[1][0];
				cent[1]+= tf->uv[1][1];
				nactive++;
			}
			if (SIMA_UVSEL_CHECK(efa, tf, 2)) {
				cent[0]+= tf->uv[2][0];
				cent[1]+= tf->uv[2][1];
				nactive++;
			}
			if (efa->v4 && SIMA_UVSEL_CHECK(efa, tf, 3)) {
				cent[0]+= tf->uv[3][0];
				cent[1]+= tf->uv[3][1];
				nactive++;
			}
		}
	}
		
	if(block) {	// do the buttons
		if (nactive) {
			ocent[0]= cent[0]/nactive;
			ocent[1]= cent[1]/nactive;
			if (G.sima->flag & SI_COORDFLOATS) {
			} else {
				ocent[0] *= imx;
				ocent[1] *= imy;
			}
			
			uiBlockBeginAlign(block);
			if(nactive==1) {
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Vertex X:",	10, 40, 145, 19, &ocent[0], -10*imx, 10.0*imx, step, digits, "");
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Vertex Y:",	10, 20, 145, 19, &ocent[1], -10*imy, 10.0*imy, step, digits, "");
			}
			else {
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Median X:",	10, 40, 145, 19, &ocent[0], -10*imx, 10.0*imx, step, digits, "");
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Median Y:",	10, 20, 145, 19, &ocent[1], -10*imy, 10.0*imy, step, digits, "");
			}
			uiBlockEndAlign(block);
		}
	}
	else {	// apply event
		float delta[2];
		
		cent[0]= cent[0]/nactive;
		cent[1]= cent[1]/nactive;
			
		if (G.sima->flag & SI_COORDFLOATS) {
			delta[0]= ocent[0]-cent[0];
			delta[1]= ocent[1]-cent[1];
		}
		else {
			delta[0]= ocent[0]/imx - cent[0];
			delta[1]= ocent[1]/imy - cent[1];
		}

		for (efa= em->faces.first; efa; efa= efa->next) {
			/*if (!(efa->f & SELECT)) continue;*/
			if (SIMA_FACEDRAW_CHECK(efa)) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				
				if (SIMA_UVSEL_CHECK(efa, tf, 0)) {
					tf->uv[0][0]+= delta[0];
					tf->uv[0][1]+= delta[1];
				}
				if (SIMA_UVSEL_CHECK(efa, tf, 1)) {
					tf->uv[1][0]+= delta[0];
					tf->uv[1][1]+= delta[1];
				}
				if (SIMA_UVSEL_CHECK(efa, tf, 2)) {
					tf->uv[2][0]+= delta[0];
					tf->uv[2][1]+= delta[1];
				}
				if (efa->v4 && SIMA_UVSEL_CHECK(efa, tf, 3)) {
					tf->uv[3][0]+= delta[0];
					tf->uv[3][1]+= delta[1];
				}
			}
		}
			
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
	}
}


/* is used for both read and write... */
void image_editcursor_buts(uiBlock *block)
{
	static float ocent[2];
	/*float cent[2]= {0.0, 0.0};*/
	int imx= 256, imy= 256;
	int step, digits;
	
	if( is_uv_tface_editing_allowed_silent()==0 ) return;
	
	image_transform_but_attr(&imx, &imy, &step, &digits);
		
	if(block) {	// do the buttons
		ocent[0]= G.v2d->cursor[0];
		ocent[1]= G.v2d->cursor[1];
		if (G.sima->flag & SI_COORDFLOATS) {
		} else {
			ocent[0] *= imx;
			ocent[1] *= imy;
		}
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_CURSOR_IMAGE, "Cursor X:",	165, 40, 145, 19, &ocent[0], -10*imx, 10.0*imx, step, digits, "");
		uiDefButF(block, NUM, B_CURSOR_IMAGE, "Cursor Y:",	165, 20, 145, 19, &ocent[1], -10*imy, 10.0*imy, step, digits, "");
		uiBlockEndAlign(block);
	
	}
	else {	// apply event
		if (G.sima->flag & SI_COORDFLOATS) {
			G.v2d->cursor[0]= ocent[0];
			G.v2d->cursor[1]= ocent[1];
		}
		else {
			G.v2d->cursor[0]= ocent[0]/imx;
			G.v2d->cursor[1]= ocent[1]/imy;
		}
		allqueue(REDRAWIMAGE, 0);
	}
}


void image_info(Image *ima, ImBuf *ibuf, char *str)
{
	int ofs= 0;
	
	str[0]= 0;
	
	if(ima==NULL) return;
	if(ibuf==NULL) {
		sprintf(str, "Can not get an image");
		return;
	}
	
	if(ima->source==IMA_SRC_MOVIE) {
		ofs= sprintf(str, "Movie ");
		if(ima->anim) 
			ofs+= sprintf(str+ofs, "%d frs", IMB_anim_get_duration(ima->anim));
	}
	else
	 	ofs= sprintf(str, "Image ");

	ofs+= sprintf(str+ofs, ": size %d x %d,", ibuf->x, ibuf->y);
	
	if(ibuf->rect_float) {
		if(ibuf->channels!=4) {
			sprintf(str+ofs, "%d float channel(s)", ibuf->channels);
		}
		else if(ibuf->depth==32)
			strcat(str, " RGBA float");
		else
			strcat(str, " RGB float");
	}
	else {
		if(ibuf->depth==32)
			strcat(str, " RGBA byte");
		else
			strcat(str, " RGB byte");
	}
	if(ibuf->zbuf || ibuf->zbuf_float)
		strcat(str, " + Z");
	
}

static void image_panel_properties(short cntrl)	// IMAGE_HANDLER_PROPERTIES
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "image_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Image Properties", "Image", 10, 10, 318, 204)==0)
		return;
	
	/* note, it draws no bottom half in facemode, for vertex buttons */
	uiblock_image_panel(block, &G.sima->image, &G.sima->iuser, B_REDR, B_REDR);
}	

static void image_panel_game_properties(short cntrl)	// IMAGE_HANDLER_GAME_PROPERTIES
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "image_panel_game_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_GAME_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Real-time Properties", "Image", 10, 10, 318, 204)==0)
		return;

	if (ibuf) {
		char str[128];
		
		image_info(G.sima->image, ibuf, str);
		uiDefBut(block, LABEL, B_NOP, str,		10,180,300,19, 0, 0, 0, 0, 0, "");

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, IMA_TWINANIM, B_TWINANIM, "Anim", 10,150,140,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Toggles use of animated texture");
		uiDefButS(block, NUM, B_TWINANIM, "Start:",		10,130,140,19, &G.sima->image->twsta, 0.0, 128.0, 0, 0, "Displays the start frame of an animated texture");
		uiDefButS(block, NUM, B_TWINANIM, "End:",		10,110,140,19, &G.sima->image->twend, 0.0, 128.0, 0, 0, "Displays the end frame of an animated texture");
		uiDefButS(block, NUM, B_NOP, "Speed", 				10,90,140,19, &G.sima->image->animspeed, 1.0, 100.0, 0, 0, "Displays Speed of the animation in frames per second");
		uiBlockEndAlign(block);

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, IMA_TILES, B_SIMAGEDRAW1, "Tiles",	160,150,140,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Toggles use of tilemode for faces (Shift LMB to pick the tile for selected faces)");
		uiDefButS(block, NUM, B_SIMAGEDRAW, "X:",		160,130,70,19, &G.sima->image->xrep, 1.0, 16.0, 0, 0, "Sets the degree of repetition in the X direction");
		uiDefButS(block, NUM, B_SIMAGEDRAW, "Y:",		230,130,70,19, &G.sima->image->yrep, 1.0, 16.0, 0, 0, "Sets the degree of repetition in the Y direction");
		uiBlockBeginAlign(block);

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, IMA_CLAMP_U, B_SIMAGEDRAW, "ClampX",	160,100,70,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Disable texture repeating horizontaly");
		uiDefButBitS(block, TOG, IMA_CLAMP_V, B_SIMAGEDRAW, "ClampY",	230,100,70,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Disable texture repeating vertically");
		uiBlockEndAlign(block);		
	}
}

static void image_panel_transform_properties(short cntrl)	// IMAGE_HANDLER_TRANSFORM_PROPERTIES
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "image_transform_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_TRANSFORM_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Transform Properties", "Image", 10, 10, 318, 204)==0)
		return;
	
	uiDefButBitI(block, TOG, SI_COORDFLOATS, B_SIMAGEDRAW1, "Normalized Coords",	10,80,140,19, &G.sima->flag, 0, 0, 0, 0, "Display coords from 0.0 to 1.0 rather then in pixels");
	
	image_editvertex_buts(block);
	image_editcursor_buts(block);
}

static void image_panel_paint(short cntrl)	// IMAGE_HANDLER_PAINT
{
	/* B_SIMABRUSHCHANGE only redraws and eats the mouse messages  */
	/* so that LEFTMOUSE does not 'punch' through the floating panel */
	/* B_SIMANOTHING */
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;
	uiBlock *block;
	ID *id;
	int yco, xco, butw;

	block= uiNewBlock(&curarea->uiblocks, "image_panel_paint", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PAINT);  // for close and esc
	if(uiNewPanel(curarea, block, "Image Paint", "Image", 10, 230, 318, 204)==0)
		return;

	yco= 160;

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_SIMABRUSHCHANGE, "Draw",		0  ,yco,80,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_DRAW, 0, 0, "Draw brush");
	uiDefButS(block, ROW, B_SIMABRUSHCHANGE, "Soften",		80 ,yco,80,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_SOFTEN, 0, 0, "Soften brush");
	uiDefButS(block, ROW, B_SIMABRUSHCHANGE, "Smear",		160,yco,80,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_SMEAR, 0, 0, "Smear brush");	
	uiDefButS(block, ROW, B_SIMABRUSHCHANGE, "Clone",		240,yco,80,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_CLONE, 0, 0, "Clone brush, use RMB to drag source image");	
	uiBlockEndAlign(block);
	yco -= 30;

	uiBlockSetCol(block, TH_BUT_SETTING2);
	id= (ID*)settings->imapaint.brush;
	xco= std_libbuttons(block, 0, yco, 0, NULL, B_SIMABRUSHBROWSE, ID_BR, 0, id, NULL, &(G.sima->menunr), 0, B_SIMABRUSHLOCAL, B_SIMABRUSHDELETE, 0, B_KEEPDATA);
	uiBlockSetCol(block, TH_AUTO);

	if(brush && !brush->id.lib) {
		butw= 320-(xco+10);

		uiDefButS(block, MENU, B_SIMANOTHING, "Mix %x0|Add %x1|Subtract %x2|Multiply %x3|Lighten %x4|Darken %x5|Erase Alpha %x6|Add Alpha %x7", xco+10,yco,butw,19, &brush->blend, 0, 0, 0, 0, "Blending method for applying brushes");

		uiDefButBitS(block, TOG|BIT, BRUSH_TORUS, B_SIMABRUSHCHANGE, "Wrap",	xco+10,yco-25,butw,19, &brush->flag, 0, 0, 0, 0, "Enables torus wrapping");

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG|BIT, BRUSH_AIRBRUSH, B_SIMABRUSHCHANGE, "Airbrush",	xco+10,yco-50,butw,19, &brush->flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse (spray)");
		uiDefButF(block, NUM, B_SIMANOTHING, "Rate ", xco+10,yco-70,butw,19, &brush->rate, 0.01, 1.0, 0, 0, "Number of paints per second for Airbrush");
		uiBlockEndAlign(block);

		yco -= 25;

		uiBlockBeginAlign(block);
		uiDefButF(block, COL, B_VPCOLSLI, "",					0,yco,200,19, brush->rgb, 0, 0, 0, 0, "");
		uiDefButF(block, NUMSLI, B_SIMANOTHING, "Opacity ",		0,yco-20,180,19, &brush->alpha, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
		uiDefButBitS(block, TOG|BIT, BRUSH_ALPHA_PRESSURE, B_SIMANOTHING, "P",	180,yco-20,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiDefButI(block, NUMSLI, B_SIMANOTHING, "Size ",		0,yco-40,180,19, &brush->size, 1, 200, 0, 0, "The size of the brush");
		uiDefButBitS(block, TOG|BIT, BRUSH_SIZE_PRESSURE, B_SIMANOTHING, "P",	180,yco-40,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiDefButF(block, NUMSLI, B_SIMANOTHING, "Falloff ",		0,yco-60,180,19, &brush->innerradius, 0.0, 1.0, 0, 0, "The fall off radius of the brush");
		uiDefButBitS(block, TOG|BIT, BRUSH_RAD_PRESSURE, B_SIMANOTHING, "P",	180,yco-60,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiDefButF(block, NUMSLI, B_SIMANOTHING, "Spacing ",0,yco-80,180,19, &brush->spacing, 1.0, 100.0, 0, 0, "Repeating paint on %% of brush diameter");
		uiDefButBitS(block, TOG|BIT, BRUSH_SPACING_PRESSURE, B_SIMANOTHING, "P",	180,yco-80,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiBlockEndAlign(block);

		yco -= 110;

		if(settings->imapaint.tool == PAINT_TOOL_CLONE) {
			id= (ID*)brush->clone.image;
			uiBlockSetCol(block, TH_BUT_SETTING2);
			xco= std_libbuttons(block, 0, yco, 0, NULL, B_SIMACLONEBROWSE, ID_IM, 0, id, 0, &G.sima->menunr, 0, 0, B_SIMACLONEDELETE, 0, 0);
			uiBlockSetCol(block, TH_AUTO);
			if(id) {
				butw= 320-(xco+5);
				uiDefButF(block, NUMSLI, B_SIMABRUSHCHANGE, "B ",xco+5,yco,butw,19, &brush->clone.alpha , 0.0, 1.0, 0, 0, "Opacity of clone image display");
			}
		}
		else {
			MTex *mtex= brush->mtex[brush->texact];

			uiBlockSetCol(block, TH_BUT_SETTING2);
			id= (mtex)? (ID*)mtex->tex: NULL;
			xco= std_libbuttons(block, 0, yco, 0, NULL, B_SIMABTEXBROWSE, ID_TE, 0, id, NULL, &(G.sima->menunr), 0, 0, B_SIMABTEXDELETE, 0, 0);
			/*uiDefButBitS(block, TOG|BIT, BRUSH_FIXED_TEX, B_SIMABRUSHCHANGE, "Fixed",	xco+5,yco,butw,19, &brush->flag, 0, 0, 0, 0, "Keep texture origin in fixed position");*/
			uiBlockSetCol(block, TH_AUTO);
		}
	}

#if 0
		uiDefButBitS(block, TOG|BIT, IMAGEPAINT_DRAW_TOOL_DRAWING, B_SIMABRUSHCHANGE, "TD", 0,1,50,19, &settings->imapaint.flag.flag, 0, 0, 0, 0, "Enables brush shape while drawing");
		uiDefButBitS(block, TOG|BIT, IMAGEPAINT_DRAW_TOOL, B_SIMABRUSHCHANGE, "TP", 50,1,50,19, &settings->imapaint.flag.flag, 0, 0, 0, 0, "Enables brush shape while not drawing");
#endif
}

static void image_panel_curves_reset(void *cumap_v, void *ibuf_v)
{
	CurveMapping *cumap = cumap_v;
	int a;
	
	for(a=0; a<CM_TOT; a++)
		curvemap_reset(cumap->cm+a, &cumap->clipr);
	
	cumap->black[0]=cumap->black[1]=cumap->black[2]= 0.0f;
	cumap->white[0]=cumap->white[1]=cumap->white[2]= 1.0f;
	curvemapping_set_black_white(cumap, NULL, NULL);
	
	curvemapping_changed(cumap, 0);
	curvemapping_do_ibuf(cumap, ibuf_v);
	
	allqueue(REDRAWIMAGE, 0);
}


static void image_panel_curves(short cntrl)	// IMAGE_HANDLER_CURVES
{
	ImBuf *ibuf;
	uiBlock *block;
	uiBut *bt;
	
	/* and we check for spare */
	ibuf= imagewindow_get_ibuf(G.sima);
	
	block= uiNewBlock(&curarea->uiblocks, "image_panel_curves", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_CURVES);  // for close and esc
	if(uiNewPanel(curarea, block, "Curves", "Image", 10, 450, 318, 204)==0)
		return;
	
	if (ibuf) {
		rctf rect;
		
		if(G.sima->cumap==NULL)
			G.sima->cumap= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
		
		rect.xmin= 110; rect.xmax= 310;
		rect.ymin= 10; rect.ymax= 200;
		curvemap_buttons(block, G.sima->cumap, 'c', B_SIMACURVES, B_SIMAGEDRAW, &rect);
		
		bt=uiDefBut(block, BUT, B_SIMARANGE, "Reset",	10, 160, 90, 19, NULL, 0.0f, 0.0f, 0, 0, "Reset Black/White point and curves");
		uiButSetFunc(bt, image_panel_curves_reset, G.sima->cumap, ibuf);
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_SIMARANGE, "Min R:",	10, 120, 90, 19, G.sima->cumap->black, -1000.0f, 1000.0f, 10, 2, "Black level");
		uiDefButF(block, NUM, B_SIMARANGE, "Min G:",	10, 100, 90, 19, G.sima->cumap->black+1, -1000.0f, 1000.0f, 10, 2, "Black level");
		uiDefButF(block, NUM, B_SIMARANGE, "Min B:",	10, 80, 90, 19, G.sima->cumap->black+2, -1000.0f, 1000.0f, 10, 2, "Black level");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_SIMARANGE, "Max R:",	10, 50, 90, 19, G.sima->cumap->white, -1000.0f, 1000.0f, 10, 2, "White level");
		uiDefButF(block, NUM, B_SIMARANGE, "Max G:",	10, 30, 90, 19, G.sima->cumap->white+1, -1000.0f, 1000.0f, 10, 2, "White level");
		uiDefButF(block, NUM, B_SIMARANGE, "Max B:",	10, 10, 90, 19, G.sima->cumap->white+2, -1000.0f, 1000.0f, 10, 2, "White level");
		
	}
}

/* are there curves? curves visible? and curves do something? */
static int image_curves_active(ScrArea *sa)
{
	SpaceImage *sima= sa->spacedata.first;

	if(sima->cumap) {
		if(curvemapping_RGBA_does_something(sima->cumap)) {
			short a;
			for(a=0; a<SPACE_MAXHANDLER; a+=2) {
				if(sima->blockhandler[a] == IMAGE_HANDLER_CURVES)
					return 1;
			}
		}
	}
	return 0;
}

/* 0: disable preview 
   otherwise refresh preview
*/
void image_preview_event(int event)
{
	int exec= 0;
	

	if(event==0) {
		G.scene->r.scemode &= ~R_COMP_CROP;
		exec= 1;
	}
	else {
		if(image_preview_active(curarea, NULL, NULL)) {
			G.scene->r.scemode |= R_COMP_CROP;
			exec= 1;
		}
		else
			G.scene->r.scemode &= ~R_COMP_CROP;
	}
	
	if(exec && G.scene->nodetree) {
		/* should work when no node editor in screen..., so we execute right away */
		
		ntreeCompositTagGenerators(G.scene->nodetree);

		G.afbreek= 0;
		G.scene->nodetree->timecursor= set_timecursor;
		G.scene->nodetree->test_break= blender_test_break;
		
		BIF_store_spare();
		
		ntreeCompositExecTree(G.scene->nodetree, &G.scene->r, 1);	/* 1 is do_previews */
		
		G.scene->nodetree->timecursor= NULL;
		G.scene->nodetree->test_break= NULL;
		
		scrarea_do_windraw(curarea);
		waitcursor(0);
		
		allqueue(REDRAWNODE, 1);
	}	
}


/* nothing drawn here, we use it to store values */
static void preview_cb(struct ScrArea *sa, struct uiBlock *block)
{
	rctf dispf;
	rcti *disprect= &G.scene->r.disprect;
	int winx= (G.scene->r.size*G.scene->r.xsch)/100;
	int winy= (G.scene->r.size*G.scene->r.ysch)/100;
	short mval[2];
	
	if(G.scene->r.mode & R_BORDER) {
		winx*= (G.scene->r.border.xmax - G.scene->r.border.xmin);
		winy*= (G.scene->r.border.ymax - G.scene->r.border.ymin);
	}
	
	/* while dragging we need to update the rects, otherwise it doesn't end with correct one */

	BLI_init_rctf(&dispf, 15.0f, (block->maxx - block->minx)-15.0f, 15.0f, (block->maxy - block->miny)-15.0f);
	ui_graphics_to_window_rct(sa->win, &dispf, disprect);
	
	/* correction for gla draw */
	BLI_translate_rcti(disprect, -curarea->winrct.xmin, -curarea->winrct.ymin);
	
	calc_image_view(G.sima, 'p');
//	printf("winrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);
	/* map to image space coordinates */
	mval[0]= disprect->xmin; mval[1]= disprect->ymin;
	areamouseco_to_ipoco(G.v2d, mval, &dispf.xmin, &dispf.ymin);
	mval[0]= disprect->xmax; mval[1]= disprect->ymax;
	areamouseco_to_ipoco(G.v2d, mval, &dispf.xmax, &dispf.ymax);
	
	/* map to render coordinates */
	disprect->xmin= dispf.xmin;
	disprect->xmax= dispf.xmax;
	disprect->ymin= dispf.ymin;
	disprect->ymax= dispf.ymax;
	
	CLAMP(disprect->xmin, 0, winx);
	CLAMP(disprect->xmax, 0, winx);
	CLAMP(disprect->ymin, 0, winy);
	CLAMP(disprect->ymax, 0, winy);
//	printf("drawrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);

}

static int is_preview_allowed(ScrArea *cur)
{
	SpaceImage *sima= cur->spacedata.first;
	ScrArea *sa;

	/* check if another areawindow has preview set */
	for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa!=cur && sa->spacetype==SPACE_IMAGE) {
			if(image_preview_active(sa, NULL, NULL))
			   return 0;
		}
	}
	/* check image type */
	if(sima->image==NULL || sima->image->type!=IMA_TYPE_COMPOSITE)
		return 0;
	
	return 1;
}

static void image_panel_preview(ScrArea *sa, short cntrl)	// IMAGE_HANDLER_PREVIEW
{
	uiBlock *block;
	SpaceImage *sima= sa->spacedata.first;
	int ofsx, ofsy;
	
	if(is_preview_allowed(sa)==0) {
		rem_blockhandler(sa, IMAGE_HANDLER_PREVIEW);
		G.scene->r.scemode &= ~R_COMP_CROP;	/* quite weak */
		return;
	}
	
	block= uiNewBlock(&sa->uiblocks, "image_panel_preview", UI_EMBOSS, UI_HELV, sa->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PREVIEW);  // for close and esc
	
	ofsx= -150+(sa->winx/2)/sima->blockscale;
	ofsy= -100+(sa->winy/2)/sima->blockscale;
	if(uiNewPanel(sa, block, "Preview", "Image", ofsx, ofsy, 300, 200)==0) return;
	
	uiBlockSetDrawExtraFunc(block, preview_cb);
	
}

static void image_blockhandlers(ScrArea *sa)
{
	SpaceImage *sima= sa->spacedata.first;
	short a;

	/* warning; blocks need to be freed each time, handlers dont remove  */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(sima->blockhandler[a]) {

		case IMAGE_HANDLER_PROPERTIES:
			image_panel_properties(sima->blockhandler[a+1]);
			break;
		case IMAGE_HANDLER_GAME_PROPERTIES:
			image_panel_game_properties(sima->blockhandler[a+1]);
			break;
		case IMAGE_HANDLER_TRANSFORM_PROPERTIES:
			if (EM_texFaceCheck())
				image_panel_transform_properties(sima->blockhandler[a+1]);
			break;
		case IMAGE_HANDLER_PAINT:
			image_panel_paint(sima->blockhandler[a+1]);
			break;		
		case IMAGE_HANDLER_CURVES:
			image_panel_curves(sima->blockhandler[a+1]);
			break;		
		case IMAGE_HANDLER_PREVIEW:
			image_panel_preview(sa, sima->blockhandler[a+1]);
			break;		
		}
		/* clear action value for event */
		sima->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);
}

void imagespace_composite_flipbook(ScrArea *sa)
{
	SpaceImage *sima= sa->spacedata.first;
	ImBuf *ibuf;
	int cfrao= G.scene->r.cfra;
	int sfra, efra;
	
	if(sa->spacetype!=SPACE_IMAGE)
		return;
	if(sima->iuser.frames<2)
		return;
	if(G.scene->nodetree==NULL)
		return;
	
	sfra= sima->iuser.sfra;
	efra= sima->iuser.sfra + sima->iuser.frames-1;
	G.scene->nodetree->test_break= blender_test_break;
	
	for(G.scene->r.cfra=sfra; G.scene->r.cfra<=efra; G.scene->r.cfra++) {
		
		set_timecursor(CFRA);
		
		BKE_image_all_free_anim_ibufs(CFRA);
		ntreeCompositTagAnimated(G.scene->nodetree);
		ntreeCompositExecTree(G.scene->nodetree, &G.scene->r, G.scene->r.cfra!=cfrao);	/* 1 is no previews */
		
		force_draw(0);
		
		ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
		/* save memory in flipbooks */
		if(ibuf)
			imb_freerectfloatImBuf(ibuf);
		
		if(blender_test_break())
			break;
	}
	G.scene->nodetree->test_break= NULL;
	waitcursor(0);
	
	play_anim(0);
	
	allqueue(REDRAWNODE, 1);
	allqueue(REDRAWIMAGE, 1);
	
	G.scene->r.cfra= cfrao;
}

static void imagespace_grid(SpaceImage *sima)
{
	float gridsize, gridstep= 1.0f/32.0f;
	float fac, blendfac;
	
	gridsize= sima->zoom;
	
	calc_image_view(sima, 'f');
	myortho2(sima->v2d.cur.xmin, sima->v2d.cur.xmax, sima->v2d.cur.ymin, sima->v2d.cur.ymax);
	
	BIF_ThemeColorShade(TH_BACK, 20);
	glRectf(0.0, 0.0, 1.0, 1.0);
	
	if(gridsize<=0.0f) return;
	
	if(gridsize<1.0f) {
		while(gridsize<1.0f) {
			gridsize*= 4.0;
			gridstep*= 4.0;
		}
	}
	else {
		while(gridsize>=4.0f) {
			gridsize/= 4.0;
			gridstep/= 4.0;
		}
	}
	
	/* the fine resolution level */
	blendfac= 0.25*gridsize - floor(0.25*gridsize);
	CLAMP(blendfac, 0.0, 1.0);
	BIF_ThemeColorShade(TH_BACK, (int)(20.0*(1.0-blendfac)));
	
	fac= 0.0f;
	glBegin(GL_LINES);
	while(fac<1.0) {
		glVertex2f(0.0f, fac);
		glVertex2f(1.0f, fac);
		glVertex2f(fac, 0.0f);
		glVertex2f(fac, 1.0f);
		fac+= gridstep;
	}
	
	/* the large resolution level */
	BIF_ThemeColor(TH_BACK);
	
	fac= 0.0f;
	while(fac<1.0) {
		glVertex2f(0.0f, fac);
		glVertex2f(1.0f, fac);
		glVertex2f(fac, 0.0f);
		glVertex2f(fac, 1.0f);
		fac+= 4.0*gridstep;
	}
	glEnd();
	
}

static void sima_draw_alpha_backdrop(SpaceImage *sima, float x1, float y1, float xsize, float ysize)
{
	float tile= sima->zoom*15.0f;
	float x, y, maxx, maxy;
	
	glColor3ub(100, 100, 100);
	glRectf(x1, y1, x1 + sima->zoom*xsize, y1 + sima->zoom*ysize);
	glColor3ub(160, 160, 160);
	
	maxx= x1+sima->zoom*xsize;
	maxy= y1+sima->zoom*ysize;
	
	for(x=0; x<xsize; x+=30) {
		for(y=0; y<ysize; y+=30) {
			float fx= x1 + sima->zoom*x;
			float fy= y1 + sima->zoom*y;
			float tilex= tile, tiley= tile;
			
			if(fx+tile > maxx)
				tilex= maxx-fx;
			if(fy+tile > maxy)
				tiley= maxy-fy;
			
			glRectf(fx, fy, fx + tilex, fy + tiley);
		}
	}
	for(x=15; x<xsize; x+=30) {
		for(y=15; y<ysize; y+=30) {
			float fx= x1 + sima->zoom*x;
			float fy= y1 + sima->zoom*y;
			float tilex= tile, tiley= tile;
			
			if(fx+tile > maxx)
				tilex= maxx-fx;
			if(fy+tile > maxy)
				tiley= maxy-fy;
			
			glRectf(fx, fy, fx + tilex, fy + tiley);
		}
	}
}

static void sima_draw_alpha_pixels(float x1, float y1, int rectx, int recty, unsigned int *recti)
{
	
	/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
	if(G.order==B_ENDIAN)
		glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_UNSIGNED_INT, recti);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
}

static void sima_draw_alpha_pixelsf(float x1, float y1, int rectx, int recty, float *rectf)
{
	float *trectf= MEM_mallocN(rectx*recty*4, "temp");
	int a, b;
	
	for(a= rectx*recty -1, b= 4*a+3; a>=0; a--, b-=4)
		trectf[a]= rectf[b];
	
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_FLOAT, trectf);
	MEM_freeN(trectf);
	/* ogl trick below is slower... (on ATI 9600) */
//	glColorMask(1, 0, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+3);
//	glColorMask(0, 1, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+2);
//	glColorMask(0, 0, 1, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+1);
//	glColorMask(1, 1, 1, 1);
}

static void sima_draw_zbuf_pixels(float x1, float y1, int rectx, int recty, int *recti)
{
	if(recti==NULL)
		return;
	
	/* zbuffer values are signed, so we need to shift color range */
	glPixelTransferf(GL_RED_SCALE, 0.5f);
	glPixelTransferf(GL_GREEN_SCALE, 0.5f);
	glPixelTransferf(GL_BLUE_SCALE, 0.5f);
	glPixelTransferf(GL_RED_BIAS, 0.5f);
	glPixelTransferf(GL_GREEN_BIAS, 0.5f);
	glPixelTransferf(GL_BLUE_BIAS, 0.5f);
	
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_INT, recti);
	
	glPixelTransferf(GL_RED_SCALE, 1.0f);
	glPixelTransferf(GL_GREEN_SCALE, 1.0f);
	glPixelTransferf(GL_BLUE_SCALE, 1.0f);
	glPixelTransferf(GL_RED_BIAS, 0.0f);
	glPixelTransferf(GL_GREEN_BIAS, 0.0f);
	glPixelTransferf(GL_BLUE_BIAS, 0.0f);
}

static void sima_draw_zbuffloat_pixels(float x1, float y1, int rectx, int recty, float *rect_float)
{
	float bias, scale, *rectf, clipend;
	int a;
	
	if(rect_float==NULL)
		return;
	
	if(G.scene->camera && G.scene->camera->type==OB_CAMERA) {
		bias= ((Camera *)G.scene->camera->data)->clipsta;
		clipend= ((Camera *)G.scene->camera->data)->clipend;
		scale= 1.0f/(clipend-bias);
	}
	else {
		bias= 0.1f;
		scale= 0.01f;
		clipend= 100.0f;
	}
	
	rectf= MEM_mallocN(rectx*recty*4, "temp");
	for(a= rectx*recty -1; a>=0; a--) {
		if(rect_float[a]>clipend)
			rectf[a]= 0.0f;
		else if(rect_float[a]<bias)
			rectf[a]= 1.0f;
		else {
			rectf[a]= 1.0f - (rect_float[a]-bias)*scale;
			rectf[a]*= rectf[a];
		}
	}
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_FLOAT, rectf);
	
	MEM_freeN(rectf);
}

static void imagewindow_draw_renderinfo(ScrArea *sa)
{
	SpaceImage *sima= sa->spacedata.first;
	rcti rect;
	float colf[3];
	char *str= sima->showspare?sima->info_spare:sima->info_str;
	
	if(str==NULL)
		return;
	
	rect= sa->winrct;
	rect.ymin= rect.ymax-RW_HEADERY;
	
	glaDefine2DArea(&rect);
	
	/* clear header rect */
	BIF_GetThemeColor3fv(TH_BACK, colf);
	glClearColor(colf[0]+0.1f, colf[1]+0.1f, colf[2]+0.1f, 1.0); 
	glClear(GL_COLOR_BUFFER_BIT);
	
	BIF_ThemeColor(TH_TEXT_HI);
	glRasterPos2i(12, 5);
	if(sima->showspare) {
		BMF_DrawString(G.fonts, "(Previous)");
		glRasterPos2i(72, 5);
	}		
	BMF_DrawString(G.fonts, str);
}

void drawimagespace(ScrArea *sa, void *spacedata)
{
	SpaceImage *sima= spacedata;
	ImBuf *ibuf= NULL;
	Brush *brush;
	float col[3];
	unsigned int *rect;
	float x1, y1;
	short sx, sy, dx, dy, show_render= 0, show_viewer= 0;
	
		/* If derived data is used then make sure that object
		 * is up-to-date... might not be the case because updates
		 * are normally done in drawview and could get here before
		 * drawing a View3D.
		 */
	if (!G.obedit && OBACT && (sima->flag & SI_DRAWSHADOW)) {
		object_handle_update(OBACT);
	}
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();
	
	if(sima->image && sima->image->source==IMA_SRC_VIEWER) {
		show_viewer= 1;
		if(sima->image->type==IMA_TYPE_R_RESULT)
			show_render= 1;
	}
	what_image(sima);
	
	if(sima->image) {
		
		/* UGLY hack? until now iusers worked fine... but for flipbook viewer we need this */
		if(sima->image->type==IMA_TYPE_COMPOSITE) {
			ImageUser *iuser= ntree_get_active_iuser(G.scene->nodetree);
			if(iuser) {
				BKE_image_user_calc_imanr(iuser, G.scene->r.cfra, 0);
				G.sima->iuser= *iuser;
			}
		}
		/* and we check for spare */
		ibuf= imagewindow_get_ibuf(sima);
	}
	
	if(ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL)) {
		imagespace_grid(sima);
		if(show_viewer==0)
			draw_tfaces();
	}
	else {
		float xim, yim, xoffs=0.0f, yoffs= 0.0f;
		
		if(image_preview_active(sa, &xim, &yim)) {
			xoffs= G.scene->r.disprect.xmin;
			yoffs= G.scene->r.disprect.ymin;
			glColor3ub(0,0,0);
			calc_image_view(sima, 'f');	
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glRectf(0.0f, 0.0f, 1.0f, 1.0f);
			glLoadIdentity();
		}
		else {
			xim= ibuf->x; yim= ibuf->y;
		}
		
		/* calc location */
		x1= sima->zoom*xoffs + ((float)sa->winx - sima->zoom*(float)xim)/2.0f;
		y1= sima->zoom*yoffs + ((float)sa->winy - sima->zoom*(float)yim)/2.0f;
	
		x1-= sima->zoom*sima->xof;
		y1-= sima->zoom*sima->yof;
		
		/* needed for gla draw */
		if(show_render) { 
			rcti rct= sa->winrct; 
			
			imagewindow_draw_renderinfo(sa);	/* calls glaDefine2DArea too */
			
			rct.ymax-=RW_HEADERY; 
			glaDefine2DArea(&rct);
		}
		else glaDefine2DArea(&sa->winrct);
		
		glPixelZoom((float)sima->zoom, (float)sima->zoom);
				
		if(sima->flag & SI_EDITTILE) {
			/* create char buffer from float if needed */
			if(ibuf->rect_float && ibuf->rect==NULL)
				IMB_rect_from_float(ibuf);

			glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
			
			glPixelZoom(1.0, 1.0);
			
			dx= ibuf->x/sima->image->xrep;
			dy= ibuf->y/sima->image->yrep;
			sy= (sima->curtile / sima->image->xrep);
			sx= sima->curtile - sy*sima->image->xrep;
	
			sx*= dx;
			sy*= dy;
			
			calc_image_view(sima, 'p');	/* pixel */
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			
			cpack(0x0);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRects(sx,  sy,  sx+dx-1,  sy+dy-1); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			cpack(0xFFFFFF);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRects(sx+1,  sy+1,  sx+dx,  sy+dy); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else if(sima->mode==SI_TEXTURE) {
			
			if(sima->image->tpageflag & IMA_TILES) {
				
				/* just leave this a while */
				if(sima->image->xrep<1) return;
				if(sima->image->yrep<1) return;
				
				if(sima->curtile >= sima->image->xrep*sima->image->yrep) 
					sima->curtile = sima->image->xrep*sima->image->yrep - 1; 
				
				dx= ibuf->x/sima->image->xrep;
				dy= ibuf->y/sima->image->yrep;
				
				sy= (sima->curtile / sima->image->xrep);
				sx= sima->curtile - sy*sima->image->xrep;
		
				sx*= dx;
				sy*= dy;
				
				/* create char buffer from float if needed */
				if(ibuf->rect_float && ibuf->rect==NULL)
					IMB_rect_from_float(ibuf);

				rect= get_part_from_ibuf(ibuf, sx, sy, sx+dx, sy+dy);
				
				/* rect= ibuf->rect; */
				for(sy= 0; sy+dy<=ibuf->y; sy+= dy) {
					for(sx= 0; sx+dx<=ibuf->x; sx+= dx) {
						glaDrawPixelsSafe(x1+sx*sima->zoom, y1+sy*sima->zoom, dx, dy, dx, GL_RGBA, GL_UNSIGNED_BYTE, rect);
					}
				}
				
				MEM_freeN(rect);
			}
			else {
				/* this part is generic image display */
				if(sima->flag & SI_SHOW_ALPHA) {
					if(ibuf->rect)
						sima_draw_alpha_pixels(x1, y1, ibuf->x, ibuf->y, ibuf->rect);
					else if(ibuf->rect_float && ibuf->channels==4)
						sima_draw_alpha_pixelsf(x1, y1, ibuf->x, ibuf->y, ibuf->rect_float);
				}
				else if(sima->flag & SI_SHOW_ZBUF && ((ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1)) == 0)) {
					if(ibuf->zbuf)
						sima_draw_zbuf_pixels(x1, y1, ibuf->x, ibuf->y, ibuf->zbuf);
					else if(ibuf->zbuf_float)
						sima_draw_zbuffloat_pixels(x1, y1, ibuf->x, ibuf->y, ibuf->zbuf_float);
					else if(ibuf->channels==1)
						sima_draw_zbuffloat_pixels(x1, y1, ibuf->x, ibuf->y, ibuf->rect_float);
				}
				else {
					if(sima->flag & SI_USE_ALPHA) {
						sima_draw_alpha_backdrop(sima, x1, y1, (float)ibuf->x, (float)ibuf->y);
						glEnable(GL_BLEND);
						glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					}
					
					/* detect if we need to redo the curve map. 
					   ibuf->rect is zero for compositor and render results after change 
					   convert to 32 bits always... drawing float rects isnt supported well (atis)
					
					   NOTE: if float buffer changes, we have to manually remove the rect
					*/
					
					if(ibuf->rect_float) {
						if(ibuf->rect==NULL) {
							if(image_curves_active(sa))
								curvemapping_do_ibuf(G.sima->cumap, ibuf);
							else 
								IMB_rect_from_float(ibuf);
						}
					}

					if(ibuf->rect)
						glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
//					else
//						glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_FLOAT, ibuf->rect_float);
					
					if(sima->flag & SI_USE_ALPHA)
						glDisable(GL_BLEND);
				}
			}
			
			brush= G.scene->toolsettings->imapaint.brush;
			if(brush && (G.scene->toolsettings->imapaint.tool == PAINT_TOOL_CLONE)) {
				int w, h;
				unsigned char *clonerect;

				/* this is not very efficient, but glDrawPixels doesn't allow
				   drawing with alpha */
				clonerect= alloc_alpha_clone_image(&w, &h);

				if(clonerect) {
					int offx, offy;
					offx = sima->zoom*ibuf->x * + brush->clone.offset[0];
					offy = sima->zoom*ibuf->y * + brush->clone.offset[1];

					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glaDrawPixelsSafe(x1 + offx, y1 + offy, w, h, w, GL_RGBA, GL_UNSIGNED_BYTE, clonerect);
					glDisable(GL_BLEND);

					MEM_freeN(clonerect);
				}
			}
			
			glPixelZoom(1.0, 1.0);
			
			if(show_viewer==0) { 
				draw_tfaces();
			}
		}

		glPixelZoom(1.0, 1.0);

		calc_image_view(sima, 'f');	/* float */
	}

	draw_image_transform(ibuf);

	mywinset(sa->win);	/* restore scissor after gla call... */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	if(G.rendering==0) {
		draw_image_view_tool();
		draw_image_view_icon();
	}
	draw_area_emboss(sa);

	/* it is important to end a view in a transform compatible with buttons */
	bwin_scalematrix(sa->win, sima->blockscale, sima->blockscale, sima->blockscale);
	if(!(G.rendering && show_render))
		image_blockhandlers(sa);

	sa->win_swap= WIN_BACK_OK;
}

static void image_zoom_power_of_two(void)
{
	/* Make zoom a power of 2 */

	G.sima->zoom = 1 / G.sima->zoom;
	G.sima->zoom = log(G.sima->zoom) / log(2);
	G.sima->zoom = ceil(G.sima->zoom);
	G.sima->zoom = pow(2, G.sima->zoom);
	G.sima->zoom = 1 / G.sima->zoom;
}

static void image_zoom_set_factor(float zoomfac)
{
	SpaceImage *sima= curarea->spacedata.first;
	int width, height;

	if (zoomfac <= 0.0f)
		return;

	sima->zoom *= zoomfac;

	if (sima->zoom > 0.1f && sima->zoom < 4.0f)
		return;

	/* check zoom limits */

	calc_image_view(G.sima, 'p');
	width= 256;
	height= 256;
	if (sima->image) {
		ImBuf *ibuf= imagewindow_get_ibuf(sima);

		if (ibuf) {
			float xim, yim;
			/* I know a bit weak... but preview uses not actual image size */
			if(image_preview_active(curarea, &xim, &yim)) {
				width= (int) xim;
				height= (int) yim;
			}
			else {
				width= ibuf->x;
				height= ibuf->y;
			}
		}
	}
	width *= sima->zoom;
	height *= sima->zoom;

	if ((width < 4) && (height < 4))
		sima->zoom /= zoomfac;
	else if((curarea->winrct.xmax - curarea->winrct.xmin) <= sima->zoom)
		sima->zoom /= zoomfac;
	else if((curarea->winrct.ymax - curarea->winrct.ymin) <= sima->zoom)
		sima->zoom /= zoomfac;
}

void image_viewmove(int mode)
{
	short mval[2], mvalo[2], zoom0;
	int oldcursor;
	Window *win;
	
	getmouseco_sc(mvalo);
	zoom0= G.sima->zoom;
	
	oldcursor=get_cursor();
	win=winlay_get_active_window();
	
	SetBlenderCursor(BC_NSEW_SCROLLCURSOR);
	
	while(get_mbut()&(L_MOUSE|M_MOUSE)) {

		getmouseco_sc(mval);

		if(mvalo[0]!=mval[0] || mvalo[1]!=mval[1]) {
		
			if(mode==0) {
				G.sima->xof += (mvalo[0]-mval[0])/G.sima->zoom;
				G.sima->yof += (mvalo[1]-mval[1])/G.sima->zoom;
			}
			else if (mode==1) {
				float factor;

				factor= 1.0+(float)(mvalo[0]-mval[0]+mvalo[1]-mval[1])/300.0;
				image_zoom_set_factor(factor);
			}

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		else BIF_wait_for_statechange();
	}
	window_set_cursor(win, oldcursor);
	
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
}

void image_viewzoom(unsigned short event, int invert)
{
	SpaceImage *sima= curarea->spacedata.first;

	if(event==WHEELDOWNMOUSE || event==PADMINUS)
		image_zoom_set_factor((U.uiflag & USER_WHEELZOOMDIR)? 1.25: 0.8);
	else if(event==WHEELUPMOUSE || event==PADPLUSKEY)
		image_zoom_set_factor((U.uiflag & USER_WHEELZOOMDIR)? 0.8: 1.25);
	else if(event==PAD1)
		sima->zoom= 1.0;
	else if(event==PAD2)
		sima->zoom= (invert)? 2.0: 0.5;
	else if(event==PAD4)
		sima->zoom= (invert)? 4.0: 0.25;
	else if(event==PAD8)
		sima->zoom= (invert)? 8.0: 0.125;
	
	/* ensure pixel exact locations for draw */
	sima->xof= (int)sima->xof;
	sima->yof= (int)sima->yof;
	
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
}

/**
 * Updates the fields of the View2D member of the SpaceImage struct.
 * Default behavior is to reset the position of the image and set the zoom to 1
 * If the image will not fit within the window rectangle, the zoom is adjusted
 *
 * @return    void
 *   
 */
void image_home(void)
{
	ImBuf *ibuf;
	int width, height, imgwidth, imgheight;
	float zoomX, zoomY;

	if (curarea->spacetype != SPACE_IMAGE) return;
	ibuf= imagewindow_get_ibuf(G.sima);
	
	if (ibuf == NULL) {
		imgwidth = 256;
		imgheight = 256;
	}
	else {
		imgwidth = ibuf->x;
		imgheight = ibuf->y;
	}

	/* Check if the image will fit in the image with zoom==1 */
	width = curarea->winx;
	height = curarea->winy;
	if (((imgwidth >= width) || (imgheight >= height)) && 
		((width > 0) && (height > 0))) {
		/* Find the zoom value that will fit the image in the image space */
		zoomX = ((float)width) / ((float)imgwidth);
		zoomY = ((float)height) / ((float)imgheight);
		G.sima->zoom= MIN2(zoomX, zoomY);

		image_zoom_power_of_two();
	}
	else {
		G.sima->zoom= 1.0f;
	}

	G.sima->xof= G.sima->yof= 0.0f;
	
	calc_image_view(G.sima, 'p');
	
	scrarea_queue_winredraw(curarea);
}

void image_viewcenter(void)
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);
	float size, min[2], max[2], d[2], xim=256.0f, yim=256.0f;

	if( is_uv_tface_editing_allowed()==0 ) return;

	if (!minmax_tface_uv(min, max)) return;

	if(ibuf) {
		xim= ibuf->x;
		yim= ibuf->y;
	}

	G.sima->xof= (int) (((min[0] + max[0])*0.5f - 0.5f)*xim);
	G.sima->yof= (int) (((min[1] + max[1])*0.5f - 0.5f)*yim);

	d[0] = max[0] - min[0];
	d[1] = max[1] - min[1];
	size= 0.5*MAX2(d[0], d[1])*MAX2(xim, yim)/256.0f;
	
	if(size<=0.01) size= 0.01;

	G.sima->zoom= 0.7/size;

	calc_image_view(G.sima, 'p');

	scrarea_queue_winredraw(curarea);
}


/* *********************** render callbacks ***************** */

/* set on initialize render, only one render output to imagewindow can exist, so the global isnt dangerous yet :) */
static ScrArea *image_area= NULL;

/* can get as well the full picture, as the parts while rendering */
static void imagewindow_progress(ScrArea *sa, RenderResult *rr, volatile rcti *renrect)
{
	SpaceImage *sima= sa->spacedata.first;
	float x1, y1, *rectf= NULL;
	unsigned int *rect32= NULL;
	int ymin, ymax, xmin, xmax;
	
	/* if renrect argument, we only display scanlines */
	if(renrect) {
		/* if ymax==recty, rendering of layer is ready, we should not draw, other things happen... */
		if(rr->renlay==NULL || renrect->ymax>=rr->recty)
			return;
		
		/* xmin here is first subrect x coord, xmax defines subrect width */
		xmin = renrect->xmin;
		xmax = renrect->xmax - xmin;
		if (xmax<2) return;
		
		ymin= renrect->ymin;
		ymax= renrect->ymax - ymin;
		if(ymax<2)
			return;
		renrect->ymin= renrect->ymax;
	}
	else {
		xmin = ymin = 0;
		xmax = rr->rectx - 2*rr->crop;
		ymax = rr->recty - 2*rr->crop;
	}
	
	/* image window cruft */
	
	/* find current float rect for display, first case is after composit... still weak */
	if(rr->rectf)
		rectf= rr->rectf;
	else {
		if(rr->rect32)
			rect32= (unsigned int *)rr->rect32;
		else {
			if(rr->renlay==NULL || rr->renlay->rectf==NULL) return;
			rectf= rr->renlay->rectf;
		}
	}
	if(rectf) {
		/* if scanline updates... */
		rectf+= 4*(rr->rectx*ymin + xmin);
		
		/* when rendering more pixels than needed, we crop away cruft */
		if(rr->crop)
			rectf+= 4*(rr->crop*rr->rectx + rr->crop);
	}
	
	/* tilerect defines drawing offset from (0,0) */
	/* however, tilerect (xmin, ymin) is first pixel */
	x1 = sima->centx + (rr->tilerect.xmin + rr->crop + xmin)*sima->zoom;
	y1 = sima->centy + (rr->tilerect.ymin + rr->crop + ymin)*sima->zoom;
	
	/* needed for gla draw */
	{ rcti rct= sa->winrct; rct.ymax-= RW_HEADERY; glaDefine2DArea(&rct);}

	glPixelZoom((float)sima->zoom, (float)sima->zoom);
	
	if(rect32)
		glaDrawPixelsSafe(x1, y1, xmax, ymax, rr->rectx, GL_RGBA, GL_UNSIGNED_BYTE, rect32);
	else
		glaDrawPixelsSafe_to32(x1, y1, xmax, ymax, rr->rectx, rectf);
	
	glPixelZoom(1.0, 1.0);
	
}


/* in render window; display a couple of scanlines of rendered image */
/* NOTE: called while render, so no malloc allowed! */
static void imagewindow_progress_display_cb(RenderResult *rr, volatile rcti *rect)
{
	
	if (image_area) {
		
		imagewindow_progress(image_area, rr, rect);

		/* no screen_swapbuffers, prevent any other window to draw */
		myswapbuffers();
	}
}

/* unused, init_display_cb is called on each render */
static void imagewindow_clear_display_cb(RenderResult *rr)
{
	if (image_area) {
	}
}

/* returns biggest area that is not uv/image editor. Note that it uses buttons */
/* window as the last possible alternative.									   */
static ScrArea *biggest_non_image_area(void)
{
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0, bwmaxsize= 0;
	short foundwin= 0;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->winx > 10 && sa->winy > 10) {
			size= sa->winx*sa->winy;
			if(sa->spacetype == SPACE_BUTS) {
				if(foundwin == 0 && size > bwmaxsize) {
					bwmaxsize= size;
					big= sa;	
				}
			}
			else if(sa->spacetype != SPACE_IMAGE && size > maxsize) {
				maxsize= size;
				big= sa;
				foundwin= 1;
			}
		}
	}
		
	return big;
}

static ScrArea *biggest_area(void)
{
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		size= sa->winx*sa->winy;
		if(size > maxsize) {
			maxsize= size;
			big= sa;
		}
	}
	return big;
}


/* if R_DISPLAYIMAGE
      use Image Window showing Render Result
	  else: turn largest non-image area into Image Window (not to frustrate texture or composite usage)
	  else: then we use Image Window anyway...
   if R_DISPSCREEN
      make a new temp fullscreen area with Image Window
*/

static ScrArea *find_area_showing_r_result(void)
{
	ScrArea *sa;
	SpaceImage *sima;
	
	/* find an imagewindow showing render result */
	for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			sima= sa->spacedata.first;
			if(sima->image && sima->image->type==IMA_TYPE_R_RESULT)
				break;
		}
	}
	return sa;
}

static ScrArea *imagewindow_set_render_display(void)
{
	ScrArea *sa;
	SpaceImage *sima;
	
	sa= find_area_showing_r_result();
	
	if(sa==NULL) {
		/* find largest open non-image area */
		sa= biggest_non_image_area();
		if(sa) {
			newspace(sa, SPACE_IMAGE);
			sima= sa->spacedata.first;
			
			/* makes ESC go back to prev space */
			sima->flag |= SI_PREVSPACE;
		}
		else {
			/* use any area of decent size */
			sa= biggest_area();
			if(sa->spacetype!=SPACE_IMAGE) {
				newspace(sa, SPACE_IMAGE);
				sima= sa->spacedata.first;
				
				/* makes ESC go back to prev space */
				sima->flag |= SI_PREVSPACE;
			}
		}
	}
	
	sima= sa->spacedata.first;
	
	/* get the correct image, and scale it */
	sima->image= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	
	if(G.displaymode==R_DISPLAYSCREEN) {
		if(sa->full==0) {
			sima->flag |= SI_FULLWINDOW;
			/* fullscreen works with lousy curarea */
			curarea= sa;
			area_fullscreen();
			sa= curarea;
		}
	}
	
	return sa;
}

static void imagewindow_init_display_cb(RenderResult *rr)
{
	
	image_area= imagewindow_set_render_display();
	
	if(image_area) {
		SpaceImage *sima= image_area->spacedata.first;
		
		areawinset(image_area->win);
		
		if(sima->info_str==NULL)
			sima->info_str= MEM_callocN(RW_MAXTEXT, "info str imagewin");
		
		/* calc location using original size (tiles don't tell) */
		sima->centx= (image_area->winx - sima->zoom*(float)rr->rectx)/2.0f;
		sima->centy= (image_area->winy - sima->zoom*(float)rr->recty)/2.0f;
		
		sima->centx-= sima->zoom*sima->xof;
		sima->centy-= sima->zoom*sima->yof;
		
		drawimagespace(image_area, sima);
		if(image_area->headertype) scrarea_do_headdraw(image_area);
		screen_swapbuffers();
		
		allqueue(REDRAWIMAGE, 0);	/* redraw in end */
	}
}

/* coming from BIF_toggle_render_display() */
void imagewindow_toggle_render(void)
{
	ScrArea *sa;
	
	/* check if any imagewindow is showing temporal render output */
	for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			SpaceImage *sima= sa->spacedata.first;
			
			if(sima->image && sima->image->type==IMA_TYPE_R_RESULT)
				if(sima->flag & (SI_PREVSPACE|SI_FULLWINDOW))
					break;
		}
	}
	if(sa) {
		addqueue(sa->win, ESCKEY, 1);	/* also returns from fullscreen */
	}
	else {
		sa= imagewindow_set_render_display();
		scrarea_queue_headredraw(sa);
		scrarea_queue_winredraw(sa);
	}
}

/* NOTE: called while render, so no malloc allowed! */
static void imagewindow_renderinfo_cb(RenderStats *rs)
{
	
	if(image_area) {
		SpaceImage *sima= image_area->spacedata.first;
		
		if(rs)
			make_renderinfo_string(rs, sima->info_str);

		imagewindow_draw_renderinfo(image_area);
		
		/* no screen_swapbuffers, prevent any other window to draw */
		myswapbuffers();
	}
}

void imagewindow_render_callbacks(Render *re)
{
	RE_display_init_cb(re, imagewindow_init_display_cb);
	RE_display_draw_cb(re, imagewindow_progress_display_cb);
	RE_display_clear_cb(re, imagewindow_clear_display_cb);
	RE_stats_draw_cb(re, imagewindow_renderinfo_cb);	
}

void imagewin_store_spare(void)
{
	ScrArea *sa= find_area_showing_r_result();

	if(sa) {
		ImBuf *ibuf;
		SpaceImage *sima= sa->spacedata.first;
		
		if(sima->spare==NULL)
			return;
		
		/* only store when it does not show spare */
		if(sima->showspare==0)
			return;
		sima->showspare= 0;
		
		/* free spare */
		IMB_freeImBuf(sima->spare);
		
		/* make a copy of render result */
		ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
		sima->spare= IMB_dupImBuf(ibuf);
		
		if(sima->info_str)
			BLI_strncpy(sima->info_spare, sima->info_str, RW_MAXTEXT);

	}
}

/* context: in current image window? */
void imagewindow_swap_render_rects(void)
{
	ScrArea *sa= find_area_showing_r_result();
					
	if(sa) {
		SpaceImage *sima= sa->spacedata.first;
		ImBuf *ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
		if(ibuf) {
			
			sima->showspare ^= 1;
			
			if(sima->spare==NULL)
				sima->spare= IMB_allocImBuf(ibuf->x, ibuf->y, 32, 0, 0);
			if(sima->info_spare==NULL)
				sima->info_spare= MEM_callocN(RW_MAXTEXT, "info str imagewin");
			
			allqueue(REDRAWIMAGE, 0);
		}
	}
}

