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
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_bmfont.h"
#include "BKE_image.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

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

void BL_MakeDrawable(wmWindowManager *wm, wmWindow *win)
{
	wm_window_make_drawable(wm, win);
}

void BL_SetSwapInterval(struct wmWindow *win, int interval)
{
	wm_window_set_swap_interval(win, interval);
}

int BL_GetSwapInterval(struct wmWindow *win)
{
	return wm_window_get_swap_interval(win);
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
void BL_MakeScreenShot(bScreen *screen, ScrArea *curarea, const char *filename)
{
	unsigned int *dumprect;
	int dumpsx, dumpsy;
	
	dumprect = screenshot(curarea, &dumpsx, &dumpsy);

	if (dumprect) {
		/* initialize image file format data */
		Scene *scene = (screen)? screen->scene: NULL;
		ImageFormatData im_format;

		if (scene)
			im_format = scene->r.im_format;
		else
			BKE_imformat_defaults(&im_format);

		/* create file path */
		char path[FILE_MAX];
		BLI_strncpy(path, filename, sizeof(path));
		BLI_path_abs(path, G.main->name);
		BKE_add_image_extension_from_type(path, im_format.imtype);

		/* create and save imbuf */
		ImBuf *ibuf = IMB_allocImBuf(dumpsx, dumpsy, 24, 0);
		ibuf->rect = dumprect;

		BKE_imbuf_write_as(ibuf, path, &im_format, false);

		ibuf->rect = NULL;
		IMB_freeImBuf(ibuf);
		MEM_freeN(dumprect);
	}
}

