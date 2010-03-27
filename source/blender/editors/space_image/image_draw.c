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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_gpencil.h"
#include "ED_image.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"


#include "RE_pipeline.h"

#include "image_intern.h"

#define HEADER_HEIGHT 18

static void image_verify_buffer_float(SpaceImage *sima, Image *ima, ImBuf *ibuf, int color_manage)
{
	/* detect if we need to redo the curve map.
	   ibuf->rect is zero for compositor and render results after change 
	   convert to 32 bits always... drawing float rects isnt supported well (atis)
	
	   NOTE: if float buffer changes, we have to manually remove the rect
	*/

	if(ibuf->rect_float) {
		if(ibuf->rect==NULL) {
			if (color_manage) {
					if (ima && ima->source == IMA_SRC_VIEWER)
						ibuf->profile = IB_PROFILE_LINEAR_RGB;
			} else {
				ibuf->profile = IB_PROFILE_NONE;
			}
			IMB_rect_from_float(ibuf);
		}
	}
}

static void draw_render_info(Scene *scene, Image *ima, ARegion *ar)
{
	RenderResult *rr;
	rcti rect;
	float colf[3];
	
	rr= BKE_image_acquire_renderresult(scene, ima);

	if(rr && rr->text) {
		rect= ar->winrct;
		rect.xmin= 0;
		rect.ymin= ar->winrct.ymax - ar->winrct.ymin - HEADER_HEIGHT;
		rect.xmax= ar->winrct.xmax - ar->winrct.xmin;
		rect.ymax= ar->winrct.ymax - ar->winrct.ymin;
		
		/* clear header rect */
		UI_GetThemeColor3fv(TH_BACK, colf);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(colf[0]+0.1f, colf[1]+0.1f, colf[2]+0.1f, 0.5f);
		glRecti(rect.xmin, rect.ymin, rect.xmax, rect.ymax+1);
		glDisable(GL_BLEND);
		
		UI_ThemeColor(TH_TEXT_HI);

		UI_DrawString(12, rect.ymin + 5, rr->text);
	}

	BKE_image_release_renderresult(scene, ima);
}

void draw_image_info(ARegion *ar, int channels, int x, int y, char *cp, float *fp, int *zp, float *zpf)
{
	char str[256];
	int ofs;
	
	ofs= sprintf(str, "X: %d Y: %d ", x, y);
	if(cp)
		ofs+= sprintf(str+ofs, "| R: %d G: %d B: %d A: %d ", cp[0], cp[1], cp[2], cp[3]);

	if(fp) {
		if(channels==4)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f A: %.3f ", fp[0], fp[1], fp[2], fp[3]);
		else if(channels==1)
			ofs+= sprintf(str+ofs, "| Val: %.3f ", fp[0]);
		else if(channels==3)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f ", fp[0], fp[1], fp[2]);
	}

	if(zp)
		ofs+= sprintf(str+ofs, "| Z: %.4f ", 0.5+0.5*(((float)*zp)/(float)0x7fffffff));
	if(zpf)
		ofs+= sprintf(str+ofs, "| Z: %.3f ", *zpf);
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glColor4f(.0,.0,.0,.25);
	glRectf(0.0, 0.0, ar->winrct.xmax - ar->winrct.xmin + 1, 30.0);
	glDisable(GL_BLEND);
	
	glColor3ub(255, 255, 255);
	
	UI_DrawString(10, 10, str);
}

void draw_image_line(struct ARegion *ar, int x1, int y1, int x2, int y2)
{
	glColor3ub(0,0,0);
	glBegin(GL_LINES);
	
	glVertex2i(x1, y1);
	glVertex2i(x2, y2);
	
	glEnd();
}

/* image drawing */

static void draw_image_grid(ARegion *ar, float zoomx, float zoomy)
{
	float gridsize, gridstep= 1.0f/32.0f;
	float fac, blendfac;
	int x1, y1, x2, y2;
	
	/* the image is located inside (0,0),(1, 1) as set by view2d */
	UI_ThemeColorShade(TH_BACK, 20);

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x1, &y1);
	UI_view2d_to_region_no_clip(&ar->v2d, 1.0f, 1.0f, &x2, &y2);
	glRectf(x1, y1, x2, y2);

	/* gridsize adapted to zoom level */
	gridsize= 0.5f*(zoomx+zoomy);
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
	UI_ThemeColorShade(TH_BACK, (int)(20.0*(1.0-blendfac)));
	
	fac= 0.0f;
	glBegin(GL_LINES);
	while(fac<1.0f) {
		glVertex2f(x1, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x2, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y1);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y2);
		fac+= gridstep;
	}
	
	/* the large resolution level */
	UI_ThemeColor(TH_BACK);
	
	fac= 0.0f;
	while(fac<1.0f) {
		glVertex2f(x1, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x2, y1*(1.0f-fac) + y2*fac);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y1);
		glVertex2f(x1*(1.0f-fac) + x2*fac, y2);
		fac+= 4.0f*gridstep;
	}
	glEnd();
}

static void sima_draw_alpha_backdrop(float x1, float y1, float xsize, float ysize, float zoomx, float zoomy)
{
	GLubyte checker_stipple[32*32/8] =
	{
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
		0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255, \
	};
	
	glColor3ub(100, 100, 100);
	glRectf(x1, y1, x1 + zoomx*xsize, y1 + zoomy*ysize);
	glColor3ub(160, 160, 160);

	glEnable(GL_POLYGON_STIPPLE);
	glPolygonStipple(checker_stipple);
	glRectf(x1, y1, x1 + zoomx*xsize, y1 + zoomy*ysize);
	glDisable(GL_POLYGON_STIPPLE);
}

static void sima_draw_alpha_pixels(float x1, float y1, int rectx, int recty, unsigned int *recti)
{
	
	/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
	if(ENDIAN_ORDER == B_ENDIAN)
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

#ifdef WITH_LCMS
static void sima_draw_colorcorrected_pixels(float x1, float y1, ImBuf *ibuf)
{
	colorcorrection_do_ibuf(ibuf, "MONOSCNR.ICM"); /* path is hardcoded here, find some place better */
	
	glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->crect);
}
#endif

static void sima_draw_zbuf_pixels(float x1, float y1, int rectx, int recty, int *recti)
{
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

static void sima_draw_zbuffloat_pixels(Scene *scene, float x1, float y1, int rectx, int recty, float *rect_float)
{
	float bias, scale, *rectf, clipend;
	int a;
	
	if(scene->camera && scene->camera->type==OB_CAMERA) {
		bias= ((Camera *)scene->camera->data)->clipsta;
		clipend= ((Camera *)scene->camera->data)->clipend;
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

static void draw_image_buffer(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	int x, y;
	int color_manage = scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, fx, fy, &x, &y);

	/* this part is generic image display */
	if(sima->flag & SI_SHOW_ALPHA) {
		if(ibuf->rect)
			sima_draw_alpha_pixels(x, y, ibuf->x, ibuf->y, ibuf->rect);
		else if(ibuf->rect_float && ibuf->channels==4)
			sima_draw_alpha_pixelsf(x, y, ibuf->x, ibuf->y, ibuf->rect_float);
	}
	else if(sima->flag & SI_SHOW_ZBUF && (ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1))) {
		if(ibuf->zbuf)
			sima_draw_zbuf_pixels(x, y, ibuf->x, ibuf->y, ibuf->zbuf);
		else if(ibuf->zbuf_float)
			sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->zbuf_float);
		else if(ibuf->channels==1)
			sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->rect_float);
	}
#ifdef WITH_LCMS
	else if(sima->flag & SI_COLOR_CORRECTION) {
		image_verify_buffer_float(sima, ima, ibuf, color_manage);
		
		sima_draw_colorcorrected_pixels(x, y, ibuf);

	}
#endif
	else {
		if(sima->flag & SI_USE_ALPHA) {
			sima_draw_alpha_backdrop(x, y, ibuf->x, ibuf->y, zoomx, zoomy);

			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}

		/* we don't draw floats buffers directly but
		 * convert them, and optionally apply curves */
		image_verify_buffer_float(sima, ima, ibuf, color_manage);

		if(ibuf->rect)
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
		/*else
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_FLOAT, ibuf->rect_float);*/
		
		if(sima->flag & SI_USE_ALPHA)
			glDisable(GL_BLEND);
	}

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
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

static void draw_image_buffer_tiled(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	unsigned int *rect;
	int dx, dy, sx, sy, x, y;
	int color_manage = scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;

	/* verify valid values, just leave this a while */
	if(ima->xrep<1) return;
	if(ima->yrep<1) return;
	
	glPixelZoom(zoomx, zoomy);

	if(sima->curtile >= ima->xrep*ima->yrep) 
		sima->curtile = ima->xrep*ima->yrep - 1; 
	
	/* create char buffer from float if needed */
	image_verify_buffer_float(sima, ima, ibuf, color_manage);

	/* retrieve part of image buffer */
	dx= ibuf->x/ima->xrep;
	dy= ibuf->y/ima->yrep;
	sx= (sima->curtile % ima->xrep)*dx;
	sy= (sima->curtile / ima->xrep)*dy;
	rect= get_part_from_ibuf(ibuf, sx, sy, sx+dx, sy+dy);
	
	/* draw repeated */
	for(sy=0; sy+dy<=ibuf->y; sy+= dy) {
		for(sx=0; sx+dx<=ibuf->x; sx+= dx) {
			UI_view2d_to_region_no_clip(&ar->v2d, fx + (float)sx/(float)ibuf->x, fy + (float)sy/(float)ibuf->y, &x, &y);

			glaDrawPixelsSafe(x, y, dx, dy, dx, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		}
	}

	glPixelZoom(1.0f, 1.0f);

	MEM_freeN(rect);
}

static void draw_image_buffer_repeated(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float zoomx, float zoomy)
{
	float x, y;
	double time_current;
	
	time_current = PIL_check_seconds_timer();

	for(x=floor(ar->v2d.cur.xmin); x<ar->v2d.cur.xmax; x += 1.0f) { 
		for(y=floor(ar->v2d.cur.ymin); y<ar->v2d.cur.ymax; y += 1.0f) { 
			if(ima && (ima->tpageflag & IMA_TILES))
				draw_image_buffer_tiled(sima, ar, scene, ima, ibuf, x, y, zoomx, zoomy);
			else
				draw_image_buffer(sima, ar, scene, ima, ibuf, x, y, zoomx, zoomy);

			/* only draw until running out of time */
			if((PIL_check_seconds_timer() - time_current) > 0.25)
				return;
		}
	}
}

/* draw uv edit */

/* draw grease pencil */
void draw_image_grease_pencil(bContext *C, short onlyv2d)
{
	/* draw in View2D space? */
	if (onlyv2d) {
		/* assume that UI_view2d_ortho(C) has been called... */
		SpaceImage *sima= (SpaceImage *)CTX_wm_space_data(C);
		void *lock;
		ImBuf *ibuf= ED_space_image_acquire_buffer(sima, &lock);
		
		/* draw grease-pencil ('image' strokes) */
		//if (sima->flag & SI_DISPGP)
			draw_gpencil_2dimage(C, ibuf);

		ED_space_image_release_buffer(sima, lock);
	}
	else {
		/* assume that UI_view2d_restore(C) has been called... */
		//SpaceImage *sima= (SpaceImage *)CTX_wm_space_data(C);
		
		/* draw grease-pencil ('screen' strokes) */
		//if (sima->flag & SI_DISPGP)
			draw_gpencil_2dview(C, 0);
	}
}

/* XXX becomes WM paint cursor */
#if 0
static void draw_image_view_tool(Scene *scene)
{
	ToolSettings *settings= scene->toolsettings;
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
#endif

static unsigned char *get_alpha_clone_image(Scene *scene, int *width, int *height)
{
	Brush *brush = paint_brush(&scene->toolsettings->imapaint.paint);
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

static void draw_image_paint_helpers(SpaceImage *sima, ARegion *ar, Scene *scene, float zoomx, float zoomy)
{
	Brush *brush;
	int x, y, w, h;
	unsigned char *clonerect;

	brush= paint_brush(&scene->toolsettings->imapaint.paint);

	if(brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE)) {
		/* this is not very efficient, but glDrawPixels doesn't allow
		   drawing with alpha */
		clonerect= get_alpha_clone_image(scene, &w, &h);

		if(clonerect) {
			UI_view2d_to_region_no_clip(&ar->v2d, brush->clone.offset[0], brush->clone.offset[1], &x, &y);

			glPixelZoom(zoomx, zoomy);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glaDrawPixelsSafe(x, y, w, h, w, GL_RGBA, GL_UNSIGNED_BYTE, clonerect);
			glDisable(GL_BLEND);

			glPixelZoom(1.0, 1.0);

			MEM_freeN(clonerect);
		}
	}
}

/* draw main image area */

void draw_image_main(SpaceImage *sima, ARegion *ar, Scene *scene)
{
	Image *ima;
	ImBuf *ibuf;
	float zoomx, zoomy;
	int show_viewer, show_render;
	void *lock;

	/* XXX can we do this in refresh? */
#if 0
	what_image(sima);
	
	if(sima->image) {
		ED_image_aspect(sima->image, &xuser_asp, &yuser_asp);
		
		/* UGLY hack? until now iusers worked fine... but for flipbook viewer we need this */
		if(sima->image->type==IMA_TYPE_COMPOSITE) {
			ImageUser *iuser= ntree_get_active_iuser(scene->nodetree);
			if(iuser) {
				BKE_image_user_calc_imanr(iuser, scene->r.cfra, 0);
				sima->iuser= *iuser;
			}
		}
		/* and we check for spare */
		ibuf= ED_space_image_buffer(sima);
	}
#endif

	/* retrieve the image and information about it */
	ima= ED_space_image(sima);
	ibuf= ED_space_image_acquire_buffer(sima, &lock);
	ED_space_image_zoom(sima, ar, &zoomx, &zoomy);

	show_viewer= (ima && ima->source == IMA_SRC_VIEWER);
	show_render= (show_viewer && ima->type == IMA_TYPE_R_RESULT);

	/* draw the image or grid */
	if(ibuf==NULL)
		draw_image_grid(ar, zoomx, zoomy);
	else if(sima->flag & SI_DRAW_TILE)
		draw_image_buffer_repeated(sima, ar, scene, ima, ibuf, zoomx, zoomy);
	else if(ima && (ima->tpageflag & IMA_TILES))
		draw_image_buffer_tiled(sima, ar, scene, ima, ibuf, 0.0f, 0.0, zoomx, zoomy);
	else
		draw_image_buffer(sima, ar, scene, ima, ibuf, 0.0f, 0.0f, zoomx, zoomy);

	/* paint helpers */
	draw_image_paint_helpers(sima, ar, scene, zoomx, zoomy);


	/* XXX integrate this code */
#if 0
	if(ibuf) {
		float xoffs=0.0f, yoffs= 0.0f;
		
		if(image_preview_active(sa, &xim, &yim)) {
			xoffs= scene->r.disprect.xmin;
			yoffs= scene->r.disprect.ymin;
			glColor3ub(0,0,0);
			calc_image_view(sima, 'f');	
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glRectf(0.0f, 0.0f, 1.0f, 1.0f);
			glLoadIdentity();
		}
	}
#endif

	ED_space_image_release_buffer(sima, lock);

	/* render info */
	if(ima && show_render)
		draw_render_info(scene, ima, ar);
}

