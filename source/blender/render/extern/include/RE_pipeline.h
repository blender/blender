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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RE_PIPELINE_H
#define RE_PIPELINE_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "BKE_utildefines.h"

struct Scene;
struct RenderData;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is what is exposed of render to outside world */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


#define RE_MAXNAME	32

/* only used as handle */
typedef struct Render Render;

/* Render Result usage:

- render engine allocates/frees and delivers raw floating point rects
- right now it's full rects, but might become tiles or file 
- the display client has to allocate display rects, sort out what to display, 
  and how it's converted
*/

/* a renderlayer is a full image, but with all passes and samples */
/* size of the rects is defined in RenderResult */
typedef struct RenderLayer {
	struct RenderLayer *next, *prev;
	
	char name[RE_MAXNAME];
	int flag, type;
	
	float *rectf;	/* standard rgba buffer */
	float *rectz;	/* standard camera coordinate zbuffer */
	
	ListBase passes;
	
} RenderLayer;

typedef struct RenderResult {
	
	/* target image size */
	int rectx, recty;
	short crop, pad;
	
	/* optional, 32 bits version of (composited?) layers */
	int *rect32;
	
	/* coordinates within final image (after cropping) */
	rcti tilerect;
	
	/* the main buffers */
	ListBase layers;
	
	/* optional saved endresult on disk */
	char exrfile[FILE_MAXDIR];
	int filehandle;
	
} RenderResult;

typedef struct RenderStats {
	int totface, totvert, tothalo, totlamp, totpart;
	short curfield, curblur, curpart, partsdone, convertdone;
	double starttime, lastframetime;
	
} RenderStats;

/* *********************** API ******************** */

/* the name is used as identifier, so elsewhere in blender the result can retrieved */
/* calling a new render with same name, frees automatic existing render */
Render *RE_NewRender (const char *name);
Render *RE_GetRender(const char *name);

/* use free render as signal to do everything over (previews) */
void RE_FreeRender (Render *re);
/* only called on exit */
void RE_FreeAllRender (void);

/* get results and statistics */
RenderResult *RE_GetResult(Render *re);
RenderStats *RE_GetStats(Render *re);
void RE_ResultGet32(Render *re, unsigned int *rect);

/* obligatory initialize call, disprect is optional */
void RE_InitState (struct Render *re, struct RenderData *rd, int winx, int winy, rcti *disprect);

/* use this to change disprect of active render */
void RE_SetDispRect (struct Render *re, rcti *disprect);

/* set up the viewplane/perspective matrix, three choices */
void RE_SetCamera(Render *re, struct Object *camera);
void RE_SetWindow (Render *re, rctf *viewplane, float clipsta, float clipend);
void RE_SetOrtho (Render *re, rctf *viewplane, float clipsta, float clipend);

/* option to set viewmatrix before making dbase */
void RE_SetView (Render *re, float mat[][4]);

/* make or free the dbase */
void RE_Database_FromScene(Render *re, Scene *scene, int use_camera_view);
void RE_Database_Free (Render *re);

/* project dbase again, when viewplane/perspective changed */
void RE_DataBase_ApplyWindow(Render *re);

/* the main processor, assumes all was set OK! */
void RE_TileProcessor(Render *re);

/* only RE_NewRender() needed, main Blender render calls */
void RE_BlenderFrame(Render *re, struct Scene *scene, int frame);
void RE_BlenderAnim(Render *re, struct Scene *scene, int sfra, int efra);


/* display and event callbacks */
void RE_display_init_cb	(Render *re, void (*f)(RenderResult *rr));
void RE_display_clear_cb(Render *re, void (*f)(RenderResult *rr));
void RE_display_draw_cb	(Render *re, void (*f)(RenderResult *rr, struct rcti *rect));
void RE_stats_draw_cb	(Render *re, void (*f)(RenderStats *rs));
void RE_timecursor_cb	(Render *re, void (*f)(int));
void RE_test_break_cb	(Render *re, int (*f)(void));
void RE_test_return_cb	(Render *re, int (*f)(void));
void RE_error_cb		(Render *re, void (*f)(const char *str));




#endif /* RE_PIPELINE_H */

