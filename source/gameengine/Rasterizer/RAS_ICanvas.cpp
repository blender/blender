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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Rasterizer/RAS_ICanvas.cpp
 *  \ingroup bgerast
 */

#include "RAS_ICanvas.h"
#include "DNA_scene_types.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BLI_task.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}


// Task data for saving screenshots in a different thread.
struct ScreenshotTaskData
{
	unsigned int *dumprect;
	int dumpsx;
	int dumpsy;
	char *path;
	ImageFormatData *im_format;
};

/**
 * Function that actually performs the image compression and saving to disk of a screenshot.
 * Run in a separate thread by RAS_ICanvas::save_screenshot().
 *
 * @param taskdata Must point to a ScreenshotTaskData object. This function takes ownership
 *                 of all pointers in the ScreenshotTaskData, and frees them.
 */
void save_screenshot_thread_func(TaskPool *__restrict pool, void *taskdata, int threadid);


RAS_ICanvas::RAS_ICanvas()
{
	m_taskscheduler = BLI_task_scheduler_create(TASK_SCHEDULER_AUTO_THREADS);
	m_taskpool = BLI_task_pool_create(m_taskscheduler, NULL);
}

RAS_ICanvas::~RAS_ICanvas()
{
	if (m_taskpool) {
		BLI_task_pool_work_and_wait(m_taskpool);
		BLI_task_pool_free(m_taskpool);
		m_taskpool = NULL;
	}

	if (m_taskscheduler) {
		BLI_task_scheduler_free(m_taskscheduler);
		m_taskscheduler = NULL;
	}
}


void save_screenshot_thread_func(TaskPool *__restrict UNUSED(pool), void *taskdata, int UNUSED(threadid))
{
	ScreenshotTaskData *task = static_cast<ScreenshotTaskData *>(taskdata);

	/* create and save imbuf */
	ImBuf *ibuf = IMB_allocImBuf(task->dumpsx, task->dumpsy, 24, 0);
	ibuf->rect = task->dumprect;

	BKE_imbuf_write_as(ibuf, task->path, task->im_format, false);

	ibuf->rect = NULL;
	IMB_freeImBuf(ibuf);
	MEM_freeN(task->dumprect);
	MEM_freeN(task->path);
	MEM_freeN(task->im_format);
}


void RAS_ICanvas::save_screenshot(const char *filename, int dumpsx, int dumpsy, unsigned int *dumprect,
                                  ImageFormatData * im_format)
{
	/* create file path */
	char *path = (char *)MEM_mallocN(FILE_MAX, "screenshot-path");
	BLI_strncpy(path, filename, FILE_MAX);
	BLI_path_abs(path, G.main->name);
	BLI_path_frame(path, m_frame, 0);
	m_frame++;
	BKE_image_path_ensure_ext_from_imtype(path, im_format->imtype);

	/* Save the actual file in a different thread, so that the
	 * game engine can keep running at full speed. */
	ScreenshotTaskData *task = (ScreenshotTaskData *)MEM_mallocN(sizeof(ScreenshotTaskData), "screenshot-data");
	task->dumprect = dumprect;
	task->dumpsx = dumpsx;
	task->dumpsy = dumpsy;
	task->path = path;
	task->im_format = im_format;

	BLI_task_pool_push(m_taskpool,
	                   save_screenshot_thread_func,
	                   task,
	                   true, // free task data
	                   TASK_PRIORITY_LOW);
}
