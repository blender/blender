/**
 * $Id$
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

#include "KX_BlenderGL.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "BLF_api.h"
#ifdef __cplusplus
}
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include "BKE_bmfont.h"
#include "BKE_image.h"

extern "C" {
#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"
#include "wm_cursors.h"
#include "wm_window.h"
}

/* end of blender block */

/* was in drawmesh.c */
void spack(unsigned int ucol)
{
	char *cp= (char *)&ucol;
        
	glColor3ub(cp[3], cp[2], cp[1]);
}

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

	if(glIsEnabled(GL_BLEND)) glDisable(GL_BLEND);
	if(glIsEnabled(GL_ALPHA_TEST)) glDisable(GL_ALPHA_TEST);

	if(glIsEnabled(GL_LIGHTING)) {
		glDisable(GL_LIGHTING);
		glDisable(GL_COLOR_MATERIAL);
	}

	if(GLEW_ARB_multitexture) {
		for(int i=0; i<MAXTEX; i++) {
			glActiveTextureARB(GL_TEXTURE0_ARB+i);

			if(GLEW_ARB_texture_cube_map)
				if(glIsEnabled(GL_TEXTURE_CUBE_MAP_ARB))
					glDisable(GL_TEXTURE_CUBE_MAP_ARB);

			if(glIsEnabled(GL_TEXTURE_2D))
				glDisable(GL_TEXTURE_2D);
		}

		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if(GLEW_ARB_texture_cube_map)
			if(glIsEnabled(GL_TEXTURE_CUBE_MAP_ARB))
				glDisable(GL_TEXTURE_CUBE_MAP_ARB);

		if(glIsEnabled(GL_TEXTURE_2D))
			glDisable(GL_TEXTURE_2D);
	}
}

void BL_print_gamedebug_line(const char* text, int xco, int yco, int width, int height)
{	
	/* gl prepping */
	DisableForText();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glOrtho(0, width, 0, height, -100, 100);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* the actual drawing */
	glColor3ub(255, 255, 255);
	BLF_draw_default(xco, height-yco, 0.0f, (char *)text);

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

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	
	glOrtho(0, width, 0, height, -100, 100);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* draw in black first*/
	glColor3ub(0, 0, 0);
	BLF_draw_default(xco+2, height-yco-2, 0.0f, (char *)text);
	glColor3ub(255, 255, 255);
	BLF_draw_default(xco, height-yco, 0.0f, (char *)text);

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


void BL_MakeScreenShot(struct ARegion *ar, const char* filename)
{
	char copyfilename[MAX_FILE_LENGTH];
	strcpy(copyfilename,filename);

	// filename read - only
	
	  /* XXX will need to change at some point */
	//XXX BIF_screendump(0);

	// write+read filename
	//XXX write_screendump((char*) copyfilename);
}

