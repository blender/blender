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
 * Contributor(s): (c) 2006 Blender Foundation, full refactor
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/render_types.h
 *  \ingroup render
 */


#ifndef __RENDER_TYPES_H__
#define __RENDER_TYPES_H__

/* ------------------------------------------------------------------------- */
/* exposed internal in render module only! */
/* ------------------------------------------------------------------------- */

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_threads.h"

#include "BKE_main.h"

#include "RE_pipeline.h"

struct Object;
struct RenderEngine;
struct ReportList;
struct Main;

/* this is handed over to threaded hiding/passes/shading engine */
typedef struct RenderPart {
	struct RenderPart *next, *prev;

	RenderResult *result;			/* result of part rendering */
	ListBase fullresult;			/* optional full sample buffers */

	rcti disprect;					/* part coordinates within total picture */
	int rectx, recty;				/* the size */
	int nr;							/* nr is partnr */
	short status;
} RenderPart;

enum {
	PART_STATUS_NONE        = 0,
	PART_STATUS_IN_PROGRESS = 1,
	PART_STATUS_RENDERED    = 2,
	PART_STATUS_MERGED      = 3
};

/* controls state of render, everything that's read-only during render stage */
struct Render {
	struct Render *next, *prev;
	char name[RE_MAXNAME];
	int slot;

	/* state settings */
	short flag, ok, result_ok;

	/* result of rendering */
	RenderResult *result;
	/* if render with single-layer option, other rendered layers are stored here */
	RenderResult *pushedresult;
	/* a list of RenderResults, for fullsample */
	ListBase fullresult;
	/* read/write mutex, all internal code that writes to re->result must use a
	 * write lock, all external code must use a read lock. internal code is assumed
	 * to not conflict with writes, so no lock used for that */
	ThreadRWMutex resultmutex;

	/* window size, display rect, viewplane */
	int winx, winy;			/* buffer width and height with percentage applied
							 * without border & crop. convert to long before multiplying together to avoid overflow. */
	rcti disprect;			/* part within winx winy */
	rctf viewplane;			/* mapped on winx winy */

	/* final picture width and height (within disprect) */
	int rectx, recty;

	/* real maximum size of parts after correction for minimum
	 * partx*xparts can be larger than rectx, in that case last part is smaller */
	int partx, party;

	/* Camera transform, only used by Freestyle. */
	float viewmat[4][4], viewinv[4][4];
	float viewmat_orig[4][4];	/* for incremental render */
	float winmat[4][4];

	/* clippping */
	float clipsta;
	float clipend;

	/* main, scene, and its full copy of renderdata and world */
	struct Main *main;
	Scene *scene;
	RenderData r;
	ListBase view_layers;
	int active_view_layer;
	struct Object *camera_override;
	unsigned int lay, layer_override;

	ThreadRWMutex partsmutex;
	ListBase parts;

	/* render engine */
	struct RenderEngine *engine;

#ifdef WITH_FREESTYLE
	struct Main *freestyle_bmain;
	ListBase freestyle_renders;
#endif

	/* callbacks */
	void (*display_init)(void *handle, RenderResult *rr);
	void *dih;
	void (*display_clear)(void *handle, RenderResult *rr);
	void *dch;
	void (*display_update)(void *handle, RenderResult *rr, volatile rcti *rect);
	void *duh;
	void (*current_scene_update)(void *handle, struct Scene *scene);
	void *suh;

	void (*stats_draw)(void *handle, RenderStats *ri);
	void *sdh;
	void (*progress)(void *handle, float i);
	void *prh;

	void (*draw_lock)(void *handle, int i);
	void *dlh;
	int (*test_break)(void *handle);
	void *tbh;

	RenderStats i;

	struct ReportList *reports;

	void **movie_ctx_arr;
	char viewname[MAX_NAME];

	/* TODO replace by a whole draw manager. */
	void *gl_context;
	void *gpu_context;
};

/* **************** defines ********************* */

/* R.flag */
#define R_ANIMATION		1

#endif /* __RENDER_TYPES_H__ */
