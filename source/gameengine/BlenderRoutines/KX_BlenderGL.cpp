/*
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/BlenderRoutines/KX_BlenderGL.cpp
 *  \ingroup blroutines
 */


#include "KX_BlenderGL.h"

/* 
 * This little block needed for linking to Blender... 
 */
#ifdef WIN32
#include <vector>
#include "BLI_winstuff.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "BL_Material.h" // MAXTEX

/* Data types encoding the game world: */
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_camera_types.h"
#include "DNA_world_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_image_types.h"
#include "DNA_view3d_types.h"
#include "DNA_material_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_bmfont.h"
#include "BKE_image.h"

#include "BLI_path_util.h"

extern "C" {
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"
#include "wm_cursors.h"
#include "wm_window.h"
#include "BLF_api.h"
}

/* end of blender block */
void BL_warp_pointer(wmWindow *win, int x,int y)
{
	WM_cursor_warp(win, x, y);
}

void BL_SwapBuffers(wmWindow *win)
{
	wm_window_swap_buffers(win);
}

void DisableForText()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); /* needed for texture fonts otherwise they render as wireframe */

	if (glIsEnabled(GL_BLEND)) glDisable(GL_BLEND);
	if (glIsEnabled(GL_ALPHA_TEST)) glDisable(GL_ALPHA_TEST);

	if (glIsEnabled(GL_LIGHTING)) {
		glDisable(GL_LIGHTING);
		glDisable(GL_COLOR_MATERIAL);
	}

	if (GLEW_ARB_multitexture) {
		for (int i=0; i<MAXTEX; i++) {
			glActiveTextureARB(GL_TEXTURE0_ARB+i);

			if (GLEW_ARB_texture_cube_map)
				if (glIsEnabled(GL_TEXTURE_CUBE_MAP_ARB))
					glDisable(GL_TEXTURE_CUBE_MAP_ARB);

			if (glIsEnabled(GL_TEXTURE_2D))
				glDisable(GL_TEXTURE_2D);
		}

		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if (GLEW_ARB_texture_cube_map)
			if (glIsEnabled(GL_TEXTURE_CUBE_MAP_ARB))
				glDisable(GL_TEXTURE_CUBE_MAP_ARB);

		if (glIsEnabled(GL_TEXTURE_2D))
			glDisable(GL_TEXTURE_2D);
	}
}

/* Print 3D text */
void BL_print_game_line(int fontid, const char* text, int size, int dpi, float* color, double* mat, float aspect)
{
	/* gl prepping */
	DisableForText();

	/* the actual drawing */
	glColor4fv(color);

	/* multiply the text matrix by the object matrix */
	BLF_enable(fontid, BLF_MATRIX|BLF_ASPECT);
	BLF_matrix(fontid, mat);

	/* aspect is the inverse scale that allows you to increase */
	/* your resolution without sizing the final text size      */
	/* the bigger the size, the smaller the aspect	           */
	BLF_aspect(fontid, aspect, aspect, aspect);

	BLF_size(fontid, size, dpi);
	BLF_position(fontid, 0, 0, 0);
	BLF_draw(fontid, (char *)text, 65535);

	BLF_disable(fontid, BLF_MATRIX|BLF_ASPECT);
}

void BL_print_gamedebug_line(const char* text, int xco, int yco, int width, int height)
{	
	/* gl prepping */
	DisableForText();
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glOrtho(0, width, 0, height, -100, 100);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* the actual drawing */
	glColor3ub(255, 255, 255);
	BLF_draw_default((float)xco, (float)(height-yco), 0.0f, (char *)text, 65535); /* XXX, use real len */

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void BL_print_gamedebug_line_padded(const char* text, int xco, int yco, int width, int height)
{
	/* This is a rather important line :( The gl-mode hasn't been left
	 * behind quite as neatly as we'd have wanted to. I don't know
	 * what cause it, though :/ .*/
	DisableForText();
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	
	glOrtho(0, width, 0, height, -100, 100);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* draw in black first*/
	glColor3ub(0, 0, 0);
	BLF_draw_default((float)(xco+2), (float)(height-yco-2), 0.0f, text, 65535); /* XXX, use real len */
	glColor3ub(255, 255, 255);
	BLF_draw_default((float)xco, (float)(height-yco), 0.0f, text, 65535); /* XXX, use real len */

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void BL_HideMouse(wmWindow *win)
{
	WM_cursor_set(win, CURSOR_NONE);
}


void BL_WaitMouse(wmWindow *win)
{
	WM_cursor_set(win, CURSOR_WAIT);
}


void BL_NormalMouse(wmWindow *win)
{
	WM_cursor_set(win, CURSOR_STD);
}
#define MAX_FILE_LENGTH 512

/* get shot from frontbuffer sort of a copy from screendump.c */
static unsigned int *screenshot(ScrArea *curarea, int *dumpsx, int *dumpsy)
{
	int x=0, y=0;
	unsigned int *dumprect= NULL;
	
	x= curarea->totrct.xmin;
	y= curarea->totrct.ymin;
	*dumpsx= curarea->totrct.xmax-x;
	*dumpsy= curarea->totrct.ymax-y;

	if (*dumpsx && *dumpsy) {
		
		dumprect= (unsigned int *)MEM_mallocN(sizeof(int) * (*dumpsx) * (*dumpsy), "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, *dumpsx, *dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		glReadBuffer(GL_BACK);
	}

	return dumprect;
}

/* based on screendump.c::screenshot_exec */
void BL_MakeScreenShot(ScrArea *curarea, const char* filename)
{
	char path[MAX_FILE_LENGTH];
	strcpy(path,filename);

	unsigned int *dumprect;
	int dumpsx, dumpsy;
	
	dumprect= screenshot(curarea, &dumpsx, &dumpsy);
	if (dumprect) {
		ImBuf *ibuf;
		BLI_path_abs(path, G.main->name);
		/* BKE_add_image_extension() checks for if extension was already set */
		BKE_add_image_extension(path, R_IMF_IMTYPE_PNG); /* scene->r.im_format.imtype */
		ibuf= IMB_allocImBuf(dumpsx, dumpsy, 24, 0);
		ibuf->rect= dumprect;
		ibuf->ftype= PNG;

		IMB_saveiff(ibuf, path, IB_rect);

		ibuf->rect= NULL;
		IMB_freeImBuf(ibuf);
		MEM_freeN(dumprect);
	}
}

