/*
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Depsgraph;
struct Editing;
struct GPUOffScreen;
struct GSet;
struct ImBuf;
struct Main;
struct Mask;
struct ReportList;
struct Scene;
struct SeqIndexBuildContext;
struct Sequence;
struct SequenceModifierData;
struct Stereo3dFormat;
struct StripElem;
struct TextVars;
struct bContext;
struct bSound;
struct BlendWriter;
struct BlendDataReader;
struct BlendLibReader;
struct SequencerToolSettings;

/* Wipe effect */
enum {
  DO_SINGLE_WIPE,
  DO_DOUBLE_WIPE,
  /* DO_BOX_WIPE, */   /* UNUSED */
  /* DO_CROSS_WIPE, */ /* UNUSED */
  DO_IRIS_WIPE,
  DO_CLOCK_WIPE,
};

/* RNA enums, just to be more readable */
enum {
  SEQ_SIDE_MOUSE = -1,
  SEQ_SIDE_NONE = 0,
  SEQ_SIDE_LEFT,
  SEQ_SIDE_RIGHT,
  SEQ_SIDE_BOTH,
  SEQ_SIDE_NO_CHANGE,
};

/* **********************************************************************
 * sequencer.c
 *
 * Sequencer iterators
 * **********************************************************************
 */

typedef struct SeqIterator {
  struct Sequence **array;
  int tot, cur;

  struct Sequence *seq;
  int valid;
} SeqIterator;

#define SEQ_ALL_BEGIN(ed, _seq) \
  { \
    SeqIterator iter_macro; \
    for (SEQ_iterator_begin(ed, &iter_macro, false); iter_macro.valid; \
         SEQ_iterator_next(&iter_macro)) { \
      _seq = iter_macro.seq;

#define SEQ_ALL_END \
  } \
  SEQ_iterator_end(&iter_macro); \
  } \
  ((void)0)

#define SEQ_CURRENT_BEGIN(_ed, _seq) \
  { \
    SeqIterator iter_macro; \
    for (SEQ_iterator_begin(_ed, &iter_macro, true); iter_macro.valid; \
         SEQ_iterator_next(&iter_macro)) { \
      _seq = iter_macro.seq;

#define SEQ_CURRENT_END SEQ_ALL_END

void SEQ_iterator_begin(struct Editing *ed, SeqIterator *iter, const bool use_current_sequences);
void SEQ_iterator_next(SeqIterator *iter);
void SEQ_iterator_end(SeqIterator *iter);

/* **********************************************************************
 * render.c
 *
 * Sequencer render functions
 * **********************************************************************
 */

typedef enum eSeqTaskId {
  SEQ_TASK_MAIN_RENDER,
  SEQ_TASK_PREFETCH_RENDER,
} eSeqTaskId;

typedef struct SeqRenderData {
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  int rectx;
  int recty;
  int preview_render_size;
  int for_render;
  int motion_blur_samples;
  float motion_blur_shutter;
  bool skip_cache;
  bool is_proxy_render;
  bool is_prefetch_render;
  int view_id;
  /* ID of task for asigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id;

  /* special case for OpenGL render */
  struct GPUOffScreen *gpu_offscreen;
  // int gpu_samples;
  // bool gpu_full_samples;
} SeqRenderData;

struct ImBuf *SEQ_render_give_ibuf(const SeqRenderData *context,
                                   float timeline_frame,
                                   int chanshown);
struct ImBuf *SEQ_render_give_ibuf_direct(const SeqRenderData *context,
                                          float timeline_frame,
                                          struct Sequence *seq);
void SEQ_render_init_colorspace(struct Sequence *seq);
void SEQ_render_new_render_data(struct Main *bmain,
                                struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                int rectx,
                                int recty,
                                int preview_render_size,
                                int for_render,
                                SeqRenderData *r_context);
int SEQ_render_evaluate_frame(struct ListBase *seqbase, int timeline_frame);
struct StripElem *SEQ_render_give_stripelem(struct Sequence *seq, int timeline_frame);

/* **********************************************************************
 * render.c
 *
 * Sequencer color space functions
 * ********************************************************************** */

void SEQ_render_imbuf_from_sequencer_space(struct Scene *scene, struct ImBuf *ibuf);
void SEQ_render_pixel_from_sequencer_space_v4(struct Scene *scene, float pixel[4]);

/* **********************************************************************
 * sequencer.c
 *
 * Sequencer scene functions
 * ********************************************************************** */

struct SequencerToolSettings *SEQ_tool_settings_init(void);
void SEQ_tool_settings_free(struct SequencerToolSettings *tool_settings);
eSeqImageFitMethod SEQ_tool_settings_fit_method_get(struct Scene *scene);
void SEQ_tool_settings_fit_method_set(struct Scene *scene, eSeqImageFitMethod fit_method);

struct SequencerToolSettings *SEQ_tool_settings_copy(struct SequencerToolSettings *tool_settings);
struct Editing *SEQ_editing_get(struct Scene *scene, bool alloc);
struct Editing *SEQ_editing_ensure(struct Scene *scene);
void SEQ_editing_free(struct Scene *scene, const bool do_id_user);
struct ListBase *SEQ_active_seqbase_get(const struct Editing *ed);
void SEQ_sort(struct Scene *scene);
struct Sequence *SEQ_sequence_from_strip_elem(ListBase *seqbase, struct StripElem *se);
struct Sequence *SEQ_select_active_get(struct Scene *scene);
int SEQ_select_active_get_pair(struct Scene *scene,
                               struct Sequence **seq_act,
                               struct Sequence **seq_other);
void SEQ_select_active_set(struct Scene *scene, struct Sequence *seq);
struct Mask *SEQ_active_mask_get(struct Scene *scene);
/* apply functions recursively */
int SEQ_iterator_seqbase_recursive_apply(struct ListBase *seqbase,
                                         int (*apply_fn)(struct Sequence *seq, void *),
                                         void *arg);
int SEQ_iterator_recursive_apply(struct Sequence *seq,
                                 int (*apply_fn)(struct Sequence *, void *),
                                 void *arg);
float SEQ_time_sequence_get_fps(struct Scene *scene, struct Sequence *seq);
int SEQ_time_find_next_prev_edit(struct Scene *scene,
                                 int timeline_frame,
                                 const short side,
                                 const bool do_skip_mute,
                                 const bool do_center,
                                 const bool do_unselected);
/* maintenance functions, mostly for RNA */
void SEQ_sequence_free(struct Scene *scene, struct Sequence *seq, const bool do_clean_animdata);
void SEQ_relations_sequence_free_anim(struct Sequence *seq);
const char *SEQ_sequence_give_name(struct Sequence *seq);
ListBase *SEQ_get_seqbase_from_sequence(struct Sequence *seq, int *r_offset);
void SEQ_time_update_sequence(struct Scene *scene, struct Sequence *seq);
void SEQ_time_update_sequence_bounds(struct Scene *scene, struct Sequence *seq);
void SEQ_add_reload_new_file(struct Main *bmain,
                             struct Scene *scene,
                             struct Sequence *seq,
                             const bool lock_range);
void SEQ_add_movie_reload_if_needed(struct Main *bmain,
                                    struct Scene *scene,
                                    struct Sequence *seq,
                                    bool *r_was_reloaded,
                                    bool *r_can_produce_frames);
void SEQ_alpha_mode_from_file_extension(struct Sequence *seq);
void SEQ_relations_update_changed_seq_and_deps(struct Scene *scene,
                                               struct Sequence *changed_seq,
                                               int len_change,
                                               int ibuf_change);
bool SEQ_relations_check_scene_recursion(struct Scene *scene, struct ReportList *reports);
bool SEQ_relations_render_loop_check(struct Sequence *seq_main, struct Sequence *seq);
int SEQ_time_cmp_time_startdisp(const void *a, const void *b);

/* **********************************************************************
 * proxy.c
 *
 * Proxy functions
 * ********************************************************************** */

bool SEQ_proxy_rebuild_context(struct Main *bmain,
                               struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct Sequence *seq,
                               struct GSet *file_list,
                               ListBase *queue);
void SEQ_proxy_rebuild(struct SeqIndexBuildContext *context,
                       short *stop,
                       short *do_update,
                       float *progress);
void SEQ_proxy_rebuild_finish(struct SeqIndexBuildContext *context, bool stop);
void SEQ_proxy_set(struct Sequence *seq, bool value);
bool SEQ_can_use_proxy(struct Sequence *seq, int psize);
int SEQ_rendersize_to_proxysize(int render_size);
double SEQ_rendersize_to_scale_factor(int size);

/* **********************************************************************
 * image_cache.c
 *
 * Sequencer memory cache management functions
 * ********************************************************************** */

void SEQ_cache_cleanup(struct Scene *scene);
void SEQ_cache_iterate(struct Scene *scene,
                       void *userdata,
                       bool callback_init(void *userdata, size_t item_count),
                       bool callback_iter(void *userdata,
                                          struct Sequence *seq,
                                          int timeline_frame,
                                          int cache_type,
                                          float cost));

/* **********************************************************************
 * prefetch.c
 *
 * Sequencer frame prefetching
 * ********************************************************************** */

#define SEQ_CACHE_COST_MAX 10.0f
void SEQ_prefetch_stop_all(void);
void SEQ_prefetch_stop(struct Scene *scene);
bool SEQ_prefetch_need_redraw(struct Main *bmain, struct Scene *scene);

/* **********************************************************************
 * sequencer.c
 *
 * Sequencer editing functions
 * **********************************************************************
 */

/* for transform but also could use elsewhere */
int SEQ_transform_get_left_handle_frame(struct Sequence *seq, bool metaclip);
int SEQ_transform_get_right_handle_frame(struct Sequence *seq, bool metaclip);
void SEQ_transform_set_left_handle_frame(struct Sequence *seq, int val);
void SEQ_transform_set_right_handle_frame(struct Sequence *seq, int val);
void SEQ_transform_handle_xlimits(struct Sequence *seq, int leftflag, int rightflag);
bool SEQ_transform_sequence_can_be_translated(struct Sequence *seq);
bool SEQ_transform_single_image_check(struct Sequence *seq);
void SEQ_transform_fix_single_image_seq_offsets(struct Sequence *seq);
bool SEQ_transform_test_overlap(struct ListBase *seqbasep, struct Sequence *test);
void SEQ_transform_translate_sequence(struct Scene *scene, struct Sequence *seq, int delta);
const struct Sequence *SEQ_get_topmost_sequence(const struct Scene *scene, int frame);
struct ListBase *SEQ_get_seqbase_by_seq(struct ListBase *seqbase, struct Sequence *seq);
void SEQ_offset_animdata(struct Scene *scene, struct Sequence *seq, int ofs);
void SEQ_dupe_animdata(struct Scene *scene, const char *name_src, const char *name_dst);
bool SEQ_transform_seqbase_shuffle_ex(struct ListBase *seqbasep,
                                      struct Sequence *test,
                                      struct Scene *evil_scene,
                                      int channel_delta);
bool SEQ_transform_seqbase_shuffle(struct ListBase *seqbasep,
                                   struct Sequence *test,
                                   struct Scene *evil_scene);
bool SEQ_transform_seqbase_shuffle_time(ListBase *seqbasep,
                                        struct Scene *evil_scene,
                                        ListBase *markers,
                                        const bool use_sync_markers);
bool SEQ_transform_seqbase_isolated_sel_check(struct ListBase *seqbase);
void SEQ_relations_free_imbuf(struct Scene *scene, struct ListBase *seqbasep, bool for_render);
struct Sequence *SEQ_sequence_dupli_recursive(const struct Scene *scene_src,
                                              struct Scene *scene_dst,
                                              struct ListBase *new_seq_list,
                                              struct Sequence *seq,
                                              int dupe_flag);
int SEQ_edit_sequence_swap(struct Sequence *seq_a, struct Sequence *seq_b, const char **error_str);
void SEQ_sound_update_bounds_all(struct Scene *scene);
void SEQ_sound_update_bounds(struct Scene *scene, struct Sequence *seq);
void SEQ_sound_update_muting(struct Editing *ed);
void SEQ_sound_update(struct Scene *scene, struct bSound *sound);
void SEQ_sound_update_length(struct Main *bmain, struct Scene *scene);
void SEQ_sequence_base_unique_name_recursive(ListBase *seqbasep, struct Sequence *seq);
void SEQ_sequence_base_dupli_recursive(const struct Scene *scene_src,
                                       struct Scene *scene_dst,
                                       struct ListBase *nseqbase,
                                       const struct ListBase *seqbase,
                                       int dupe_flag,
                                       const int flag);
bool SEQ_sequence_has_source(struct Sequence *seq);
struct Sequence *SEQ_get_sequence_by_name(struct ListBase *seqbase,
                                          const char *name,
                                          bool recursive);
void SEQ_edit_flag_for_removal(struct Scene *scene,
                               struct ListBase *seqbase,
                               struct Sequence *seq);
void SEQ_edit_remove_flagged_sequences(struct Scene *scene, struct ListBase *seqbase);

/* **********************************************************************
 * sequencer.c
 *
 * Cache invalidation
 * **********************************************************************
 */

void SEQ_relations_invalidate_cache_raw(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_cache_preprocessed(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_cache_composite(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_dependent(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_scene_strips(struct Main *bmain, struct Scene *scene_target);
void SEQ_relations_invalidate_movieclip_strips(struct Main *bmain, struct MovieClip *clip_target);
void SEQ_relations_invalidate_cache_in_range(struct Scene *scene,
                                             struct Sequence *seq,
                                             struct Sequence *range_mask,
                                             int invalidate_types);
void SEQ_relations_free_all_anim_ibufs(struct Scene *scene, int timeline_frame);

/* **********************************************************************
 * util.c
 *
 * Add strips
 * **********************************************************************
 */

void SEQ_set_scale_to_fit(const struct Sequence *seq,
                          const int image_width,
                          const int image_height,
                          const int preview_width,
                          const int preview_height,
                          const eSeqImageFitMethod fit_method);

/* **********************************************************************
 * sequencer.c
 *
 * Add strips
 * **********************************************************************
 */

/* api for adding new sequence strips */
typedef struct SeqLoadInfo {
  int start_frame;
  int end_frame;
  int channel;
  int flag; /* use sound, replace sel */
  int type;
  int len;         /* only for image strips */
  char path[1024]; /* 1024 = FILE_MAX */
  eSeqImageFitMethod fit_method;

  /* multiview */
  char views_format;
  struct Stereo3dFormat *stereo3d_format;

  /* return values */
  char name[64];
  struct Sequence *seq_sound; /* for movie's */
  int tot_success;
  int tot_error;
} SeqLoadInfo;

/* SeqLoadInfo.flag */
#define SEQ_LOAD_REPLACE_SEL (1 << 0)
#define SEQ_LOAD_FRAME_ADVANCE (1 << 1)
#define SEQ_LOAD_MOVIE_SOUND (1 << 2)
#define SEQ_LOAD_SOUND_CACHE (1 << 3)
#define SEQ_LOAD_SYNC_FPS (1 << 4)
#define SEQ_LOAD_SOUND_MONO (1 << 5)

/* seq_dupli' flags */
#define SEQ_DUPE_UNIQUE_NAME (1 << 0)
#define SEQ_DUPE_CONTEXT (1 << 1)
#define SEQ_DUPE_ANIM (1 << 2)
#define SEQ_DUPE_ALL (1 << 3) /* otherwise only selected are copied */
#define SEQ_DUPE_IS_RECURSIVE_CALL (1 << 4)

/* use as an api function */
typedef struct Sequence *(*SeqLoadFn)(struct bContext *, ListBase *, struct SeqLoadInfo *);

struct Sequence *SEQ_sequence_alloc(ListBase *lb, int timeline_frame, int machine, int type);
struct Sequence *SEQ_add_image_strip(struct bContext *C,
                                     ListBase *seqbasep,
                                     struct SeqLoadInfo *seq_load);
struct Sequence *SEQ_add_sound_strip(struct bContext *C,
                                     ListBase *seqbasep,
                                     struct SeqLoadInfo *seq_load);
struct Sequence *SEQ_add_movie_strip(struct bContext *C,
                                     ListBase *seqbasep,
                                     struct SeqLoadInfo *seq_load);

/* **********************************************************************
 * modifier.c
 *
 * Modifiers
 * **********************************************************************
 */

typedef struct SequenceModifierTypeInfo {
  /* default name for the modifier */
  char name[64]; /* MAX_NAME */

  /* DNA structure name used on load/save filed */
  char struct_name[64]; /* MAX_NAME */

  /* size of modifier data structure, used by allocation */
  int struct_size;

  /* data initialization */
  void (*init_data)(struct SequenceModifierData *smd);

  /* free data used by modifier,
   * only modifier-specific data should be freed, modifier descriptor would
   * be freed outside of this callback
   */
  void (*free_data)(struct SequenceModifierData *smd);

  /* copy data from one modifier to another */
  void (*copy_data)(struct SequenceModifierData *smd, struct SequenceModifierData *target);

  /* apply modifier on a given image buffer */
  void (*apply)(struct SequenceModifierData *smd, struct ImBuf *ibuf, struct ImBuf *mask);
} SequenceModifierTypeInfo;

const struct SequenceModifierTypeInfo *SEQ_modifier_type_info_get(int type);
struct SequenceModifierData *SEQ_modifier_new(struct Sequence *seq, const char *name, int type);
bool SEQ_modifier_remove(struct Sequence *seq, struct SequenceModifierData *smd);
void SEQ_modifier_clear(struct Sequence *seq);
void SEQ_modifier_free(struct SequenceModifierData *smd);
void SEQ_modifier_unique_name(struct Sequence *seq, struct SequenceModifierData *smd);
struct SequenceModifierData *SEQ_modifier_find_by_name(struct Sequence *seq, const char *name);
struct ImBuf *SEQ_modifier_apply_stack(const SeqRenderData *context,
                                       struct Sequence *seq,
                                       struct ImBuf *ibuf,
                                       int timeline_frame);
void SEQ_modifier_list_copy(struct Sequence *seqn, struct Sequence *seq);
int SEQ_sequence_supports_modifiers(struct Sequence *seq);

void SEQ_modifier_blend_write(struct BlendWriter *writer, struct ListBase *modbase);
void SEQ_modifier_blend_read_data(struct BlendDataReader *reader, struct ListBase *lb);
void SEQ_modifier_blend_read_lib(struct BlendLibReader *reader,
                                 struct Scene *scene,
                                 struct ListBase *lb);

/* **********************************************************************
 * seqeffects.c
 *
 * Sequencer effect strip management functions
 *  **********************************************************************
 */

struct SeqEffectHandle {
  bool multithreaded;
  bool supports_mask;

  /* constructors & destructor */
  /* init is _only_ called on first creation */
  void (*init)(struct Sequence *seq);

  /* number of input strips needed
   * (called directly after construction) */
  int (*num_inputs)(void);

  /* load is called first time after readblenfile in
   * get_sequence_effect automatically */
  void (*load)(struct Sequence *seqconst);

  /* duplicate */
  void (*copy)(struct Sequence *dst, struct Sequence *src, const int flag);

  /* destruct */
  void (*free)(struct Sequence *seq, const bool do_id_user);

  /* returns: -1: no input needed,
   * 0: no early out,
   * 1: out = ibuf1,
   * 2: out = ibuf2 */
  int (*early_out)(struct Sequence *seq, float facf0, float facf1);

  /* stores the y-range of the effect IPO */
  void (*store_icu_yrange)(struct Sequence *seq, short adrcode, float *ymin, float *ymax);

  /* stores the default facf0 and facf1 if no IPO is present */
  void (*get_default_fac)(struct Sequence *seq, float timeline_frame, float *facf0, float *facf1);

  /* execute the effect
   * sequence effects are only required to either support
   * float-rects or byte-rects
   * (mixed cases are handled one layer up...) */

  struct ImBuf *(*execute)(const SeqRenderData *context,
                           struct Sequence *seq,
                           float timeline_frame,
                           float facf0,
                           float facf1,
                           struct ImBuf *ibuf1,
                           struct ImBuf *ibuf2,
                           struct ImBuf *ibuf3);

  struct ImBuf *(*init_execution)(const SeqRenderData *context,
                                  struct ImBuf *ibuf1,
                                  struct ImBuf *ibuf2,
                                  struct ImBuf *ibuf3);

  void (*execute_slice)(const SeqRenderData *context,
                        struct Sequence *seq,
                        float timeline_frame,
                        float facf0,
                        float facf1,
                        struct ImBuf *ibuf1,
                        struct ImBuf *ibuf2,
                        struct ImBuf *ibuf3,
                        int start_line,
                        int total_lines,
                        struct ImBuf *out);
};

struct SeqEffectHandle SEQ_effect_handle_get(struct Sequence *seq);
int SEQ_effect_get_num_inputs(int seq_type);
void SEQ_effect_text_font_unload(struct TextVars *data, const bool do_id_user);
void SEQ_effect_text_font_load(struct TextVars *data, const bool do_id_user);

/* **********************************************************************
 * sequencer.c
 *
 * Clipboard
 * **********************************************************************
 */

extern ListBase seqbase_clipboard;
extern int seqbase_clipboard_frame;
void SEQ_clipboard_pointers_store(struct Main *bmain, struct ListBase *seqbase);
void SEQ_clipboard_pointers_restore(struct ListBase *seqbase, struct Main *bmain);
void SEQ_clipboard_free(void);

/* **********************************************************************
 * sequencer.c
 *
 * Depsgraph
 * **********************************************************************
 */

/* A debug and development function which checks whether sequences have unique UUIDs.
 * Errors will be reported to the console. */
void SEQ_relations_check_uuids_unique_and_report(const struct Scene *scene);
/* Generate new UUID for the given sequence. */
void SEQ_relations_session_uuid_generate(struct Sequence *sequence);

/* **********************************************************************
 * strip_edit.c
 *
 * Editing functions
 * **********************************************************************
 */

typedef enum eSeqSplitMethod {
  SEQ_SPLIT_SOFT,
  SEQ_SPLIT_HARD,
} eSeqSplitMethod;

struct Sequence *SEQ_edit_strip_split(struct Main *bmain,
                                      struct Scene *scene,
                                      struct ListBase *seqbase,
                                      struct Sequence *seq,
                                      const int timeline_frame,
                                      const eSeqSplitMethod method);
bool SEQ_edit_remove_gaps(struct Scene *scene,
                          struct ListBase *seqbase,
                          const int initial_frame,
                          const bool remove_all_gaps);

/* **********************************************************************
 * strip_time.c
 *
 * Editing functions
 * **********************************************************************
 */

void SEQ_timeline_boundbox(const struct Scene *scene,
                           const struct ListBase *seqbase,
                           struct rctf *rect);

/* **********************************************************************
 * strip_transform.c
 *
 * Editing functions
 * **********************************************************************
 */

void SEQ_transform_offset_after_frame(struct Scene *scene,
                                      struct ListBase *seqbase,
                                      const int delta,
                                      const int timeline_frame);

#ifdef __cplusplus
}
#endif
