/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"

namespace blender::gpu {
class Texture;
}
struct ExrHandle;
struct ImBuf;
struct Image;
struct ImageFormatData;
struct MovieWriter;
struct Main;
struct Object;
struct RenderData;
struct RenderResult;
struct ReportList;
struct Scene;
struct StampData;
struct ViewLayer;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is what is exposed of render to outside world */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* length of the scene name + passname */
#define RE_MAXNAME ((MAX_ID_NAME - 2) + 10)

/* only used as handle */
struct RenderView {
  struct RenderView *next, *prev;
  char name[/*EXR_VIEW_MAXNAME*/ 64];

  /**
   * Image buffer of a composited layer or a sequencer output.
   * The `ibuf` is only allocated if it has an actual data in one of its buffers
   * (float, byte, or GPU).
   */
  struct ImBuf *ibuf;
};

struct RenderPass {
  struct RenderPass *next, *prev;
  int channels;
  char name[/*EXR_PASS_MAXNAME*/ 64];
  char chan_id[/*EXR_PASS_MAXCHAN*/ 24];

  /**
   * Image buffer which contains data of this pass.
   *
   * The data can be either CPU side stored in ibuf->float_buffer, or a GPU-side stored in
   * ibuf->gpu (during rendering, i.e.).
   *
   * The pass data storage is lazily allocated, and until data is actually provided
   * (via either CPU buffer of GPU texture) the ibuf is not allocated.
   */
  struct ImBuf *ibuf;

  int rectx, recty;

  char fullname[/*EXR_PASS_MAXNAME*/ 64];
  char view[/*EXR_VIEW_MAXNAME*/ 64];
  /** Quick lookup. */
  int view_id;

  char _pad0[4];
};

/**
 * - A render-layer is a full image, but with all passes and samples.
 * - The size of the rects is defined in #RenderResult.
 * - After render, the Combined pass is in combined,
 *   for render-layers read from files it is a real pass.
 */
struct RenderLayer {
  struct RenderLayer *next, *prev;

  /** copy of RenderData */
  char name[RE_MAXNAME];
  int layflag, passflag, pass_xor;

  int rectx, recty;

  ListBase passes;
};

struct RenderResult {
  struct RenderResult *next, *prev;

  /* The number of users of this render result. Default value is 0. The result is freed when
   * #RE_FreeRenderResult is called with the render result with 0 users. In a way this is
   * off-by-one, but it is the easiest for the currently used zero-initialized state. The way to
   * think of it is the number of extra users.
   *
   * TODO: Make it an actual number of users, so the #RE_FreeRenderResult frees the result when
   * the number of users goes to 0.
   *
   * TODO: Make it atomic. Currently it is not to allow shallow copying. */
  int user_counter;

  /* target image size */
  int rectx, recty;

  /* The temporary storage to pass image data from #RE_AcquireResultImage.
   * Is null pointer when the RenderResult is not coming from the #RE_AcquireResultImage, and is
   * a pointer to an existing ibuf in either RenderView or a RenderPass otherwise. */
  struct ImBuf *ibuf;

  /* coordinates within final image (after cropping) */
  rcti tilerect;
  /* offset to apply to get a border render in full image */
  int xof, yof;

  /* the main buffers */
  ListBase layers;

  /* multiView maps to a StringVector in OpenEXR */
  ListBase views; /* RenderView */

  /* Render layer to display. */
  RenderLayer *renlay;

  /* for render results in Image, verify validity for sequences */
  int framenr;

  /**
   * Pixels per meter (for image output).
   * - Typically initialized via #BKE_scene_ppm_get.
   * - May be zero which indicates the PPM being "unset".
   *   Although in most cases a scene is available.
   */
  double ppm[2];

  /* for acquire image, to indicate if it there is a combined layer */
  bool have_combined;

  /* render info text */
  char *text;
  char *error;

  struct StampData *stamp_data;

  bool passes_allocated;
};

struct RenderStats {
  int cfra;
  bool localview;
  double starttime, lastframetime;
  const char *infostr, *statstr;
  char scene_name[MAX_ID_NAME - 2];
  int mem_used, mem_peak;
};

/* *********************** API ******************** */

/**
 * The owner is a unique identifier for the render, either an original scene
 * datablock for regular renders, or an area for preview renders.
 * Calling a new render with an existing owner frees the existing render. */
struct Render *RE_NewRender(const void *owner);
struct Render *RE_GetRender(const void *owner);

struct Scene;
struct Render *RE_NewSceneRender(const struct Scene *scene);
struct Render *RE_GetSceneRender(const struct Scene *scene);

struct RenderEngineType;
struct ViewRender *RE_NewViewRender(struct RenderEngineType *engine_type);

/* Creates a new render for interactive compositing of the given scene. If an existing render
 * exists for the given scene, it is returned instead. See interactive_compositor_renders in
 * RenderGlobal for more information. */
struct Render *RE_NewInteractiveCompositorRender(const struct Scene *scene);

/* Assign default dummy callbacks. */

/**
 * Called for new renders and when finishing rendering
 * so we always have valid callbacks on a render.
 */
void RE_InitRenderCB(struct Render *re);

/**
 * Use free render as signal to do everything over (previews).
 *
 * Only call this while you know it will remove the link too.
 */
void RE_FreeRender(struct Render *re);
void RE_FreeViewRender(struct ViewRender *view_render);
/**
 * Only called on exit.
 */
void RE_FreeAllRender(void);

/**
 * On file load, free all interactive compositor renders.
 */
void RE_FreeInteractiveCompositorRenders(void);

/**
 * On file load, free render results.
 */
void RE_FreeAllRenderResults(void);

/**
 * On file load or changes engines, free persistent render data.
 * Assumes no engines are currently rendering.
 */
void RE_FreeAllPersistentData(void);
/**
 * Free persistent render data, optionally only for the given scene.
 */
void RE_FreePersistentData(const struct Scene *scene);

/**
 * Free cached GPU textures to reduce memory usage.
 */
void RE_FreeGPUTextureCaches(void);

/**
 * Free cached GPU textures, contexts and compositor to reduce memory usage,
 * when nothing in the UI requires them anymore.
 */
void RE_FreeUnusedGPUResources(void);

/**
 * Get results and statistics.
 */
void RE_FreeRenderResult(struct RenderResult *rr);
/**
 * If you want to know exactly what has been done.
 */
struct RenderResult *RE_AcquireResultRead(struct Render *re);
struct RenderResult *RE_AcquireResultWrite(struct Render *re);
void RE_ReferenceRenderResult(struct RenderResult *rr);
void RE_ReleaseResult(struct Render *re);
/**
 * Same as #RE_AcquireResultImage but creating the necessary views to store the result
 * fill provided result struct with a copy of thew views of what is done so far the
 * #RenderResult.views #ListBase needs to be freed after with #RE_ReleaseResultImageViews
 */
void RE_AcquireResultImageViews(struct Render *re, struct RenderResult *rr);
/**
 * Clear temporary #RenderResult struct.
 */
void RE_ReleaseResultImageViews(struct Render *re, struct RenderResult *rr);

/**
 * Fill provided result struct with what's currently active or done.
 * This #RenderResult struct is the only exception to the rule of a #RenderResult
 * always having at least one #RenderView.
 */
void RE_AcquireResultImage(struct Render *re, struct RenderResult *rr, int view_id);
void RE_ReleaseResultImage(struct Render *re);
void RE_SwapResult(struct Render *re, struct RenderResult **rr);
void RE_ClearResult(struct Render *re);
struct RenderStats *RE_GetStats(struct Render *re);

/**
 * Caller is responsible for allocating `rect` in correct size!
 */
void RE_ResultGet32(struct Render *re, unsigned int *rect);
void RE_ResultGetFloat(struct Render *re, float *rect);

bool RE_ResultIsMultiView(struct RenderResult *rr);

void RE_render_result_full_channel_name(char *fullname,
                                        const char *layname,
                                        const char *passname,
                                        const char *viewname,
                                        const char *chan_id,
                                        int channel);

struct ImBuf *RE_render_result_rect_to_ibuf(struct RenderResult *rr,
                                            const struct ImageFormatData *imf,
                                            const float dither,
                                            int view_id);
void RE_render_result_rect_from_ibuf(struct RenderResult *rr,
                                     const struct ImBuf *ibuf,
                                     int view_id);

struct RenderLayer *RE_GetRenderLayer(struct RenderResult *rr, const char *name);
float *RE_RenderLayerGetPass(struct RenderLayer *rl, const char *name, const char *viewname);
struct ImBuf *RE_RenderLayerGetPassImBuf(struct RenderLayer *rl,
                                         const char *name,
                                         const char *viewname);

bool RE_HasSingleLayer(struct Render *re);

/**
 * Add passes for grease pencil.
 * Create a render-layer and render-pass for grease-pencil layer.
 */
struct RenderPass *RE_create_gp_pass(struct RenderResult *rr,
                                     const char *layername,
                                     const char *viewname);

void RE_create_render_pass(struct RenderResult *rr,
                           const char *name,
                           int channels,
                           const char *chan_id,
                           const char *layername,
                           const char *viewname,
                           bool allocate);

/**
 * Obligatory initialize call, doesn't change during entire render sequence.
 * \param disprect: is optional. if NULL it assumes full window render.
 */
void RE_InitState(struct Render *re,
                  struct Render *source,
                  struct RenderData *rd,
                  struct ListBase *render_layers,
                  struct ViewLayer *single_layer,
                  int winx,
                  int winy,
                  const rcti *disprect);

/**
 * Set up the view-plane/perspective matrix, three choices.
 *
 * \return camera override if set.
 */
struct Object *RE_GetCamera(struct Render *re);
void RE_SetOverrideCamera(struct Render *re, struct Object *cam_ob);
/**
 * Per render, there's one persistent view-plane. Parts will set their own view-planes.
 *
 * \note call this after #RE_InitState().
 */
void RE_SetCamera(struct Render *re, const struct Object *cam_ob);

/**
 * Get current view and window transform.
 */
void RE_GetViewPlane(struct Render *re, rctf *r_viewplane, rcti *r_disprect);

/**
 * Set the render threads based on the command-line and auto-threads setting.
 */
void RE_init_threadcount(Render *re);

bool RE_WriteRenderViewsMovie(struct ReportList *reports,
                              struct RenderResult *rr,
                              struct Scene *scene,
                              struct RenderData *rd,
                              struct MovieWriter **movie_writers,
                              int totvideos,
                              bool preview);

/**
 * General Blender frame render call.
 *
 * \note Only #RE_NewRender() needed, main Blender render calls.
 *
 * \param write_still: Saves frames to disk (typically disabled). Useful for batch-operations
 * (e.g. rendering from Python) when an additional save action for is inconvenient.
 * This is the default behavior for #RE_RenderAnim.
 */
void RE_RenderFrame(struct Render *re,
                    struct Main *bmain,
                    struct Scene *scene,
                    struct ViewLayer *single_layer,
                    struct Object *camera_override,
                    int frame,
                    float subframe,
                    bool write_still);
/**
 * A version of #RE_RenderFrame that saves images to disk.
 */
void RE_RenderAnim(struct Render *re,
                   struct Main *bmain,
                   struct Scene *scene,
                   struct ViewLayer *single_layer,
                   struct Object *camera_override,
                   int sfra,
                   int efra,
                   int tfra);
#ifdef WITH_FREESTYLE
void RE_RenderFreestyleStrokes(struct Render *re,
                               struct Main *bmain,
                               struct Scene *scene,
                               bool render);
void RE_RenderFreestyleExternal(struct Render *re);
#endif

void RE_SetActiveRenderView(struct Render *re, const char *viewname);
const char *RE_GetActiveRenderView(struct Render *re);

/**
 * Error reporting.
 */
void RE_SetReports(struct Render *re, struct ReportList *reports);

/**
 * Main preview render call.
 */
void RE_PreviewRender(struct Render *re, struct Main *bmain, struct Scene *scene);

/**
 * Only the temp file!
 */
bool RE_ReadRenderResult(struct Scene *scene, struct Scene *scenode);

struct RenderResult *RE_MultilayerConvert(
    ExrHandle *exrhandle, const char *colorspace, bool predivide, int rectx, int recty);

/* Display and event callbacks. */

/**
 * Image and movie output has to move to either #ImBuf or kernel.
 */
void RE_display_init_cb(struct Render *re,
                        void *handle,
                        void (*f)(void *handle, RenderResult *rr));
void RE_display_clear_cb(struct Render *re,
                         void *handle,
                         void (*f)(void *handle, RenderResult *rr));
void RE_display_update_cb(struct Render *re,
                          void *handle,
                          void (*f)(void *handle, RenderResult *rr, struct rcti *rect));
void RE_stats_draw_cb(struct Render *re, void *handle, void (*f)(void *handle, RenderStats *rs));
void RE_progress_cb(struct Render *re, void *handle, void (*f)(void *handle, float));
void RE_draw_lock_cb(struct Render *re, void *handle, void (*f)(void *handle, bool lock));
void RE_test_break_cb(struct Render *re, void *handle, bool (*f)(void *handle));
void RE_prepare_viewlayer_cb(struct Render *re,
                             void *handle,
                             bool (*f)(void *handle, ViewLayer *vl, struct Depsgraph *depsgraph));
void RE_current_scene_update_cb(struct Render *re,
                                void *handle,
                                void (*f)(void *handle, struct Scene *scene));

void RE_system_gpu_context_ensure(Render *re);
void RE_system_gpu_context_free(Render *re);
void *RE_system_gpu_context_get(Render *re);

void *RE_blender_gpu_context_ensure(Render *re);
void RE_blender_gpu_context_free(Render *re);

/**
 * \param x: ranges from -1 to 1.
 *
 * TODO: Should move to kernel once... still unsure on how/where.
 */
float RE_filter_value(int type, float x);

bool RE_seq_render_active(struct Scene *scene, struct RenderData *rd);

/**
 * Used in the interface to decide whether to show layers or passes.
 */
bool RE_layers_have_name(struct RenderResult *result);
bool RE_passes_have_name(struct RenderLayer *rl);

struct RenderPass *RE_pass_find_by_name(struct RenderLayer *rl,
                                        const char *name,
                                        const char *viewname);

/**
 * Set the buffer data of the render pass.
 * The pass takes ownership of the data, and creates an implicit sharing handle to allow its
 * sharing with other users.
 */
void RE_pass_set_buffer_data(struct RenderPass *pass, float *data);

/**
 * Ensure a GPU texture corresponding to the render buffer data exists.
 */
blender::gpu::Texture *RE_pass_ensure_gpu_texture_cache(struct Render *re,
                                                        struct RenderPass *rpass);

void RE_GetCameraWindow(struct Render *re, const struct Object *camera, float r_winmat[4][4]);
/**
 * Must be called after #RE_GetCameraWindow(), does not change `re->winmat`.
 */
void RE_GetCameraWindowWithOverscan(const struct Render *re, float overscan, float r_winmat[4][4]);
void RE_GetCameraModelMatrix(const struct Render *re,
                             const struct Object *camera,
                             float r_modelmat[4][4]);

void RE_GetWindowMatrixWithOverscan(bool is_ortho,
                                    float clip_start,
                                    float clip_end,
                                    rctf viewplane,
                                    float overscan,
                                    float r_winmat[4][4]);

struct Scene *RE_GetScene(struct Render *re);
void RE_SetScene(struct Render *re, struct Scene *sce);

bool RE_is_rendering_allowed(struct Scene *scene,
                             struct ViewLayer *single_layer,
                             struct Object *camera_override,
                             struct ReportList *reports);

bool RE_allow_render_generic_object(struct Object *ob);

/******* defined in `render_result.cc` *********/

bool RE_HasCombinedLayer(const RenderResult *result);
bool RE_HasFloatPixels(const RenderResult *result);
bool RE_RenderResult_is_stereo(const RenderResult *result);
struct RenderView *RE_RenderViewGetById(struct RenderResult *rr, int view_id);
struct RenderView *RE_RenderViewGetByName(struct RenderResult *rr, const char *viewname);

RenderResult *RE_DuplicateRenderResult(RenderResult *rr);

struct ImBuf *RE_RenderPassEnsureImBuf(RenderPass *render_pass);
struct ImBuf *RE_RenderViewEnsureImBuf(const RenderResult *render_result, RenderView *render_view);

/* Returns true if the pass is a color (as opposite of data) and needs to be color managed. */
bool RE_RenderPassIsColor(const RenderPass *render_pass);
