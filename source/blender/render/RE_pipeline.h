/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BLI_implicit_sharing.h"

struct ImBuf;
struct Image;
struct ImageFormatData;
struct Main;
struct Object;
struct RenderData;
struct RenderResult;
struct ReportList;
struct Scene;
struct StampData;
struct ViewLayer;
struct bMovieHandle;

#ifdef __cplusplus
extern "C" {
#endif

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is what is exposed of render to outside world */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* length of the scene name + passname */
#define RE_MAXNAME ((MAX_ID_NAME - 2) + 10)

/* only used as handle */
typedef struct Render Render;

/* Buffer of a floating point values which uses implicit sharing.
 *
 * The buffer is allocated by render passes creation, and then is shared with the render result
 * and image buffer. */
typedef struct RenderBuffer {
  float *data;
  const ImplicitSharingInfoHandle *sharing_info;
} RenderBuffer;

/* Specialized render buffer to store 8bpp passes. */
typedef struct RenderByteBuffer {
  uint8_t *data;
  const ImplicitSharingInfoHandle *sharing_info;
} RenderByteBuffer;

/* Render Result usage:
 *
 * - render engine allocates/frees and delivers raw floating point rects
 * - right now it's full rects, but might become tiles or file
 * - the display client has to allocate display rects, sort out what to display,
 *   and how it's converted
 */

typedef struct RenderView {
  struct RenderView *next, *prev;
  char name[64]; /* EXR_VIEW_MAXNAME */

  /* if this exists, result of composited layers */
  RenderBuffer combined_buffer;
  RenderBuffer z_buffer;

  /* optional, 32 bits version of picture, used for sequencer, OpenGL render and image curves */
  RenderByteBuffer byte_buffer;

} RenderView;

typedef struct RenderPass {
  struct RenderPass *next, *prev;
  int channels;
  char name[64];   /* amount defined in IMB_openexr.h */
  char chan_id[8]; /* amount defined in IMB_openexr.h */

  RenderBuffer buffer;

  int rectx, recty;

  char fullname[64]; /* EXR_PASS_MAXNAME */
  char view[64];     /* EXR_VIEW_MAXNAME */
  int view_id;       /* quick lookup */

  char _pad0[4];
} RenderPass;

/**
 * - A render-layer is a full image, but with all passes and samples.
 * - The size of the rects is defined in #RenderResult.
 * - After render, the Combined pass is in combined,
 *   for render-layers read from files it is a real pass.
 */
typedef struct RenderLayer {
  struct RenderLayer *next, *prev;

  /** copy of RenderData */
  char name[RE_MAXNAME];
  int layflag, passflag, pass_xor;

  int rectx, recty;

  /** Optional saved end-result on disk. */
  void *exrhandle;

  ListBase passes;

} RenderLayer;

typedef struct RenderResult {
  struct RenderResult *next, *prev;

  /* target image size */
  int rectx, recty;

  /* The following byte, combined, and z buffers are for temporary storage only,
   * for RenderResult structs created in #RE_AcquireResultImage - which do not have RenderView */

  /* Optional, 32 bits version of picture, used for OpenGL render and image curves. */
  RenderByteBuffer byte_buffer;

  /* if this exists, a copy of one of layers, or result of composited layers */
  RenderBuffer combined_buffer;
  RenderBuffer z_buffer;

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

  /* for acquire image, to indicate if it there is a combined layer */
  bool have_combined;

  /* render info text */
  char *text;
  char *error;

  struct StampData *stamp_data;

  bool passes_allocated;
} RenderResult;

typedef struct RenderStats {
  int cfra;
  bool localview;
  double starttime, lastframetime;
  const char *infostr, *statstr;
  char scene_name[MAX_ID_NAME - 2];
  float mem_used, mem_peak;
} RenderStats;

/* *********************** API ******************** */

/**
 * The name is used as identifier, so elsewhere in blender the result can retrieved.
 * Calling a new render with same name, frees automatic existing render.
 */
struct Render *RE_NewRender(const char *name);
struct Render *RE_GetRender(const char *name);

struct Scene;
struct Render *RE_NewSceneRender(const struct Scene *scene);
struct Render *RE_GetSceneRender(const struct Scene *scene);

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
/**
 * Only called on exit.
 */
void RE_FreeAllRender(void);

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
 * Get results and statistics.
 */
void RE_FreeRenderResult(struct RenderResult *rr);
/**
 * If you want to know exactly what has been done.
 */
struct RenderResult *RE_AcquireResultRead(struct Render *re);
struct RenderResult *RE_AcquireResultWrite(struct Render *re);
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
/**
 * Only for acquired results, for lock.
 *
 * \note The caller is responsible for allocating `rect` in correct size!
 */
void RE_AcquiredResultGet32(struct Render *re,
                            struct RenderResult *result,
                            unsigned int *rect,
                            int view_id);

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
RenderBuffer *RE_RenderLayerGetPassBuffer(struct RenderLayer *rl,
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
                  rcti *disprect);

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
                              struct bMovieHandle *mh,
                              void **movie_ctx_arr,
                              int totvideos,
                              bool preview);

/**
 * General Blender frame render call.
 *
 * \note Only #RE_NewRender() needed, main Blender render calls.
 *
 * \param write_still: Saves frames to disk (typically disabled). Useful for batch-operations
 * (rendering from Python for e.g.) when an additional save action for is inconvenient.
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
    void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty);

/* Display and event callbacks. */

/**
 * Image and movie output has to move to either imbuf or kernel.
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
void RE_current_scene_update_cb(struct Render *re,
                                void *handle,
                                void (*f)(void *handle, struct Scene *scene));

void RE_gl_context_create(Render *re);
void RE_gl_context_destroy(Render *re);
void *RE_gl_context_get(Render *re);
void *RE_gpu_context_get(Render *re);

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
 * Only provided for API compatibility, don't use this in new code!
 */
struct RenderPass *RE_pass_find_by_type(struct RenderLayer *rl,
                                        int passtype,
                                        const char *viewname);

/**
 * Set the buffer data of the render pass.
 * The pass takes ownership of the data, and creates an implicit sharing handle to allow its
 * sharing with other users.
 */
void RE_pass_set_buffer_data(struct RenderPass *pass, float *data);

/* shaded view or baking options */
#define RE_BAKE_NORMALS 0
#define RE_BAKE_DISPLACEMENT 1
#define RE_BAKE_AO 2

void RE_GetCameraWindow(struct Render *re, const struct Object *camera, float mat[4][4]);
/**
 * Must be called after #RE_GetCameraWindow(), does not change `re->winmat`.
 */
void RE_GetCameraWindowWithOverscan(const struct Render *re, float overscan, float r_winmat[4][4]);
void RE_GetCameraModelMatrix(const struct Render *re,
                             const struct Object *camera,
                             float r_modelmat[4][4]);

struct Scene *RE_GetScene(struct Render *re);
void RE_SetScene(struct Render *re, struct Scene *sce);

bool RE_is_rendering_allowed(struct Scene *scene,
                             struct ViewLayer *single_layer,
                             struct Object *camera_override,
                             struct ReportList *reports);

bool RE_allow_render_generic_object(struct Object *ob);

/******* defined in render_result.c *********/

bool RE_HasCombinedLayer(const RenderResult *result);
bool RE_HasFloatPixels(const RenderResult *result);
bool RE_RenderResult_is_stereo(const RenderResult *result);
struct RenderView *RE_RenderViewGetById(struct RenderResult *rr, int view_id);
struct RenderView *RE_RenderViewGetByName(struct RenderResult *rr, const char *viewname);

RenderResult *RE_DuplicateRenderResult(RenderResult *rr);

/**
 * Create new render buffer which takes ownership of the given data.
 * Creates an implicit sharing  handle for the data as well. */
RenderBuffer RE_RenderBuffer_new(float *data);

/**
 * Assign the buffer data.
 *
 * The current buffer data is freed and the new one is assigned, and the implicit sharing for it.
 */
void RE_RenderBuffer_assign_data(RenderBuffer *render_buffer, float *data);

/**
 * Effectively `lhs = rhs`. The lhs will share the same buffer as the rhs (with an increased user
 * counter).
 *
 * The current content of the lhs is freed.
 * The rhs and its data is allowed to be nullptr, in which case the lhs's data will be nullptr
 * after this call.
 */
void RE_RenderBuffer_assign_shared(RenderBuffer *lhs, const RenderBuffer *rhs);

/**
 * Free data of the given buffer.
 *
 * The data and implicit sharing information of the buffer is set to nullptr after this call.
 * The buffer itself is not freed.
 */
void RE_RenderBuffer_data_free(RenderBuffer *render_buffer);

/* Implementation of above, but for byte buffer. */
/* TODO(sergey): Once everything is C++ we can remove the duplicated API. */
RenderByteBuffer RE_RenderByteBuffer_new(uint8_t *data);
void RE_RenderByteBuffer_assign_data(RenderByteBuffer *render_buffer, uint8_t *data);
void RE_RenderByteBuffer_assign_shared(RenderByteBuffer *lhs, const RenderByteBuffer *rhs);
void RE_RenderByteBuffer_data_free(RenderByteBuffer *render_buffer);

#ifdef __cplusplus
}
#endif
