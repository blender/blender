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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 * Making screendumps.
 */

/** \file blender/editors/screen/screendump.c
 *  \ingroup edscr
 */


#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_image.h"
#include "BKE_report.h"
#include "BKE_writeavi.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "WM_types.h"
#include "WM_api.h"

#include "PIL_time.h"

#include "ED_screen_types.h"

#include "screen_intern.h"

typedef struct ScreenshotData {
	unsigned int *dumprect;
	int dumpsx, dumpsy;
	rcti crop;

	ImageFormatData im_format;
} ScreenshotData;

/* get shot from frontbuffer */
static unsigned int *screenshot(bContext *C, int *dumpsx, int *dumpsy)
{
	wmWindow *win= CTX_wm_window(C);
	int x=0, y=0;
	unsigned int *dumprect= NULL;
	
	x= 0;
	y= 0;
	*dumpsx= win->sizex;
	*dumpsy= win->sizey;

	if (*dumpsx && *dumpsy) {
		
		dumprect= MEM_mallocN(sizeof(int) * (*dumpsx) * (*dumpsy), "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, *dumpsx, *dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		glReadBuffer(GL_BACK);
	}

	return dumprect;
}

/* call from both exec and invoke */
static int screenshot_data_create(bContext *C, wmOperator *op)
{
	unsigned int *dumprect;
	int dumpsx, dumpsy;

	/* do redraw so we don't show popups/menus */
	WM_redraw_windows(C);
	
	dumprect= screenshot(C, &dumpsx, &dumpsy);

	if (dumprect) {
		ScreenshotData *scd= MEM_callocN(sizeof(ScreenshotData), "screenshot");
		ScrArea *sa= CTX_wm_area(C);
		
		scd->dumpsx= dumpsx;
		scd->dumpsy= dumpsy;
		scd->dumprect= dumprect;
		if (sa) {
			scd->crop= sa->totrct;
		}

		BKE_imformat_defaults(&scd->im_format);

		op->customdata = scd;

		return TRUE;
	}
	else {
		op->customdata= NULL;
		return FALSE;
	}
}

static void screenshot_data_free(wmOperator *op)
{
	ScreenshotData *scd= op->customdata;

	if (scd) {
		if (scd->dumprect)
			MEM_freeN(scd->dumprect);
		MEM_freeN(scd);
		op->customdata= NULL;
	}
}

static void screenshot_crop(ImBuf *ibuf, rcti crop)
{
	unsigned int *to= ibuf->rect;
	unsigned int *from= ibuf->rect + crop.ymin*ibuf->x + crop.xmin;
	int y, cropw= crop.xmax - crop.xmin, croph = crop.ymax - crop.ymin;

	if (cropw > 0 && croph > 0) {
		for (y=0; y<croph; y++, to+=cropw, from+=ibuf->x)
			memmove(to, from, sizeof(unsigned int)*cropw);

		ibuf->x= cropw;
		ibuf->y= croph;
	}
}

static int screenshot_exec(bContext *C, wmOperator *op)
{
	ScreenshotData *scd= op->customdata;

	if (scd == NULL) {
		/* when running exec directly */
		screenshot_data_create(C, op);
		scd= op->customdata;
	}

	if (scd) {
		if (scd->dumprect) {
			ImBuf *ibuf;
			char path[FILE_MAX];

			RNA_string_get(op->ptr, "filepath", path);
			BLI_path_abs(path, G.main->name);

			/* operator ensures the extension */
			ibuf= IMB_allocImBuf(scd->dumpsx, scd->dumpsy, 24, 0);
			ibuf->rect= scd->dumprect;

			/* crop to show only single editor */
			if (!RNA_boolean_get(op->ptr, "full"))
				screenshot_crop(ibuf, scd->crop);

			if (scd->im_format.planes == R_IMF_PLANES_BW) {
				/* bw screenshot? - users will notice if it fails! */
				IMB_color_to_bw(ibuf);
			}
			BKE_imbuf_write(ibuf, path, &scd->im_format);

			IMB_freeImBuf(ibuf);
		}
	}

	screenshot_data_free(op);
	return OPERATOR_FINISHED;
}

static int screenshot_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	if (screenshot_data_create(C, op)) {
		if (RNA_struct_property_is_set(op->ptr, "filepath"))
			return screenshot_exec(C, op);

		/* extension is added by 'screenshot_check' after */
		RNA_string_set(op->ptr, "filepath", G.relbase_valid ? G.main->name : "//screen");
		
		WM_event_add_fileselect(C, op);
	
		return OPERATOR_RUNNING_MODAL;
	}	
	return OPERATOR_CANCELLED;
}

static int screenshot_check(bContext *UNUSED(C), wmOperator *op)
{
	ScreenshotData *scd = op->customdata;
	return WM_operator_filesel_ensure_ext_imtype(op, scd->im_format.imtype);
}

static int screenshot_cancel(bContext *UNUSED(C), wmOperator *op)
{
	screenshot_data_free(op);
	return OPERATOR_CANCELLED;
}

static int screenshot_draw_check_prop(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);

	return !(strcmp(prop_id, "filepath") == 0);
}

static void screenshot_draw(bContext *UNUSED(C), wmOperator *op)
{
	uiLayout *layout = op->layout;
	ScreenshotData *scd = op->customdata;
	PointerRNA ptr;

	/* image template */
	RNA_pointer_create(NULL, &RNA_ImageFormatSettings, &scd->im_format, &ptr);
	uiTemplateImageSettings(layout, &ptr);

	/* main draw call */
	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiDefAutoButsRNA(layout, &ptr, screenshot_draw_check_prop, '\0');
}


void SCREEN_OT_screenshot(wmOperatorType *ot)
{
	ot->name = "Save Screenshot"; /* weak: opname starting with 'save' makes filewindow give save-over */
	ot->idname = "SCREEN_OT_screenshot";
	ot->description = "Capture a picture of the active area or whole Blender window";
	
	ot->invoke = screenshot_invoke;
	ot->check = screenshot_check;
	ot->exec = screenshot_exec;
	ot->cancel = screenshot_cancel;
	ot->ui = screenshot_draw;
	ot->poll = WM_operator_winactive;
	
	ot->flag = 0;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE, FILE_SPECIAL, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
	RNA_def_boolean(ot->srna, "full", 1, "Full Screen", "Screenshot the whole Blender window");
}

/* *************** screenshot movie job ************************* */

typedef struct ScreenshotJob {
	Main *bmain;
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
	
	if (sj->dumprect)
		MEM_freeN(sj->dumprect);
	
	MEM_freeN(sj);
}


/* called before redraw notifiers, copies a new dumprect */
static void screenshot_updatejob(void *sjv)
{
	ScreenshotJob *sj= sjv;
	unsigned int *dumprect;
	
	if (sj->dumprect==NULL) {
		dumprect= MEM_mallocN(sizeof(int) * sj->dumpsx * sj->dumpsy, "dumprect");
		glReadPixels(sj->x, sj->y, sj->dumpsx, sj->dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		
		sj->dumprect= dumprect;
	}
}


/* only this runs inside thread */
static void screenshot_startjob(void *sjv, short *stop, short *do_update, float *UNUSED(progress))
{
	ScreenshotJob *sj= sjv;
	RenderData rd= sj->scene->r;
	bMovieHandle *mh= BKE_movie_handle_get(sj->scene->r.im_format.imtype);
	
	/* we need this as local variables for renderdata */
	rd.frs_sec= U.scrcastfps;
	rd.frs_sec_base= 1.0f;
	
	if (BKE_imtype_is_movie(rd.im_format.imtype)) {
		if (!mh->start_movie(sj->scene, &rd, sj->dumpsx, sj->dumpsy, &sj->reports)) {
			printf("screencast job stopped\n");
			return;
		}
	}
	else
		mh= NULL;
	
	sj->stop= stop;
	sj->do_update= do_update;
	
	*do_update= 1; // wait for opengl rect
	
	while (*stop==0) {
		
		if (sj->dumprect) {
			
			if (mh) {
				if (mh->append_movie(&rd, rd.sfra, rd.cfra, (int *)sj->dumprect,
				                    sj->dumpsx, sj->dumpsy, &sj->reports))
				{
					BKE_reportf(&sj->reports, RPT_INFO, "Appended frame: %d", rd.cfra);
					printf("Appended frame %d\n", rd.cfra);
				}
				else {
					break;
				}
			}
			else {
				ImBuf *ibuf= IMB_allocImBuf(sj->dumpsx, sj->dumpsy, rd.im_format.planes, 0);
				char name[FILE_MAX];
				int ok;
				
				BKE_makepicstring(name, rd.pic, sj->bmain->name, rd.cfra, rd.im_format.imtype, rd.scemode & R_EXTENSION, TRUE);
				
				ibuf->rect= sj->dumprect;
				ok= BKE_imbuf_write(ibuf, name, &rd.im_format);
				
				if (ok==0) {
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
			
			rd.cfra++;

		}
		else 
			PIL_sleep_ms(U.scrcastwait);
	}
	
	if (mh)
		mh->end_movie();

	BKE_report(&sj->reports, RPT_INFO, "Screencast job stopped");
}

static int screencast_exec(bContext *C, wmOperator *op)
{
	bScreen *screen= CTX_wm_screen(C);
	wmJob *steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), screen, "Screencast", 0);
	ScreenshotJob *sj= MEM_callocN(sizeof(ScreenshotJob), "screenshot job");

	/* setup sj */
	if (RNA_boolean_get(op->ptr, "full")) {
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
	sj->bmain= CTX_data_main(C);
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
	ot->name = "Make Screencast";
	ot->idname = "SCREEN_OT_screencast";
	ot->description = "Capture a video of the active area or whole Blender window";
	
	ot->invoke = WM_operator_confirm;
	ot->exec = screencast_exec;
	ot->poll = WM_operator_winactive;
	
	ot->flag = 0;
	
	RNA_def_property(ot->srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_boolean(ot->srna, "full", 1, "Full Screen", "Screencast the whole Blender window");
}



