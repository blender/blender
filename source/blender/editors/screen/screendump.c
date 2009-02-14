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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 * Making screendumps.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"
#include "WM_api.h"

#include "ED_fileselect.h"
#include "ED_screen.h"

typedef struct ScreenshotData {
	unsigned int *dumprect;
	int dumpsx, dumpsy;
} ScreenshotData;

static int screenshot_exec(bContext *C, wmOperator *op)
{
	ScreenshotData *scd= op->customdata;
	
	if(scd && scd->dumprect) {
		Scene *scene= CTX_data_scene(C);
		ImBuf *ibuf;
		char filename[FILE_MAX];
	
		RNA_string_get(op->ptr, "filename", filename);
	
		strcpy(G.ima, filename);
		BLI_convertstringcode(filename, G.sce);
		
		/* BKE_add_image_extension() checks for if extension was already set */
		if(scene->r.scemode & R_EXTENSION) 
			if(strlen(filename)<FILE_MAXDIR+FILE_MAXFILE-5)
				BKE_add_image_extension(scene, filename, scene->r.imtype);
		
		ibuf= IMB_allocImBuf(scd->dumpsx, scd->dumpsy, 24, 0, 0);
		ibuf->rect= scd->dumprect;
		
		if(scene->r.planes == 8) IMB_cspace(ibuf, rgb_to_bw);
		
		BKE_write_ibuf(scene, ibuf, filename, scene->r.imtype, scene->r.subimtype, scene->r.quality);

		IMB_freeImBuf(ibuf);

		MEM_freeN(scd->dumprect);
		MEM_freeN(scd);
		op->customdata= NULL;
	}
	return OPERATOR_FINISHED;
}

/* get shot from frontbuffer */
static unsigned int *screenshot(bContext *C, int *dumpsx, int *dumpsy, int fscreen)
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *curarea= CTX_wm_area(C);
	int x=0, y=0;
	unsigned int *dumprect= NULL;
	
	if(fscreen) {	/* full screen */
		x= 0;
		y= 0;
		*dumpsx= win->sizex;
		*dumpsy= win->sizey;
	} 
	else {
		x= curarea->totrct.xmin;
		y= curarea->totrct.ymin;
		*dumpsx= curarea->totrct.xmax-x;
		*dumpsy= curarea->totrct.ymax-y;
	}

	if (*dumpsx && *dumpsy) {
		
		dumprect= MEM_mallocN(sizeof(int) * dumpsx[0] * dumpsy[0], "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, *dumpsx, *dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		glReadBuffer(GL_BACK);
	}

	return dumprect;
}


static int screenshot_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	unsigned int *dumprect;
	int dumpsx, dumpsy;
	
	dumprect= screenshot(C, &dumpsx, &dumpsy, RNA_boolean_get(op->ptr, "full"));
	if(dumprect) {
		SpaceFile *sfile;
		ScreenshotData *scd= MEM_callocN(sizeof(ScreenshotData), "screenshot");
		
		scd->dumpsx= dumpsx;
		scd->dumpsy= dumpsy;
		scd->dumprect= dumprect;
		op->customdata= scd;
		
		if(RNA_property_is_set(op->ptr, "filename"))
			return screenshot_exec(C, op);
		
		ED_screen_full_newspace(C, CTX_wm_area(C), SPACE_FILE);
		
		/* settings for filebrowser */
		sfile= (SpaceFile*)CTX_wm_space_data(C);
		sfile->op = op;
		
		ED_fileselect_set_params(sfile, FILE_BLENDER, "Save Screenshot As", G.ima, 0, 0, 0);
	
		return OPERATOR_RUNNING_MODAL;
	}	
	return OPERATOR_CANCELLED;
}


void SCREEN_OT_screenshot(wmOperatorType *ot)
{
	ot->name= "Make Screenshot";
	ot->idname= "SCREEN_OT_screenshot";
	
	ot->invoke= screenshot_invoke;
	ot->exec= screenshot_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	RNA_def_property(ot->srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_boolean(ot->srna, "full", 1, "Full Screen", "");
}

/* *************** screenshot movie job ************************* */
#if 0
typedef struct ScreenshotJob {
	unsigned int *dumprect;
	int dumpsx, dumpsy;
	short *stop;
	short *do_update;
} ScreenshotJob;


static void screenshot_freejob(void *sjv)
{
	ScreenshotJob *sj= sjv;
	
	MEM_freeN(sj);
}


/* called before redraw notifiers, copies a new dumprect */
static void screenshot_updatejob(void *sjv)
{
	ScreenshotJob *sj= sjv;
	
}


/* only this runs inside thread */
static void screenshot_startjob(void *sjv, short *stop, short *do_update)
{
	ScreenshotJob *sj= sjv;
	
	sj->stop= stop;
	sj->do_update= do_update;
	
	
}

static int screenshot_job_invoke(const bContext *C, wmOperator *op, wmEvent *event)
{
	bScreen *screen= CTX_wm_screen(C);
	wmJob *steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), screen);
	ScreenshotJob *sj= MEM_callocN(sizeof(ScreenshotJob), "screenshot job");
	
	/* customdata for preview thread */
	sj->scene= CTX_data_scene(C);
	
	/* setup job */
	WM_jobs_customdata(steve, sj, screenshot_freejob);
	WM_jobs_timer(steve, 0.1, 0, 0);
	WM_jobs_callbacks(steve, screenshot_startjob, NULL, screenshot_updatejob);
	
	WM_jobs_start(steve);
	
}

void SCREEN_OT_screenshot_movie(wmOperatorType *ot)
{
	ot->name= "Make Screenshot";
	ot->idname= "SCREEN_OT_screenshot_movie";
	
	ot->invoke= screenshot_invoke;
	ot->exec= screenshot_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	RNA_def_property(ot->srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_boolean(ot->srna, "full", 1, "Full Screen", "");
}


#endif

