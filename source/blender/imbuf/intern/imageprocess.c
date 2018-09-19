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

/** \file blender/imbuf/intern/imageprocess.c
 *  \ingroup imbuf
 *
 * This file was moved here from the src/ directory. It is meant to
 * deal with endianness. It resided in a general blending lib. The
 * other functions were only used during rendering. This single
 * function remained. It should probably move to imbuf/intern/util.c,
 * but we'll keep it here for the time being. (nzc)
 *
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_math.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include <math.h>

/* Only this one is used liberally here, and in imbuf */
void IMB_convert_rgba_to_abgr(struct ImBuf *ibuf)
{
	size_t size;
	unsigned char rt, *cp = (unsigned char *)ibuf->rect;
	float rtf, *cpf = ibuf->rect_float;

	if (ibuf->rect) {
		size = ibuf->x * ibuf->y;

		while (size-- > 0) {
			rt = cp[0];
			cp[0] = cp[3];
			cp[3] = rt;
			rt = cp[1];
			cp[1] = cp[2];
			cp[2] = rt;
			cp += 4;
		}
	}

	if (ibuf->rect_float) {
		size = ibuf->x * ibuf->y;

		while (size-- > 0) {
			rtf = cpf[0];
			cpf[0] = cpf[3];
			cpf[3] = rtf;
			rtf = cpf[1];
			cpf[1] = cpf[2];
			cpf[2] = rtf;
			cpf += 4;
		}
	}
}
static void pixel_from_buffer(struct ImBuf *ibuf, unsigned char **outI, float **outF, int x, int y)

{
	size_t offset = ((size_t)ibuf->x) * y * 4 + 4 * x;

	if (ibuf->rect)
		*outI = (unsigned char *)ibuf->rect + offset;

	if (ibuf->rect_float)
		*outF = ibuf->rect_float + offset;
}

/* BICUBIC Interpolation */

void bicubic_interpolation_color(struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
	if (outF) {
		BLI_bicubic_interpolation_fl(in->rect_float, outF, in->x, in->y, 4, u, v);
	}
	else {
		BLI_bicubic_interpolation_char((unsigned char *) in->rect, outI, in->x, in->y, 4, u, v);
	}
}


void bicubic_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
	unsigned char *outI = NULL;
	float *outF = NULL;

	if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) {
		return;
	}

	pixel_from_buffer(out, &outI, &outF, xout, yout); /* gcc warns these could be uninitialized, but its ok */

	bicubic_interpolation_color(in, outI, outF, u, v);
}

/* BILINEAR INTERPOLATION */
void bilinear_interpolation_color(struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
	if (outF) {
		BLI_bilinear_interpolation_fl(in->rect_float, outF, in->x, in->y, 4, u, v);
	}
	else {
		BLI_bilinear_interpolation_char((unsigned char *) in->rect, outI, in->x, in->y, 4, u, v);
	}
}

/* function assumes out to be zero'ed, only does RGBA */
/* BILINEAR INTERPOLATION */

/* Note about wrapping, the u/v still needs to be within the image bounds,
 * just the interpolation is wrapped.
 * This the same as bilinear_interpolation_color except it wraps rather than using empty and emptyI */
void bilinear_interpolation_color_wrap(struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
	float *row1, *row2, *row3, *row4, a, b;
	unsigned char *row1I, *row2I, *row3I, *row4I;
	float a_b, ma_b, a_mb, ma_mb;
	int y1, y2, x1, x2;


	/* ImBuf in must have a valid rect or rect_float, assume this is already checked */

	x1 = (int)floor(u);
	x2 = (int)ceil(u);
	y1 = (int)floor(v);
	y2 = (int)ceil(v);

	/* sample area entirely outside image? */
	if (x2 < 0 || x1 > in->x - 1 || y2 < 0 || y1 > in->y - 1) {
		return;
	}

	/* wrap interpolation pixels - main difference from bilinear_interpolation_color  */
	if (x1 < 0) x1 = in->x + x1;
	if (y1 < 0) y1 = in->y + y1;

	if (x2 >= in->x) x2 = x2 - in->x;
	if (y2 >= in->y) y2 = y2 - in->y;

	a = u - floorf(u);
	b = v - floorf(v);
	a_b = a * b; ma_b = (1.0f - a) * b; a_mb = a * (1.0f - b); ma_mb = (1.0f - a) * (1.0f - b);

	if (outF) {
		/* sample including outside of edges of image */
		row1 = in->rect_float + ((size_t)in->x) * y1 * 4 + 4 * x1;
		row2 = in->rect_float + ((size_t)in->x) * y2 * 4 + 4 * x1;
		row3 = in->rect_float + ((size_t)in->x) * y1 * 4 + 4 * x2;
		row4 = in->rect_float + ((size_t)in->x) * y2 * 4 + 4 * x2;

		outF[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
		outF[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
		outF[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
		outF[3] = ma_mb * row1[3] + a_mb * row3[3] + ma_b * row2[3] + a_b * row4[3];

		/* clamp here or else we can easily get off-range */
		CLAMP(outF[0], 0.0f, 1.0f);
		CLAMP(outF[1], 0.0f, 1.0f);
		CLAMP(outF[2], 0.0f, 1.0f);
		CLAMP(outF[3], 0.0f, 1.0f);
	}
	if (outI) {
		/* sample including outside of edges of image */
		row1I = (unsigned char *)in->rect + ((size_t)in->x) * y1 * 4 + 4 * x1;
		row2I = (unsigned char *)in->rect + ((size_t)in->x) * y2 * 4 + 4 * x1;
		row3I = (unsigned char *)in->rect + ((size_t)in->x) * y1 * 4 + 4 * x2;
		row4I = (unsigned char *)in->rect + ((size_t)in->x) * y2 * 4 + 4 * x2;

		/* need to add 0.5 to avoid rounding down (causes darken with the smear brush)
		 * tested with white images and this should not wrap back to zero */
		outI[0] = (ma_mb * row1I[0] + a_mb * row3I[0] + ma_b * row2I[0] + a_b * row4I[0]) + 0.5f;
		outI[1] = (ma_mb * row1I[1] + a_mb * row3I[1] + ma_b * row2I[1] + a_b * row4I[1]) + 0.5f;
		outI[2] = (ma_mb * row1I[2] + a_mb * row3I[2] + ma_b * row2I[2] + a_b * row4I[2]) + 0.5f;
		outI[3] = (ma_mb * row1I[3] + a_mb * row3I[3] + ma_b * row2I[3] + a_b * row4I[3]) + 0.5f;
	}
}

void bilinear_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
	unsigned char *outI = NULL;
	float *outF = NULL;

	if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) {
		return;
	}

	pixel_from_buffer(out, &outI, &outF, xout, yout); /* gcc warns these could be uninitialized, but its ok */

	bilinear_interpolation_color(in, outI, outF, u, v);
}

/* function assumes out to be zero'ed, only does RGBA */
/* NEAREST INTERPOLATION */
void nearest_interpolation_color(struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
	const float *dataF;
	unsigned char *dataI;
	int y1, x1;

	/* ImBuf in must have a valid rect or rect_float, assume this is already checked */

	x1 = (int)(u);
	y1 = (int)(v);

	/* sample area entirely outside image? */
	if (x1 < 0 || x1 > in->x - 1 || y1 < 0 || y1 > in->y - 1) {
		if (outI)
			outI[0] = outI[1] = outI[2] = outI[3] = 0;
		if (outF)
			outF[0] = outF[1] = outF[2] = outF[3] = 0.0f;
		return;
	}

	/* sample including outside of edges of image */
	if (x1 < 0 || y1 < 0) {
		if (outI) {
			outI[0] = 0;
			outI[1] = 0;
			outI[2] = 0;
			outI[3] = 0;
		}
		if (outF) {
			outF[0] = 0.0f;
			outF[1] = 0.0f;
			outF[2] = 0.0f;
			outF[3] = 0.0f;
		}
	}
	else {
		dataI = (unsigned char *)in->rect + ((size_t)in->x) * y1 * 4 + 4 * x1;
		if (outI) {
			outI[0] = dataI[0];
			outI[1] = dataI[1];
			outI[2] = dataI[2];
			outI[3] = dataI[3];
		}
		dataF = in->rect_float + ((size_t)in->x) * y1 * 4 + 4 * x1;
		if (outF) {
			outF[0] = dataF[0];
			outF[1] = dataF[1];
			outF[2] = dataF[2];
			outF[3] = dataF[3];
		}
	}
}


void nearest_interpolation_color_wrap(struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
	const float *dataF;
	unsigned char *dataI;
	int y, x;

	/* ImBuf in must have a valid rect or rect_float, assume this is already checked */

	x = (int) floor(u);
	y = (int) floor(v);

	x = x % in->x;
	y = y % in->y;

	/* wrap interpolation pixels - main difference from nearest_interpolation_color  */
	if (x < 0) x += in->x;
	if (y < 0) y += in->y;

	dataI = (unsigned char *)in->rect + ((size_t)in->x) * y * 4 + 4 * x;
	if (outI) {
		outI[0] = dataI[0];
		outI[1] = dataI[1];
		outI[2] = dataI[2];
		outI[3] = dataI[3];
	}
	dataF = in->rect_float + ((size_t)in->x) * y * 4 + 4 * x;
	if (outF) {
		outF[0] = dataF[0];
		outF[1] = dataF[1];
		outF[2] = dataF[2];
		outF[3] = dataF[3];
	}
}

void nearest_interpolation(ImBuf *in, ImBuf *out, float x, float y, int xout, int yout)
{
	unsigned char *outI = NULL;
	float *outF = NULL;

	if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) {
		return;
	}

	pixel_from_buffer(out, &outI, &outF, xout, yout); /* gcc warns these could be uninitialized, but its ok */

	nearest_interpolation_color(in, outI, outF, x, y);
}

/*********************** Threaded image processing *************************/

static void processor_apply_func(TaskPool * __restrict pool, void *taskdata, int UNUSED(threadid))
{
	void (*do_thread) (void *) = (void (*) (void *)) BLI_task_pool_userdata(pool);
	do_thread(taskdata);
}

void IMB_processor_apply_threaded(int buffer_lines, int handle_size, void *init_customdata,
                                  void (init_handle) (void *handle, int start_line, int tot_line,
                                                      void *customdata),
                                  void *(do_thread) (void *))
{
	const int lines_per_task = 64;

	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool;

	void *handles;
	int total_tasks = (buffer_lines + lines_per_task - 1) / lines_per_task;
	int i, start_line;

	task_pool = BLI_task_pool_create(task_scheduler, do_thread);

	handles = MEM_callocN(handle_size * total_tasks, "processor apply threaded handles");

	start_line = 0;

	for (i = 0; i < total_tasks; i++) {
		int lines_per_current_task;
		void *handle = ((char *) handles) + handle_size * i;

		if (i < total_tasks - 1)
			lines_per_current_task = lines_per_task;
		else
			lines_per_current_task = buffer_lines - start_line;

		init_handle(handle, start_line, lines_per_current_task, init_customdata);

		BLI_task_pool_push(task_pool, processor_apply_func, handle, false, TASK_PRIORITY_LOW);

		start_line += lines_per_task;
	}

	/* work and wait until tasks are done */
	BLI_task_pool_work_and_wait(task_pool);

	/* Free memory. */
	MEM_freeN(handles);
	BLI_task_pool_free(task_pool);
}

typedef struct ScanlineGlobalData {
	void *custom_data;
	ScanlineThreadFunc do_thread;
	int scanlines_per_task;
	int total_scanlines;
} ScanlineGlobalData;

typedef struct ScanlineTask {
	int start_scanline;
	int num_scanlines;
} ScanlineTask;

static void processor_apply_scanline_func(TaskPool * __restrict pool,
                                          void *taskdata,
                                          int UNUSED(threadid))
{
	ScanlineGlobalData *data = BLI_task_pool_userdata(pool);
	int start_scanline = POINTER_AS_INT(taskdata);
	int num_scanlines = min_ii(data->scanlines_per_task,
	                           data->total_scanlines - start_scanline);
	data->do_thread(data->custom_data,
	                start_scanline,
	                num_scanlines);
}

void IMB_processor_apply_threaded_scanlines(int total_scanlines,
                                            ScanlineThreadFunc do_thread,
                                            void *custom_data)
{
	const int scanlines_per_task = 64;
	ScanlineGlobalData data;
	data.custom_data = custom_data;
	data.do_thread = do_thread;
	data.scanlines_per_task = scanlines_per_task;
	data.total_scanlines = total_scanlines;
	const int total_tasks = (total_scanlines + scanlines_per_task - 1) / scanlines_per_task;
	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool = BLI_task_pool_create(task_scheduler, &data);
	for (int i = 0, start_line = 0; i < total_tasks; i++) {
		BLI_task_pool_push(task_pool,
		                   processor_apply_scanline_func,
		                   POINTER_FROM_INT(start_line),
		                   false,
		                   TASK_PRIORITY_LOW);
		start_line += scanlines_per_task;
	}

	/* work and wait until tasks are done */
	BLI_task_pool_work_and_wait(task_pool);

	/* Free memory. */
	BLI_task_pool_free(task_pool);
}

/* Alpha-under */

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3])
{
	size_t a = ((size_t)x) * y;
	float *fp = rect_float;

	while (a--) {
		if (fp[3] == 0.0f) {
			copy_v3_v3(fp, backcol);
		}
		else {
			float mul = 1.0f - fp[3];

			fp[0] += mul * backcol[0];
			fp[1] += mul * backcol[1];
			fp[2] += mul * backcol[2];
		}

		fp[3] = 1.0f;

		fp += 4;
	}
}

void IMB_alpha_under_color_byte(unsigned char *rect, int x, int y, float backcol[3])
{
	size_t a = ((size_t)x) * y;
	unsigned char *cp = rect;

	while (a--) {
		if (cp[3] == 255) {
			/* pass */
		}
		else if (cp[3] == 0) {
			cp[0] = backcol[0] * 255;
			cp[1] = backcol[1] * 255;
			cp[2] = backcol[2] * 255;
		}
		else {
			float alpha = cp[3] / 255.0;
			float mul = 1.0f - alpha;

			cp[0] = (cp[0] * alpha) + mul * backcol[0];
			cp[1] = (cp[1] * alpha) + mul * backcol[1];
			cp[2] = (cp[2] * alpha) + mul * backcol[2];
		}

		cp[3] = 255;

		cp += 4;
	}
}
