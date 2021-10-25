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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RE_pipeline.h
 *  \ingroup render
 */

#ifndef __RE_PIPELINE_H__
#define __RE_PIPELINE_H__

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

struct bMovieHandle;
struct bNodeTree;
struct Image;
struct ImageFormatData;
struct Main;
struct NodeBlurData;
struct Object;
struct RenderData;
struct RenderResult;
struct ReportList;
struct Scene;
struct SceneRenderLayer;
struct EnvMap;
struct StampData;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is what is exposed of render to outside world */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* length of the scene name + passname */
#define RE_MAXNAME	((MAX_ID_NAME - 2) + 10)

/* only used as handle */
typedef struct Render Render;

/* Render Result usage:
 *
 * - render engine allocates/frees and delivers raw floating point rects
 * - right now it's full rects, but might become tiles or file
 * - the display client has to allocate display rects, sort out what to display,
 *   and how it's converted
 */

typedef struct RenderView {
	struct RenderView *next, *prev;
	char name[64];		/* EXR_VIEW_MAXNAME */

	/* if this exists, result of composited layers */
	float *rectf;
	/* if this exists, result of composited layers */
	float *rectz;
	/* optional, 32 bits version of picture, used for sequencer, ogl render and image curves */
	int *rect32;

} RenderView;

typedef struct RenderPass {
	struct RenderPass *next, *prev;
	int channels;
	char name[64];		/* amount defined in openexr_multi.h */
	char chan_id[8];	/* amount defined in openexr_multi.h */
	float *rect;
	int rectx, recty;

	char fullname[64]; /* EXR_PASS_MAXNAME */
	char view[64];		/* EXR_VIEW_MAXNAME */
	int view_id;	/* quick lookup */

	int pad;
} RenderPass;


/* a renderlayer is a full image, but with all passes and samples */
/* size of the rects is defined in RenderResult */
/* after render, the Combined pass is in combined, for renderlayers read from files it is a real pass */
typedef struct RenderLayer {
	struct RenderLayer *next, *prev;
	
	/* copy of RenderData */
	char name[RE_MAXNAME];
	unsigned int lay, lay_zmask, lay_exclude;
	int layflag, passflag, pass_xor;
	
	struct Material *mat_override;
	struct Group *light_override;

	/* MULTIVIEW_TODO: acolrect and scolrect are not supported by multiview at the moment.
	 * If they are really required they should be in RenderView instead */

	float *acolrect;	/* 4 float, optional transparent buffer, needs storage for display updates */
	float *scolrect;	/* 4 float, optional strand buffer, needs storage for display updates */
	int *display_buffer;	/* 4 char, optional color managed display buffer which is used when
	                         * Save Buffer is enabled to display combined pass of the screen. */
	int rectx, recty;

	/* optional saved endresult on disk */
	void *exrhandle;
	
	ListBase passes;
	
} RenderLayer;

typedef struct RenderResult {
	struct RenderResult *next, *prev;
	
	/* target image size */
	int rectx, recty;
	short crop, sample_nr;
	
	/* the following rect32, rectf and rectz buffers are for temporary storage only, for RenderResult structs
	 * created in #RE_AcquireResultImage - which do not have RenderView */

	/* optional, 32 bits version of picture, used for ogl render and image curves */
	int *rect32;
	/* if this exists, a copy of one of layers, or result of composited layers */
	float *rectf;
	/* if this exists, a copy of one of layers, or result of composited layers */
	float *rectz;
	
	/* coordinates within final image (after cropping) */
	rcti tilerect;
	/* offset to apply to get a border render in full image */
	int xof, yof;
	
	/* the main buffers */
	ListBase layers;
	
	/* multiView maps to a StringVector in OpenEXR */
	ListBase views;  /* RenderView */

	/* allowing live updates: */
	volatile rcti renrect;
	volatile RenderLayer *renlay;
	
	/* optional saved endresult on disk */
	int do_exr_tile;
	
	/* for render results in Image, verify validity for sequences */
	int framenr;

	/* for acquire image, to indicate if it there is a combined layer */
	int have_combined;

	/* render info text */
	char *text;
	char *error;

	struct StampData *stamp_data;
} RenderResult;


typedef struct RenderStats {
	int cfra;
	int totface, totvert, totstrand, tothalo, totlamp, totpart;
	short curfield, curblur, curpart, partsdone, convertdone, curfsa;
	bool localview;
	double starttime, lastframetime;
	const char *infostr, *statstr;
	char scene_name[MAX_ID_NAME - 2];
	float mem_used, mem_peak;
} RenderStats;

/* *********************** API ******************** */

/* the name is used as identifier, so elsewhere in blender the result can retrieved */
/* calling a new render with same name, frees automatic existing render */
struct Render *RE_NewRender(const char *name);
struct Render *RE_GetRender(const char *name);

/* assign default dummy callbacks */
void RE_InitRenderCB(struct Render *re);

/* use free render as signal to do everything over (previews) */
void RE_FreeRender(struct Render *re);
/* only called on exit */
void RE_FreeAllRender(void);
/* Free memory used by persistent data.
 * Invoked when loading new file.
 */
void RE_FreeAllPersistentData(void);
/* only call on file load */
void RE_FreeAllRenderResults(void);
/* for external render engines that can keep persistent data */
void RE_FreePersistentData(void);

/* get results and statistics */
void RE_FreeRenderResult(struct RenderResult *rr);
struct RenderResult *RE_AcquireResultRead(struct Render *re);
struct RenderResult *RE_AcquireResultWrite(struct Render *re);
void RE_ReleaseResult(struct Render *re);
void RE_AcquireResultImageViews(struct Render *re, struct RenderResult *rr);
void RE_ReleaseResultImageViews(struct Render *re, struct RenderResult *rr);
void RE_AcquireResultImage(struct Render *re, struct RenderResult *rr, const int view_id);
void RE_ReleaseResultImage(struct Render *re);
void RE_SwapResult(struct Render *re, struct RenderResult **rr);
struct RenderStats *RE_GetStats(struct Render *re);

void RE_ResultGet32(struct Render *re, unsigned int *rect);
void RE_AcquiredResultGet32(struct Render *re, struct RenderResult *result, unsigned int *rect, const int view_id);

void RE_render_result_rect_from_ibuf(
        struct RenderResult *rr, struct RenderData *rd,
        struct ImBuf *ibuf, const int view_id);

struct RenderLayer *RE_GetRenderLayer(struct RenderResult *rr, const char *name);
float *RE_RenderLayerGetPass(volatile struct RenderLayer *rl, const char *name, const char *viewname);

/* add passes for grease pencil */
struct RenderPass *RE_create_gp_pass(struct RenderResult *rr, const char *layername, const char *viewname);

/* obligatory initialize call, disprect is optional */
void RE_InitState(struct Render *re, struct Render *source, struct RenderData *rd,
                  struct SceneRenderLayer *srl,
                  int winx, int winy, rcti *disprect);
void RE_ChangeResolution(struct Render *re, int winx, int winy, rcti *disprect);
void RE_ChangeModeFlag(struct Render *re, int flag, bool clear);

/* set up the viewplane/perspective matrix, three choices */
struct Object *RE_GetCamera(struct Render *re); /* return camera override if set */
void RE_SetOverrideCamera(struct Render *re, struct Object *camera);
void RE_SetCamera(struct Render *re, struct Object *camera);
void RE_SetEnvmapCamera(struct Render *re, struct Object *cam_ob, float viewscale, float clipsta, float clipend);
void RE_SetWindow(struct Render *re, const rctf *viewplane, float clipsta, float clipend);
void RE_SetOrtho(struct Render *re, const rctf *viewplane, float clipsta, float clipend);
void RE_SetPixelSize(struct Render *re, float pixsize);

/* option to set viewmatrix before making dbase */
void RE_SetView(struct Render *re, float mat[4][4]);

/* get current view and window transform */
void RE_GetView(struct Render *re, float mat[4][4]);
void RE_GetViewPlane(struct Render *re, rctf *r_viewplane, rcti *r_disprect);

/* make or free the dbase */
void RE_Database_FromScene(
        struct Render *re, struct Main *bmain, struct Scene *scene,
        unsigned int lay, int use_camera_view);
void RE_Database_Preprocess(struct Render *re);
void RE_Database_Free(struct Render *re);

/* project dbase again, when viewplane/perspective changed */
void RE_DataBase_ApplyWindow(struct Render *re);
/* rotate scene again, for incremental render */
void RE_DataBase_IncrementalView(struct Render *re, float viewmat[4][4], int restore);

/* set the render threads based on the commandline and autothreads setting */
void RE_init_threadcount(Render *re);

/* the main processor, assumes all was set OK! */
void RE_TileProcessor(struct Render *re);

bool RE_WriteRenderViewsImage(
        struct ReportList *reports, struct RenderResult *rr, struct Scene *scene, const bool stamp, char *name);
bool RE_WriteRenderViewsMovie(
        struct ReportList *reports, struct RenderResult *rr, struct Scene *scene, struct RenderData *rd,
        struct bMovieHandle *mh, void **movie_ctx_arr,
        const int totvideos, bool preview);

/* only RE_NewRender() needed, main Blender render calls */
void RE_BlenderFrame(struct Render *re, struct Main *bmain, struct Scene *scene,
                     struct SceneRenderLayer *srl, struct Object *camera_override,
                     unsigned int lay_override, int frame, const bool write_still);
void RE_BlenderAnim(struct Render *re, struct Main *bmain, struct Scene *scene, struct Object *camera_override,
                    unsigned int lay_override, int sfra, int efra, int tfra);
#ifdef WITH_FREESTYLE
void RE_RenderFreestyleStrokes(struct Render *re, struct Main *bmain, struct Scene *scene, int render);
void RE_RenderFreestyleExternal(struct Render *re);
#endif

void RE_SetActiveRenderView(struct Render *re, const char *viewname);
const char *RE_GetActiveRenderView(struct Render *re);

/* error reporting */
void RE_SetReports(struct Render *re, struct ReportList *reports);

/* main preview render call */
void RE_PreviewRender(struct Render *re, struct Main *bmain, struct Scene *scene);

bool RE_ReadRenderResult(struct Scene *scene, struct Scene *scenode);
bool RE_WriteRenderResult(
        struct ReportList *reports, RenderResult *rr, const char *filename,
        struct ImageFormatData *imf, const bool multiview, const char *view);
struct RenderResult *RE_MultilayerConvert(
        void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty);

extern const float default_envmap_layout[];
bool RE_WriteEnvmapResult(
        struct ReportList *reports, struct Scene *scene, struct EnvMap *env,
        const char *relpath, const char imtype, float layout[12]);

/* do a full sample buffer compo */
void RE_MergeFullSample(struct Render *re, struct Main *bmain, struct Scene *sce, struct bNodeTree *ntree);

/* display and event callbacks */
void RE_display_init_cb	(struct Render *re, void *handle, void (*f)(void *handle, RenderResult *rr));
void RE_display_clear_cb(struct Render *re, void *handle, void (*f)(void *handle, RenderResult *rr));
void RE_display_update_cb(struct Render *re, void *handle, void (*f)(void *handle, RenderResult *rr, volatile struct rcti *rect));
void RE_stats_draw_cb	(struct Render *re, void *handle, void (*f)(void *handle, RenderStats *rs));
void RE_progress_cb	(struct Render *re, void *handle, void (*f)(void *handle, float));
void RE_draw_lock_cb		(struct Render *re, void *handle, void (*f)(void *handle, int));
void RE_test_break_cb	(struct Render *re, void *handle, int (*f)(void *handle));
void RE_current_scene_update_cb(struct Render *re, void *handle, void (*f)(void *handle, struct Scene *scene));

/* should move to kernel once... still unsure on how/where */
float RE_filter_value(int type, float x);
/* vector blur zbuffer method */
void RE_zbuf_accumulate_vecblur(
        struct NodeBlurData *nbd, int xsize, int ysize, float *newrect,
        const float *imgrect, float *vecbufrect, const float *zbufrect);

int RE_seq_render_active(struct Scene *scene, struct RenderData *rd);

bool RE_layers_have_name(struct RenderResult *result);

struct RenderPass *RE_pass_find_by_name(volatile struct RenderLayer *rl, const char *name, const char *viewname);
struct RenderPass *RE_pass_find_by_type(volatile struct RenderLayer *rl, int passtype, const char *viewname);

/* shaded view or baking options */
#define RE_BAKE_LIGHT				0	/* not listed in rna_scene.c -> can't be enabled! */
#define RE_BAKE_ALL					1
#define RE_BAKE_AO					2
#define RE_BAKE_NORMALS				3
#define RE_BAKE_TEXTURE				4
#define RE_BAKE_DISPLACEMENT		5
#define RE_BAKE_SHADOW				6
#define RE_BAKE_SPEC_COLOR			7
#define RE_BAKE_SPEC_INTENSITY		8
#define RE_BAKE_MIRROR_COLOR		9
#define RE_BAKE_MIRROR_INTENSITY	10
#define RE_BAKE_ALPHA				11
#define RE_BAKE_EMIT				12
#define RE_BAKE_DERIVATIVE			13
#define RE_BAKE_VERTEX_COLORS		14

void RE_Database_Baking(
        struct Render *re, struct Main *bmain, struct Scene *scene,
        unsigned int lay, const int type, struct Object *actob);

void RE_DataBase_GetView(struct Render *re, float mat[4][4]);
void RE_GetCameraWindow(struct Render *re, struct Object *camera, int frame, float mat[4][4]);
void RE_GetCameraModelMatrix(struct Render *re, struct Object *camera, float r_mat[4][4]);
struct Scene *RE_GetScene(struct Render *re);

bool RE_force_single_renderlayer(struct Scene *scene);
bool RE_is_rendering_allowed(struct Scene *scene, struct Object *camera_override, struct ReportList *reports);

bool RE_allow_render_generic_object(struct Object *ob);

/* RE_updateRenderInstances flag */
enum {
	RE_OBJECT_INSTANCES_UPDATE_VIEW  = (1 << 0),
	RE_OBJECT_INSTANCES_UPDATE_OBMAT = (1 << 1)
};
void RE_updateRenderInstances(Render *re, int flag);

/******* defined in render_result.c *********/

bool RE_HasFakeLayer(RenderResult *res);
bool RE_RenderResult_is_stereo(RenderResult *res);
struct RenderView *RE_RenderViewGetById(struct RenderResult *res, const int view_id);
struct RenderView *RE_RenderViewGetByName(struct RenderResult *res, const char *viewname);

RenderResult *RE_DuplicateRenderResult(RenderResult *rr);

#endif /* __RE_PIPELINE_H__ */

