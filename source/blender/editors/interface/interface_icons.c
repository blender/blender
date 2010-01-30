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
 * Contributors: Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include <direct.h>
#include "BLI_winstuff.h"
#endif   
#include "MEM_guardedalloc.h"

#include "GPU_extensions.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_storage_types.h"

#include "DNA_material_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_icons.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"
#include "ED_render.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h" /* elubie: should be removed once the enum for the ICONS is in BIF_preview_icons.h */

#include "interface_intern.h"


#define ICON_IMAGE_W		600
#define ICON_IMAGE_H		640

#define ICON_GRID_COLS		26
#define ICON_GRID_ROWS		30

#define ICON_GRID_MARGIN	5
#define ICON_GRID_W		16
#define ICON_GRID_H		16

typedef struct IconImage {
	int w;
	int h;
	unsigned int *rect; 
} IconImage;

typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

#define ICON_TYPE_PREVIEW	0
#define ICON_TYPE_TEXTURE	1
#define ICON_TYPE_BUFFER	2
#define ICON_TYPE_VECTOR	3

typedef struct DrawInfo {
	int type;

	union {
		/* type specific data */
		struct {
			VectorDrawFunc func;
		} vector;
		struct {
			IconImage* image;
		} buffer;
		struct {
			int x, y, w, h;
		} texture;
	} data;
} DrawInfo;

typedef struct IconTexture {
	GLuint id;
	int w;
	int h;
	float invw;
	float invh;
} IconTexture;

/* ******************* STATIC LOCAL VARS ******************* */
/* static here to cache results of icon directory scan, so it's not 
 * scanning the filesystem each time the menu is drawn */
static struct ListBase iconfilelist = {0, 0};
static IconTexture icongltex = {0, 0, 0, 0.0f, 0.0f};

/* **************************************************** */

static void def_internal_icon(ImBuf *bbuf, int icon_id, int xofs, int yofs, int size, int type)
{
	Icon *new_icon = NULL;
	IconImage *iimg = NULL;
	DrawInfo *di;
	int y = 0;
	int imgsize = 0;

	new_icon = MEM_callocN(sizeof(Icon), "texicon");

	new_icon->obj = 0; /* icon is not for library object */
	new_icon->type = 0;	

	di = MEM_callocN(sizeof(DrawInfo), "drawinfo");
	di->type= type;

	if(type == ICON_TYPE_TEXTURE) {
		di->data.texture.x= xofs;
		di->data.texture.y= yofs;
		di->data.texture.w= size;
		di->data.texture.h= size;
	}
	else if(type == ICON_TYPE_BUFFER) {
		iimg = MEM_mallocN(sizeof(IconImage), "icon_img");
		iimg->rect = MEM_mallocN(size*size*sizeof(unsigned int), "icon_rect");
		iimg->w = size;
		iimg->h = size;

		/* Here we store the rect in the icon - same as before */
		imgsize = bbuf->x;
		for (y=0; y<size; y++) {
			memcpy(&iimg->rect[y*size], &bbuf->rect[(y+yofs)*imgsize+xofs], size*sizeof(int));
		}

		di->data.buffer.image = iimg;
	}

	new_icon->drawinfo_free = UI_icons_free_drawinfo;
	new_icon->drawinfo = di;

	BKE_icon_set(icon_id, new_icon);
}

static void def_internal_vicon( int icon_id, VectorDrawFunc drawFunc)
{
	Icon *new_icon = NULL;
	DrawInfo* di;

	new_icon = MEM_callocN(sizeof(Icon), "texicon");

	new_icon->obj = 0; /* icon is not for library object */
	new_icon->type = 0;

	di = MEM_callocN(sizeof(DrawInfo), "drawinfo");
	di->type= ICON_TYPE_VECTOR;
	di->data.vector.func =drawFunc;

	new_icon->drawinfo_free = 0;
	new_icon->drawinfo = di;

	BKE_icon_set(icon_id, new_icon);
}

/* Vector Icon Drawing Routines */

	/* Utilities */

static void viconutil_set_point(GLint pt[2], int x, int y)
{
	pt[0] = x;
	pt[1] = y;
}

static void viconutil_draw_tri(GLint (*pts)[2])
{
	glBegin(GL_TRIANGLES);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glVertex2iv(pts[2]);
	glEnd();
}

static void viconutil_draw_lineloop(GLint (*pts)[2], int numPoints)
{
	int i;

	glBegin(GL_LINE_LOOP);
	for (i=0; i<numPoints; i++) {
		glVertex2iv(pts[i]);
	}
	glEnd();
}

static void viconutil_draw_lineloop_smooth(GLint (*pts)[2], int numPoints)
{
	glEnable(GL_LINE_SMOOTH);
	viconutil_draw_lineloop(pts, numPoints);
	glDisable(GL_LINE_SMOOTH);
}

static void viconutil_draw_points(GLint (*pts)[2], int numPoints, int pointSize)
{
	int i;

	glBegin(GL_QUADS);
	for (i=0; i<numPoints; i++) {
		int x = pts[i][0], y = pts[i][1];

		glVertex2i(x-pointSize,y-pointSize);
		glVertex2i(x+pointSize,y-pointSize);
		glVertex2i(x+pointSize,y+pointSize);
		glVertex2i(x-pointSize,y+pointSize);
	}
	glEnd();
}

	/* Drawing functions */

static void vicon_x_draw(int x, int y, int w, int h, float alpha)
{
	x += 3;
	y += 3;
	w -= 6;
	h -= 6;

	glEnable( GL_LINE_SMOOTH );

	glLineWidth(2.5);
	
	glColor4f(0.0, 0.0, 0.0, alpha);
	glBegin(GL_LINES);
	glVertex2i(x  ,y  );
	glVertex2i(x+w,y+h);
	glVertex2i(x+w,y  );
	glVertex2i(x  ,y+h);
	glEnd();

	glLineWidth(1.0);
	
	glDisable( GL_LINE_SMOOTH );
}

static void vicon_view3d_draw(int x, int y, int w, int h, float alpha)
{
	int cx = x + w/2;
	int cy = y + h/2;
	int d = MAX2(2, h/3);

	glColor4f(0.5, 0.5, 0.5, alpha);
	glBegin(GL_LINES);
	glVertex2i(x  , cy-d);
	glVertex2i(x+w, cy-d);
	glVertex2i(x  , cy+d);
	glVertex2i(x+w, cy+d);

	glVertex2i(cx-d, y  );
	glVertex2i(cx-d, y+h);
	glVertex2i(cx+d, y  );
	glVertex2i(cx+d, y+h);
	glEnd();
	
	glColor4f(0.0, 0.0, 0.0, alpha);
	glBegin(GL_LINES);
	glVertex2i(x  , cy);
	glVertex2i(x+w, cy);
	glVertex2i(cx, y  );
	glVertex2i(cx, y+h);
	glEnd();
}

static void vicon_edit_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[4][2];

	viconutil_set_point(pts[0], x+3  , y+3  );
	viconutil_set_point(pts[1], x+w-3, y+3  );
	viconutil_set_point(pts[2], x+w-3, y+h-3);
	viconutil_set_point(pts[3], x+3  , y+h-3);

	glColor4f(0.0, 0.0, 0.0, alpha);
	viconutil_draw_lineloop(pts, 4);

	glColor3f(1, 1, 0.0);
	viconutil_draw_points(pts, 4, 1);
}

static void vicon_editmode_hlt_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[3][2];

	viconutil_set_point(pts[0], x+w/2, y+h-2);
	viconutil_set_point(pts[1], x+3, y+4);
	viconutil_set_point(pts[2], x+w-3, y+4);

	glColor4f(0.5, 0.5, 0.5, alpha);
	viconutil_draw_tri(pts);

	glColor4f(0.0, 0.0, 0.0, 1);
	viconutil_draw_lineloop_smooth(pts, 3);

	glColor3f(1, 1, 0.0);
	viconutil_draw_points(pts, 3, 1);
}

static void vicon_editmode_dehlt_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[3][2];

	viconutil_set_point(pts[0], x+w/2, y+h-2);
	viconutil_set_point(pts[1], x+3, y+4);
	viconutil_set_point(pts[2], x+w-3, y+4);

	glColor4f(0.0f, 0.0f, 0.0f, 1);
	viconutil_draw_lineloop_smooth(pts, 3);

	glColor3f(.9f, .9f, .9f);
	viconutil_draw_points(pts, 3, 1);
}

static void vicon_disclosure_tri_right_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[3][2];
	int cx = x+w/2;
	int cy = y+w/2;
	int d = w/3, d2 = w/5;

	viconutil_set_point(pts[0], cx-d2, cy+d);
	viconutil_set_point(pts[1], cx-d2, cy-d);
	viconutil_set_point(pts[2], cx+d2, cy);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glColor4f(0.8f, 0.8f, 0.8f, alpha);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glColor4f(0.3f, 0.3f, 0.3f, alpha);
	glVertex2iv(pts[2]);
	glEnd();
	glShadeModel(GL_FLAT);

	glColor4f(0.0f, 0.0f, 0.0f, 1);
	viconutil_draw_lineloop_smooth(pts, 3);
}

static void vicon_small_tri_right_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[3][2];
	int cx = x+w/2-4;
	int cy = y+w/2;
	int d = w/5, d2 = w/7;

	viconutil_set_point(pts[0], cx-d2, cy+d);
	viconutil_set_point(pts[1], cx-d2, cy-d);
	viconutil_set_point(pts[2], cx+d2, cy);

	glColor4f(0.2f, 0.2f, 0.2f, alpha);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glVertex2iv(pts[2]);
	glEnd();
	glShadeModel(GL_FLAT);
}

static void vicon_disclosure_tri_down_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[3][2];
	int cx = x+w/2;
	int cy = y+w/2;
	int d = w/3, d2 = w/5;

	viconutil_set_point(pts[0], cx+d, cy+d2);
	viconutil_set_point(pts[1], cx-d, cy+d2);
	viconutil_set_point(pts[2], cx, cy-d2);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glColor4f(0.8f, 0.8f, 0.8f, alpha);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glColor4f(0.3f, 0.3f, 0.3f, alpha);
	glVertex2iv(pts[2]);
	glEnd();
	glShadeModel(GL_FLAT);

	glColor4f(0.0f, 0.0f, 0.0f, 1);
	viconutil_draw_lineloop_smooth(pts, 3);
}

static void vicon_move_up_draw(int x, int y, int w, int h, float alpha)
{
	int d=-2;

	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1);
	glColor3f(0.0, 0.0, 0.0);

	glBegin(GL_LINE_STRIP);
	glVertex2i(x+w/2-d*2, y+h/2+d);
	glVertex2i(x+w/2, y+h/2-d + 1);
	glVertex2i(x+w/2+d*2, y+h/2+d);
	glEnd();

	glLineWidth(1.0);
	glDisable(GL_LINE_SMOOTH);
}

static void vicon_move_down_draw(int x, int y, int w, int h, float alpha)
{
	int d=2;

	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1);
	glColor3f(0.0, 0.0, 0.0);

	glBegin(GL_LINE_STRIP);
	glVertex2i(x+w/2-d*2, y+h/2+d);
	glVertex2i(x+w/2, y+h/2-d - 1);
	glVertex2i(x+w/2+d*2, y+h/2+d);
	glEnd();

	glLineWidth(1.0);
	glDisable(GL_LINE_SMOOTH);
}

static void init_internal_icons()
{
	bTheme *btheme= U.themes.first;
	ImBuf *bbuf= NULL;
	int x, y, icontype;
	char iconfilestr[FILE_MAXDIR+FILE_MAXFILE];
	char filenamestr[FILE_MAXFILE+16];	// 16 == strlen(".blender/icons/")+1
	
	if ((btheme!=NULL) && (strlen(btheme->tui.iconfile) > 0)) {
	
#ifdef WIN32
		sprintf(filenamestr, "icons/%s", btheme->tui.iconfile);
#else
		sprintf(filenamestr, ".blender/icons/%s", btheme->tui.iconfile);
#endif
		
		BLI_make_file_string("/", iconfilestr, BLI_gethome(), filenamestr);
		
		if (BLI_exists(iconfilestr)) {
			bbuf = IMB_loadiffname(iconfilestr, IB_rect);
			if(bbuf->x < ICON_IMAGE_W || bbuf->y < ICON_IMAGE_H) {
				if (G.f & G_DEBUG)
					printf("\n***WARNING***\nIcons file %s too small.\nUsing built-in Icons instead\n", iconfilestr);
				IMB_freeImBuf(bbuf);
				bbuf= NULL;
			}
		}
	}
	if(bbuf==NULL)
		bbuf = IMB_ibImageFromMemory((int *)datatoc_blenderbuttons, datatoc_blenderbuttons_size, IB_rect);

	if(bbuf) {
		/* free existing texture if any */
		if(icongltex.id) {
			glDeleteTextures(1, &icongltex.id);
			icongltex.id= 0;
		}

		/* we only use a texture for cards with non-power of two */
		if(GPU_non_power_of_two_support()) {
			glGenTextures(1, &icongltex.id);

			if(icongltex.id) {
				icongltex.w = bbuf->x;
				icongltex.h = bbuf->y;
				icongltex.invw = 1.0f/bbuf->x;
				icongltex.invh = 1.0f/bbuf->y;

				glBindTexture(GL_TEXTURE_2D, icongltex.id);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bbuf->x, bbuf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, bbuf->rect);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glBindTexture(GL_TEXTURE_2D, 0);

				if(glGetError() == GL_OUT_OF_MEMORY) {
					glDeleteTextures(1, &icongltex.id);
					icongltex.id= 0;
				}
			}
		}
	}

	if(icongltex.id)
		icontype= ICON_TYPE_TEXTURE;
	else
		icontype= ICON_TYPE_BUFFER;
	
	for (y=0; y<ICON_GRID_ROWS; y++) {
		for (x=0; x<ICON_GRID_COLS; x++) {
			def_internal_icon(bbuf, BIFICONID_FIRST + y*ICON_GRID_COLS + x,
				x*(ICON_GRID_W+ICON_GRID_MARGIN)+ICON_GRID_MARGIN,
				y*(ICON_GRID_H+ICON_GRID_MARGIN)+ICON_GRID_MARGIN, ICON_GRID_W,
				icontype);
		}
	}

	def_internal_vicon(VICO_VIEW3D_VEC, vicon_view3d_draw);
	def_internal_vicon(VICO_EDIT_VEC, vicon_edit_draw);
	def_internal_vicon(VICO_EDITMODE_DEHLT, vicon_editmode_dehlt_draw);
	def_internal_vicon(VICO_EDITMODE_HLT, vicon_editmode_hlt_draw);
	def_internal_vicon(VICO_DISCLOSURE_TRI_RIGHT_VEC, vicon_disclosure_tri_right_draw);
	def_internal_vicon(VICO_DISCLOSURE_TRI_DOWN_VEC, vicon_disclosure_tri_down_draw);
	def_internal_vicon(VICO_MOVE_UP_VEC, vicon_move_up_draw);
	def_internal_vicon(VICO_MOVE_DOWN_VEC, vicon_move_down_draw);
	def_internal_vicon(VICO_X_VEC, vicon_x_draw);
	def_internal_vicon(VICO_SMALL_TRI_RIGHT_VEC, vicon_small_tri_right_draw);

	IMB_freeImBuf(bbuf);
}


static void init_iconfile_list(struct ListBase *list)
{
	IconFile *ifile;
	ImBuf *bbuf= NULL;
	struct direntry *dir;
	int restoredir = 1; /* restore to current directory */
	int totfile, i, index=1;
	int ifilex, ifiley;
	char icondirstr[FILE_MAX];
	char iconfilestr[FILE_MAX+16]; /* allow 256 chars for file+dir */
	char olddir[FILE_MAX];
	
	list->first = list->last = NULL;

#ifdef WIN32
	BLI_make_file_string("/", icondirstr, BLI_gethome(), "icons");
#else
	BLI_make_file_string("/", icondirstr, BLI_gethome(), ".blender/icons");
#endif
	
	if(BLI_exists(icondirstr)==0)
		return;
	
	/* since BLI_getdir changes the current working directory, restore it 
	   back to old value afterwards */
	if(!BLI_getwdN(olddir)) 
		restoredir = 0;
	totfile = BLI_getdir(icondirstr, &dir);
	if (restoredir)
		chdir(olddir);

	for(i=0; i<totfile; i++) {
		if( (dir[i].type & S_IFREG) ) {
			char *filename = dir[i].relname;
			
			if(BLI_testextensie(filename, ".png")) {
			
				/* check to see if the image is the right size, continue if not */
				/* copying strings here should go ok, assuming that we never get back
				   a complete path to file longer than 256 chars */
				sprintf(iconfilestr, "%s/%s", icondirstr, filename);
				if(BLI_exists(iconfilestr)) bbuf = IMB_loadiffname(iconfilestr, IB_rect);
				
				ifilex = bbuf->x;
				ifiley = bbuf->y;
				IMB_freeImBuf(bbuf);
				
				if ((ifilex != ICON_IMAGE_W) || (ifiley != ICON_IMAGE_H))
					continue;
			
				/* found a potential icon file, so make an entry for it in the cache list */
				ifile = MEM_callocN(sizeof(IconFile), "IconFile");
				
				BLI_strncpy(ifile->filename, filename, sizeof(ifile->filename));
				ifile->index = index;

				BLI_addtail(list, ifile);
				
				index++;
			}
		}
	}
	
	/* free temporary direntry structure that's been created by BLI_getdir() */
	i= totfile-1;
	
	for(; i>=0; i--){
		MEM_freeN(dir[i].relname);
		MEM_freeN(dir[i].path);
		if (dir[i].string) MEM_freeN(dir[i].string);
	}
	free(dir);
	dir= 0;
}

static void free_iconfile_list(struct ListBase *list)
{
	IconFile *ifile=NULL, *next_ifile=NULL;
	
	for(ifile=list->first; ifile; ifile=next_ifile) {
		next_ifile = ifile->next;
		BLI_freelinkN(list, ifile);
	}
}

int UI_iconfile_get_index(char *filename)
{
	IconFile *ifile;
	ListBase *list=&(iconfilelist);
	
	for(ifile=list->first; ifile; ifile=ifile->next) {
		if ( BLI_streq(filename, ifile->filename)) {
			return ifile->index;
		}
	}
	
	return 0;
}

ListBase *UI_iconfile_list(void)
{
	ListBase *list=&(iconfilelist);
	
	return list;
}


void UI_icons_free()
{
	if(icongltex.id) {
		glDeleteTextures(1, &icongltex.id);
		icongltex.id= 0;
	}

	free_iconfile_list(&iconfilelist);
	BKE_icons_free();
}

void UI_icons_free_drawinfo(void *drawinfo)
{
	DrawInfo *di = drawinfo;

	if(di) {
		if(di->type == ICON_TYPE_BUFFER) {
			if(di->data.buffer.image) {
				MEM_freeN(di->data.buffer.image->rect);
				MEM_freeN(di->data.buffer.image);
			}
		}

		MEM_freeN(di);
	}
}

static DrawInfo *icon_create_drawinfo()
{
	DrawInfo *di = NULL;

	di = MEM_callocN(sizeof(DrawInfo), "di_icon");
	di->type= ICON_TYPE_PREVIEW;

	return di;
}

int UI_icon_get_width(int icon_id)
{
	Icon *icon = NULL;
	DrawInfo *di = NULL;

	icon = BKE_icon_get(icon_id);
	
	if (!icon) {
		if (G.f & G_DEBUG)
			printf("UI_icon_get_width: Internal error, no icon for icon ID: %d\n", icon_id);
		return 0;
	}
	
	di = (DrawInfo *)icon->drawinfo;
	if (!di) {
		di = icon_create_drawinfo();
		icon->drawinfo = di;
	}

	if (di)
		return ICON_DEFAULT_WIDTH;

	return 0;
}

int UI_icon_get_height(int icon_id)
{
	Icon *icon = NULL;
	DrawInfo *di = NULL;

	icon = BKE_icon_get(icon_id);
	
	if (!icon) {
		if (G.f & G_DEBUG)
			printf("UI_icon_get_height: Internal error, no icon for icon ID: %d\n", icon_id);
		return 0;
	}
	
	di = (DrawInfo*)icon->drawinfo;

	if (!di) {
		di = icon_create_drawinfo();
		icon->drawinfo = di;
	}
	
	if (di)
		return ICON_DEFAULT_HEIGHT;

	return 0;
}

void UI_icons_init(int first_dyn_id)
{
	init_iconfile_list(&iconfilelist);
	BKE_icons_init(first_dyn_id);
	init_internal_icons();
}

/* Render size for preview images at level miplevel */
static int preview_render_size(int miplevel)
{
	switch (miplevel) {
		case 0: return 32;
		case 1: return PREVIEW_DEFAULT_HEIGHT;
	}
	return 0;
}

static void icon_create_mipmap(struct PreviewImage* prv_img, int miplevel) 
{
	unsigned int size = preview_render_size(miplevel);

	if (!prv_img) {
		if (G.f & G_DEBUG)
			printf("Error: requested preview image does not exist");
	}
	if (!prv_img->rect[miplevel]) {
		prv_img->w[miplevel] = size;
		prv_img->h[miplevel] = size;
		prv_img->changed[miplevel] = 1;
		prv_img->rect[miplevel] = MEM_callocN(size*size*sizeof(unsigned int), "prv_rect"); 
	}
}

/* only called when icon has changed */
/* only call with valid pointer from UI_icon_draw */
static void icon_set_image(bContext *C, ID *id, PreviewImage* prv_img, int miplevel)
{
	if (!prv_img) {
		if (G.f & G_DEBUG)
			printf("No preview image for this ID: %s\n", id->name);
		return;
	}	

	/* create the preview rect */
	icon_create_mipmap(prv_img, miplevel);

	ED_preview_icon_job(C, prv_img, id, prv_img->rect[miplevel],
		prv_img->w[miplevel], prv_img->h[miplevel]);
}

static void icon_draw_rect(float x, float y, int w, int h, float aspect, int rw, int rh, unsigned int *rect, float alpha, float *rgb)
{
	/* modulate color */
	if(alpha != 1.0f)
		glPixelTransferf(GL_ALPHA_SCALE, alpha);

	if(rgb) {
		glPixelTransferf(GL_RED_SCALE, rgb[0]);
		glPixelTransferf(GL_GREEN_SCALE, rgb[1]);
		glPixelTransferf(GL_BLUE_SCALE, rgb[2]);
	}

	/* position */
	glRasterPos2f(x, y);
	// XXX ui_rasterpos_safe(x, y, aspect);
	
	/* draw */
	if((w<1 || h<1)) {
		// XXX - TODO 2.5 verify whether this case can happen
		if (G.f & G_DEBUG)
			printf("what the heck! - icons are %i x %i pixels?\n", w, h);
	}
	/* rect contains image in 'rendersize', we only scale if needed */
	else if(rw!=w && rh!=h) {
		if(w>2000 || h>2000) { /* something has gone wrong! */
			if (G.f & G_DEBUG)
				printf("insane icon size w=%d h=%d\n",w,h);
		}
		else {
			ImBuf *ima;

			/* first allocate imbuf for scaling and copy preview into it */
			ima = IMB_allocImBuf(rw, rh, 32, IB_rect, 0);
			memcpy(ima->rect, rect, rw*rh*sizeof(unsigned int));	
			
			/* scale it */
			IMB_scaleImBuf(ima, w, h);
			glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, ima->rect);
			
			IMB_freeImBuf(ima);
		}
	}
	else
		glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, rect);

	/* restore color */
	if(alpha != 0.0f)
		glPixelTransferf(GL_ALPHA_SCALE, 1.0f);
	
	if(rgb) {
		glPixelTransferf(GL_RED_SCALE, 1.0f);
		glPixelTransferf(GL_GREEN_SCALE, 1.0f);
		glPixelTransferf(GL_BLUE_SCALE, 1.0f);
	}
}

static void icon_draw_texture(float x, float y, float w, float h, int ix, int iy, int iw, int ih, float alpha, float *rgb)
{
	float x1, x2, y1, y2;

	if(rgb) glColor4f(rgb[0], rgb[1], rgb[2], alpha);
	else glColor4f(1.0f, 1.0f, 1.0f, alpha);

	x1= ix*icongltex.invw;
	x2= (ix + ih)*icongltex.invw;
	y1= iy*icongltex.invh;
	y2= (iy + ih)*icongltex.invh;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, icongltex.id);

	glBegin(GL_QUADS);
	glTexCoord2f(x1, y1);
	glVertex2f(x, y);

	glTexCoord2f(x2, y1);
	glVertex2f(x+w, y);

	glTexCoord2f(x2, y2);
	glVertex2f(x+w, y+h);

	glTexCoord2f(x1, y2);
	glVertex2f(x, y+h);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
}

/* Drawing size for preview images at level miplevel */
static int preview_size(int miplevel)
{
	switch (miplevel) {
		case 0: return ICON_DEFAULT_HEIGHT;
		case 1: return PREVIEW_DEFAULT_HEIGHT;
	}
	return 0;
}

static void icon_draw_size(float x, float y, int icon_id, float aspect, float alpha, float *rgb, int miplevel, int draw_size, int nocreate)
{
	Icon *icon = NULL;
	DrawInfo *di = NULL;
	IconImage *iimg;
	int w, h;
	
	icon = BKE_icon_get(icon_id);
	
	if (!icon) {
		if (G.f & G_DEBUG)
			printf("icon_draw_mipmap: Internal error, no icon for icon ID: %d\n", icon_id);
		return;
	}

	di = (DrawInfo*)icon->drawinfo;
	
	if (!di) {
		di = icon_create_drawinfo();
	
		icon->drawinfo = di;		
		icon->drawinfo_free = UI_icons_free_drawinfo;		
	}
	
	/* scale width and height according to aspect */
	w = (int)(draw_size/aspect + 0.5f);
	h = (int)(draw_size/aspect + 0.5f);
	
	if(di->type == ICON_TYPE_VECTOR) {
		/* vector icons use the uiBlock transformation, they are not drawn
		with untransformed coordinates like the other icons */
		di->data.vector.func(x, y, ICON_DEFAULT_HEIGHT, ICON_DEFAULT_HEIGHT, 1.0f); 
	} 
	else if(di->type == ICON_TYPE_TEXTURE) {
		icon_draw_texture(x, y, w, h, di->data.texture.x, di->data.texture.y,
			di->data.texture.w, di->data.texture.h, alpha, rgb);
	}
	else if(di->type == ICON_TYPE_BUFFER) {
		/* it is a builtin icon */		
		iimg= di->data.buffer.image;

		if(!iimg->rect) return; /* something has gone wrong! */

		icon_draw_rect(x, y, w, h, aspect, iimg->w, iimg->h, iimg->rect, alpha, rgb);
	}
	else if(di->type == ICON_TYPE_PREVIEW) {
		PreviewImage* pi = BKE_previewimg_get((ID*)icon->obj); 

		if(pi) {			
			/* no create icon on this level in code */
			if(!pi->rect[miplevel]) return; /* something has gone wrong! */
			
			/* preview images use premul alpha ... */
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			icon_draw_rect(x, y, w, h, aspect, pi->w[miplevel], pi->h[miplevel], pi->rect[miplevel], 1.0f, NULL);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
}

void ui_id_icon_render(bContext *C, ID *id, int preview)
{
	PreviewImage *pi = BKE_previewimg_get(id); 
		
	if (pi) {			
		if ((pi->changed[0] ||!pi->rect[0])) /* changed only ever set by dynamic icons */
		{
			/* create the preview rect if necessary */				
			
			icon_set_image(C, id, pi, 0);		/* icon size */
			if (preview)
				icon_set_image(C, id, pi, 1);	/* preview size */
			
			pi->changed[0] = 0;
		}
	}
}

int ui_id_icon_get(bContext *C, ID *id, int preview)
{
	int iconid= 0;
	
	/* icon */
	switch(GS(id->name))
	{
		case ID_MA: /* fall through */
		case ID_TE: /* fall through */
		case ID_IM: /* fall through */
		case ID_WO: /* fall through */
		case ID_LA: /* fall through */
			iconid= BKE_icon_getid(id);
			/* checks if not exists, or changed */
			ui_id_icon_render(C, id, preview);
			break;
		default:
			break;
	}

	return iconid;
}

static void icon_draw_mipmap(float x, float y, int icon_id, float aspect, float alpha, int miplevel, int nocreate)
{
	int draw_size = preview_size(miplevel);
	icon_draw_size(x, y, icon_id, aspect, alpha, NULL, miplevel, draw_size, nocreate);
}

void UI_icon_draw_aspect(float x, float y, int icon_id, float aspect, float alpha)
{
	icon_draw_mipmap(x, y, icon_id, aspect, alpha, PREVIEW_MIPMAP_ZERO, 0);
}

void UI_icon_draw_aspect_color(float x, float y, int icon_id, float aspect, float *rgb)
{
	int draw_size = preview_size(PREVIEW_MIPMAP_ZERO);
	icon_draw_size(x, y, icon_id, aspect, 1.0f, rgb, PREVIEW_MIPMAP_ZERO, draw_size, 0);
}

void UI_icon_draw(float x, float y, int icon_id)
{
	UI_icon_draw_aspect(x, y, icon_id, 1.0f, 1.0f);
}

void UI_icon_draw_size(float x, float y, int size, int icon_id, float alpha)
{
	icon_draw_size(x, y, icon_id, 1.0f, alpha, NULL, 0, size, 1);
}

void UI_icon_draw_preview(float x, float y, int icon_id)
{
	icon_draw_mipmap(x, y, icon_id, 1.0f, 1.0f, PREVIEW_MIPMAP_LARGE, 0);
}

void UI_icon_draw_preview_aspect(float x, float y, int icon_id, float aspect)
{
	icon_draw_mipmap(x, y, icon_id, aspect, 1.0f, PREVIEW_MIPMAP_LARGE, 0);
}

void UI_icon_draw_preview_aspect_size(float x, float y, int icon_id, float aspect, int size)
{
	icon_draw_size(x, y, icon_id, aspect, 1.0f, NULL, PREVIEW_MIPMAP_LARGE, size, 0);
}

