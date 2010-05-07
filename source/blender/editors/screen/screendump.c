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
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"
#include "BKE_writeavi.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"
#include "WM_api.h"

#include "PIL_time.h"

#include "ED_screen_types.h"

#include "screen_intern.h"

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
		char path[FILE_MAX];
	
		RNA_string_get(op->ptr, "path", path);
	
		strcpy(G.ima, path);
		BLI_path_abs(path, G.sce);
		
		/* BKE_add_image_extension() checks for if extension was already set */
		if(scene->r.scemode & R_EXTENSION) 
			if(strlen(path)<FILE_MAXDIR+FILE_MAXFILE-5)
				BKE_add_image_extension(path, scene->r.imtype);
		
		ibuf= IMB_allocImBuf(scd->dumpsx, scd->dumpsy, 24, 0, 0);
		ibuf->rect= scd->dumprect;
		
		BKE_write_ibuf(scene, ibuf, path, scene->r.imtype, scene->r.subimtype, scene->r.quality);

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
		
		dumprect= MEM_mallocN(sizeof(int) * (*dumpsx) * (*dumpsy), "dumprect");
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
		ScreenshotData *scd= MEM_callocN(sizeof(ScreenshotData), "screenshot");
		
		scd->dumpsx= dumpsx;
		scd->dumpsy= dumpsy;
		scd->dumprect= dumprect;
		op->customdata= scd;
		
		if(RNA_property_is_set(op->ptr, "path"))
			return screenshot_exec(C, op);
		
		RNA_string_set(op->ptr, "path", G.ima);
		
		WM_event_add_fileselect(C, op);
	
		return OPERATOR_RUNNING_MODAL;
	}	
	return OPERATOR_CANCELLED;
}


void SCREEN_OT_screenshot(wmOperatorType *ot)
{
	ot->name= "Save Screenshot"; /* weak: opname starting with 'save' makes filewindow give save-over */
	ot->idname= "SCREEN_OT_screenshot";
	
	ot->invoke= screenshot_invoke;
	ot->exec= screenshot_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE, FILE_SPECIAL, FILE_SAVE);
	RNA_def_boolean(ot->srna, "full", 1, "Full Screen", "");
}

/* *************** screenshot movie job ************************* */

typedef struct ScreenshotJob {
	Scene *scene;
	unsigned int *dumprect;
	int x, y, dumpsx, dumpsy;
	short *stop;
	short *do_update;
	ReportList reports;
} ScreenshotJob;


static void screenshot_freejob(void *sjv)
{
	ScreenshotJob *sj= sjv;
	
	if(sj->dumprect)
		MEM_freeN(sj->dumprect);
	
	MEM_freeN(sj);
}


/* called before redraw notifiers, copies a new dumprect */
static void screenshot_updatejob(void *sjv)
{
	ScreenshotJob *sj= sjv;
	unsigned int *dumprect;
	
	if(sj->dumprect==NULL) {
		dumprect= MEM_mallocN(sizeof(int) * sj->dumpsx * sj->dumpsy, "dumprect");
		glReadPixels(sj->x, sj->y, sj->dumpsx, sj->dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		
		sj->dumprect= dumprect;
	}
}


/* only this runs inside thread */
static void screenshot_startjob(void *sjv, short *stop, short *do_update)
{
	ScreenshotJob *sj= sjv;
	RenderData rd= sj->scene->r;
	bMovieHandle *mh= BKE_get_movie_handle(sj->scene->r.imtype);
	int cfra= 1;
	
	/* we need this as local variables for renderdata */
	rd.frs_sec= U.scrcastfps;
	rd.frs_sec_base= 1.0f;
	
	if(BKE_imtype_is_movie(rd.imtype)) {
		if(!mh->start_movie(sj->scene, &rd, sj->dumpsx, sj->dumpsy, &sj->reports)) {
			printf("screencast job stopped\n");
			return;
		}
	}
	else
		mh= NULL;
	
	sj->stop= stop;
	sj->do_update= do_update;
	
	*do_update= 1; // wait for opengl rect
	
	while(*stop==0) {
		
		if(sj->dumprect) {
			
			if(mh) {
				if(mh->append_movie(&rd, cfra, (int *)sj->dumprect, sj->dumpsx, sj->dumpsy, &sj->reports)) {
					BKE_reportf(&sj->reports, RPT_INFO, "Appended frame: %d", cfra);
					printf("Appended frame %d\n", cfra);
				} else
					break;
			}
			else {
				ImBuf *ibuf= IMB_allocImBuf(sj->dumpsx, sj->dumpsy, rd.planes, 0, 0);
				char name[FILE_MAXDIR+FILE_MAXFILE];
				int ok;
				
				BKE_makepicstring(name, rd.pic, cfra, rd.imtype, rd.scemode & R_EXTENSION);
				
				ibuf->rect= sj->dumprect;
				ok= BKE_write_ibuf(sj->scene, ibuf, name, rd.imtype, rd.subimtype, rd.quality);
				
				if(ok==0) {
					printf("Write error: cannot save %s\n", name);
					BKE_reportf(&sj->reports, RPT_INFO, "Write error: cannot save %s\n", name);
					break;
				}
				else {
					printf("Saved file: %s\n", name);
					BKE_reportf(&sj->reports, RPT_INFO, "Saved file: %s", name);
				}
				
				/* imbuf knows which rects are not part of ibuf */
				IMB_freeImBuf(ibuf);	
			}
			
			MEM_freeN(sj->dumprect);
			sj->dumprect= NULL;
			
			*do_update= 1;
			
			cfra++;

		}
		else 
			PIL_sleep_ms(U.scrcastwait);
	}
	
	if(mh)
		mh->end_movie();

	BKE_report(&sj->reports, RPT_INFO, "Screencast job stopped");
}

static int screencast_exec(bContext *C, wmOperator *op)
{
	bScreen *screen= CTX_wm_screen(C);
	wmJob *steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), screen, 0);
	ScreenshotJob *sj= MEM_callocN(sizeof(ScreenshotJob), "screenshot job");

	/* setup sj */
	if(RNA_boolean_get(op->ptr, "full")) {
		wmWindow *win= CTX_wm_window(C);
		sj->x= 0;
		sj->y= 0;
		sj->dumpsx= win->sizex;
		sj->dumpsy= win->sizey;
	} 
	else {
		ScrArea *curarea= CTX_wm_area(C);
		sj->x= curarea->totrct.xmin;
		sj->y= curarea->totrct.ymin;
		sj->dumpsx= curarea->totrct.xmax - sj->x;
		sj->dumpsy= curarea->totrct.ymax - sj->y;
	}
	sj->scene= CTX_data_scene(C);

	BKE_reports_init(&sj->reports, RPT_PRINT);

	/* setup job */
	WM_jobs_customdata(steve, sj, screenshot_freejob);
	WM_jobs_timer(steve, 0.1, 0, NC_SCREEN|ND_SCREENCAST);
	WM_jobs_callbacks(steve, screenshot_startjob, NULL, screenshot_updatejob, NULL);
	
	WM_jobs_start(CTX_wm_manager(C), steve);
	
	WM_event_add_notifier(C, NC_SCREEN|ND_SCREENCAST, screen);
	
	return OPERATOR_FINISHED;
}

void SCREEN_OT_screencast(wmOperatorType *ot)
{
	ot->name= "Make Screencast";
	ot->idname= "SCREEN_OT_screencast";
	
	ot->invoke= WM_operator_confirm;
	ot->exec= screencast_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	RNA_def_property(ot->srna, "path", PROP_STRING, PROP_FILEPATH);
	RNA_def_boolean(ot->srna, "full", 1, "Full Screen", "");
}



