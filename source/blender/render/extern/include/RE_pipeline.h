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
struct NodeBlurData;

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

typedef struct RenderPass {
	struct RenderPass *next, *prev;
	int passtype;
	float *rect;
} RenderPass;

/* a renderlayer is a full image, but with all passes and samples */
/* size of the rects is defined in RenderResult */
typedef struct RenderLayer {
	struct RenderLayer *next, *prev;
	
	/* copy of RenderData */
	char name[RE_MAXNAME];		
	unsigned int lay;			
	int layflag, passflag;		
	
	float *rectf;	/* 4 float, standard rgba buffer */
	
	ListBase passes;
	
} RenderLayer;

typedef struct RenderResult {
	
	/* target image size */
	int rectx, recty;
	short crop, pad;
	
	/* optional, 32 bits version of picture, used for ogl render and image curves */
	int *rect32;
	/* if this exists, a copy of one of layers, or result of composited layers */
	float *rectf;
	/* if this exists, a copy of one of layers, or result of composited layers */
	float *rectz;
	
	/* coordinates within final image (after cropping) */
	rcti tilerect;
	
	/* the main buffers */
	ListBase layers;
	
	/* allowing live updates: */
	rcti renrect;
	RenderLayer *renlay;
	
	/* optional saved endresult on disk */
	char exrfile[FILE_MAXDIR];
	int filehandle;
	
} RenderResult;

typedef struct RenderStats {
	int totface, totvert, tothalo, totlamp, totpart;
	short curfield, curblur, curpart, partsdone, convertdone;
	double starttime, lastframetime;
	char *infostr;
	
} RenderStats;

/* *********************** API ******************** */

/* the name is used as identifier, so elsewhere in blender the result can retrieved */
/* calling a new render with same name, frees automatic existing render */
struct Render *RE_NewRender (const char *name);
struct Render *RE_GetRender(const char *name);

/* use free render as signal to do everything over (previews) */
void RE_FreeRender (struct Render *re);
/* only called on exit */
void RE_FreeAllRender (void);

/* get results and statistics */
RenderResult *RE_GetResult(struct Render *re);
void RE_GetResultImage(struct Render *re, RenderResult *rr);
RenderStats *RE_GetStats(struct Render *re);
void RE_ResultGet32(struct Render *re, unsigned int *rect);
float *RE_RenderLayerGetPass(RenderLayer *rl, int passtype);

/* obligatory initialize call, disprect is optional */
void RE_InitState (struct Render *re, struct RenderData *rd, int winx, int winy, rcti *disprect);

/* use this to change disprect of active render */
void RE_SetDispRect (struct Render *re, rcti *disprect);

/* set up the viewplane/perspective matrix, three choices */
void RE_SetCamera(struct Render *re, struct Object *camera);
void RE_SetWindow (struct Render *re, rctf *viewplane, float clipsta, float clipend);
void RE_SetOrtho (struct Render *re, rctf *viewplane, float clipsta, float clipend);

/* option to set viewmatrix before making dbase */
void RE_SetView (struct Render *re, float mat[][4]);

/* make or free the dbase */
void RE_Database_FromScene(struct Render *re, struct Scene *scene, int use_camera_view);
void RE_Database_Free (struct Render *re);

/* project dbase again, when viewplane/perspective changed */
void RE_DataBase_ApplyWindow(struct Render *re);

/* the main processor, assumes all was set OK! */
void RE_TileProcessor(struct Render *re, int firsttile);

/* only RE_NewRender() needed, main Blender render calls */
void RE_BlenderFrame(struct Render *re, struct Scene *scene, int frame);
void RE_BlenderAnim(struct Render *re, struct Scene *scene, int sfra, int efra);


/* display and event callbacks */
void RE_display_init_cb	(struct Render *re, void (*f)(RenderResult *rr));
void RE_display_clear_cb(struct Render *re, void (*f)(RenderResult *rr));
void RE_display_draw_cb	(struct Render *re, void (*f)(RenderResult *rr, struct rcti *rect));
void RE_stats_draw_cb	(struct Render *re, void (*f)(RenderStats *rs));
void RE_timecursor_cb	(struct Render *re, void (*f)(int));
void RE_test_break_cb	(struct Render *re, int (*f)(void));
void RE_test_return_cb	(struct Render *re, int (*f)(void));
void RE_error_cb		(struct Render *re, void (*f)(const char *str));

/* should move to kernel once... still unsure on how/where */
float RE_filter_value(int type, float x);
/* vector blur zbuffer method */
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nbd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect);

#endif /* RE_PIPELINE_H */

