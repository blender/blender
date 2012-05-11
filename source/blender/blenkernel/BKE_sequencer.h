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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_SEQUENCER_H__
#define __BKE_SEQUENCER_H__

/** \file BKE_sequencer.h
 *  \ingroup bke
 */

struct bContext;
struct Editing;
struct ImBuf;
struct Main;
struct Scene;
struct Sequence;
struct Strip;
struct StripElem;
struct bSound;

struct SeqIndexBuildContext;

#define BUILD_SEQAR_COUNT_NOTHING  0
#define BUILD_SEQAR_COUNT_CURRENT  1
#define BUILD_SEQAR_COUNT_CHILDREN 2

#define EARLY_NO_INPUT		-1
#define EARLY_DO_EFFECT		0
#define EARLY_USE_INPUT_1	1
#define EARLY_USE_INPUT_2	2

/* sequence iterator */

typedef struct SeqIterator {
	struct Sequence **array;
	int tot, cur;

	struct Sequence *seq;
	int valid;
} SeqIterator;

void seq_begin(struct Editing *ed, SeqIterator *iter, int use_pointer);
void seq_next(SeqIterator *iter);
void seq_end(SeqIterator *iter);
void seq_array(struct Editing *ed, struct Sequence ***seqarray, int *tot, int use_pointer);

#define SEQP_BEGIN(ed, _seq)                                                  \
{                                                                             \
	SeqIterator iter;                                                         \
		for (seq_begin(ed, &iter, 1); iter.valid; seq_next(&iter)) {          \
			_seq = iter.seq;
			
#define SEQ_BEGIN(ed, _seq)                                                   \
	{                                                                         \
		SeqIterator iter;                                                     \
		for (seq_begin(ed, &iter, 0); iter.valid; seq_next(&iter)) {          \
			_seq = iter.seq;

#define SEQ_END                                                               \
		}                                                                     \
		seq_end(&iter);                                                       \
	}

typedef struct SeqRenderData {
	struct Main *bmain;
	struct Scene *scene;
	int rectx;
	int recty;
	int preview_render_size;
	int motion_blur_samples;
	float motion_blur_shutter;
} SeqRenderData;

SeqRenderData seq_new_render_data(
	struct Main * bmain, struct Scene * scene,
	int rectx, int recty, int preview_render_size);

int seq_cmp_render_data(const SeqRenderData * a, const SeqRenderData * b);
unsigned int seq_hash_render_data(const SeqRenderData * a);

/* Wipe effect */
enum {
	DO_SINGLE_WIPE,
	DO_DOUBLE_WIPE,
	DO_BOX_WIPE,
	DO_CROSS_WIPE,
	DO_IRIS_WIPE,
	DO_CLOCK_WIPE
};


struct SeqEffectHandle {
	/* constructors & destructor */
	/* init & init_plugin are _only_ called on first creation */
	void (*init)(struct Sequence *seq);
	void (*init_plugin)(struct Sequence *seq, const char *fname);
	
	/* number of input strips needed 
	 * (called directly after construction) */
	int (*num_inputs)(void);
	
	/* load is called first time after readblenfile in
	 * get_sequence_effect automatically */
	void (*load)(struct Sequence *seq);
	
	/* duplicate */
	void (*copy)(struct Sequence *dst, struct Sequence *src);
	
	/* destruct */
	void (*free)(struct Sequence *seq);
	
	/* returns: -1: no input needed,
	 * 0: no early out,
	 * 1: out = ibuf1,
	 * 2: out = ibuf2 */
	int (*early_out)(struct Sequence *seq, float facf0, float facf1); 
	
	/* stores the y-range of the effect IPO */
	void (*store_icu_yrange)(struct Sequence * seq, short adrcode, float *ymin, float *ymax);
	
	/* stores the default facf0 and facf1 if no IPO is present */
	void (*get_default_fac)(struct Sequence *seq, float cfra, float * facf0, float * facf1);
	
	/* execute the effect
	 * sequence effects are only required to either support
	 * float-rects or byte-rects
	 * (mixed cases are handled one layer up...) */
	
	struct ImBuf* (*execute)(
		SeqRenderData context,
		struct Sequence *seq, float cfra,
		float facf0, float facf1,
		struct ImBuf *ibuf1, struct ImBuf *ibuf2,
		struct ImBuf *ibuf3);
};

/* ********************* prototypes *************** */

/* **********************************************************************
 * sequence.c
 *
 * sequencer render functions
 * ********************************************************************** */

struct ImBuf *give_ibuf_seq(SeqRenderData context, float cfra, int chanshown);
struct ImBuf *give_ibuf_seq_threaded(SeqRenderData context, float cfra, int chanshown);
struct ImBuf *give_ibuf_seq_direct(SeqRenderData context, float cfra, struct Sequence *seq);
struct ImBuf *give_ibuf_seqbase(SeqRenderData context, float cfra, int chan_shown, struct ListBase *seqbasep);
void give_ibuf_prefetch_request(SeqRenderData context, float cfra, int chan_shown);


/* **********************************************************************
 * sequencer scene functions
 * ********************************************************************** */
struct Editing  *BKE_sequencer_editing_get(struct Scene *scene, int alloc);
struct Editing  *BKE_sequencer_editing_ensure(struct Scene *scene);
void             BKE_sequencer_editing_free(struct Scene *scene);

void             BKE_sequencer_sort(struct Scene *scene);

struct Sequence *BKE_sequencer_active_get(struct Scene *scene);
int              BKE_sequencer_active_get_pair(struct Scene *scene,
                                               struct Sequence **seq_act, struct Sequence **seq_other);
void             BKE_sequencer_active_set(struct Scene *scene, struct Sequence *seq);


/* apply functions recursively */
int seqbase_recursive_apply(struct ListBase *seqbase, int (*apply_func)(struct Sequence *seq, void *), void *arg);
int seq_recursive_apply(struct Sequence *seq, int (*apply_func)(struct Sequence *, void *), void *arg);

/* maintenance functions, mostly for RNA */
// extern 
void seq_free_sequence(struct Scene *scene, struct Sequence *seq);
void seq_free_sequence_recurse(struct Scene *scene, struct Sequence *seq);
void seq_free_strip(struct Strip *strip);

void seq_free_clipboard(void);
const char *give_seqname(struct Sequence *seq);
void calc_sequence(struct Scene *scene, struct Sequence *seq);
void calc_sequence_disp(struct Scene *scene, struct Sequence *seq);
void reload_sequence_new_file(struct Scene *scene, struct Sequence * seq, int lock_range);
void build_seqar_cb(struct ListBase *seqbase, struct Sequence  ***seqar, int *totseq,
					int (*test_func)(struct Sequence * seq));
int evaluate_seq_frame(struct Scene *scene, int cfra);
struct StripElem *give_stripelem(struct Sequence *seq, int cfra);

// intern
void printf_strip(struct Sequence *seq); // debugging function (unused)
void update_changed_seq_and_deps(struct Scene *scene, struct Sequence *changed_seq, int len_change, int ibuf_change);

int input_have_to_preprocess(
	SeqRenderData context, struct Sequence * seq, float cfra);

struct SeqIndexBuildContext *seq_proxy_rebuild_context(struct Main *bmain, struct Scene *scene, struct Sequence *seq);
void seq_proxy_rebuild(struct SeqIndexBuildContext *context,
                       short *stop, short *do_update, float *progress);
void seq_proxy_rebuild_finish(struct SeqIndexBuildContext *context, short stop);


/* **********************************************************************
 * seqcache.c
 *
 * Sequencer memory cache management functions
 * ********************************************************************** */

typedef enum {
	SEQ_STRIPELEM_IBUF,
	SEQ_STRIPELEM_IBUF_COMP,
	SEQ_STRIPELEM_IBUF_STARTSTILL,
	SEQ_STRIPELEM_IBUF_ENDSTILL
} seq_stripelem_ibuf_t;

void seq_stripelem_cache_destruct(void);
void seq_stripelem_cache_cleanup(void);

/* returned ImBuf is properly refed and has to be freed */
struct ImBuf * seq_stripelem_cache_get(
	SeqRenderData context, struct Sequence * seq, 
	float cfra, seq_stripelem_ibuf_t type);

/* passed ImBuf is properly refed, so ownership is *not* 
 * transfered to the cache.
 * you can pass the same ImBuf multiple times to the cache without problems.
 */
   
void seq_stripelem_cache_put(
	SeqRenderData context, struct Sequence * seq, 
	float cfra, seq_stripelem_ibuf_t type, struct ImBuf * nval);

/* **********************************************************************
 * seqeffects.c
 *
 * Sequencer effect strip managment functions
 *  **********************************************************************
 */

/* intern */
struct SeqEffectHandle get_sequence_blend(struct Sequence *seq);
void sequence_effect_speed_rebuild_map(struct Scene *scene, struct Sequence *seq, int force);

/* extern */
struct SeqEffectHandle get_sequence_effect(struct Sequence *seq);
int get_sequence_effect_num_inputs(int seq_type);


/* **********************************************************************
 * Sequencer editing functions
 * **********************************************************************
 */
   
/* for transform but also could use elsewhere */
int seq_tx_get_start(struct Sequence *seq);
int seq_tx_get_end(struct Sequence *seq);
int seq_tx_get_final_left(struct Sequence *seq, int metaclip);
int seq_tx_get_final_right(struct Sequence *seq, int metaclip);
void seq_tx_set_final_left(struct Sequence *seq, int val);
void seq_tx_set_final_right(struct Sequence *seq, int val);
void seq_tx_handle_xlimits(struct Sequence *seq, int leftflag, int rightflag);
int seq_tx_test(struct Sequence * seq);
int seq_single_check(struct Sequence *seq);
void seq_single_fix(struct Sequence *seq);
int seq_test_overlap(struct ListBase * seqbasep, struct Sequence *test);
void seq_translate(struct Scene *scene, struct Sequence *seq, int delta);
void seq_sound_init(struct Scene *scene, struct Sequence *seq);
struct Sequence *seq_foreground_frame_get(struct Scene *scene, int frame);
struct ListBase *seq_seqbase(struct ListBase *seqbase, struct Sequence *seq);
struct Sequence *seq_metastrip(
	ListBase * seqbase /* = ed->seqbase */, 
	struct Sequence * meta /* = NULL */, struct Sequence *seq);

void seq_offset_animdata(struct Scene *scene, struct Sequence *seq, int ofs);
void seq_dupe_animdata(struct Scene *scene, const char *name_src, const char *name_dst);
int shuffle_seq(struct ListBase * seqbasep, struct Sequence *test, struct Scene *evil_scene);
int shuffle_seq_time(ListBase * seqbasep, struct Scene *evil_scene);
int seqbase_isolated_sel_check(struct ListBase *seqbase);
void free_imbuf_seq(struct Scene *scene, struct ListBase * seqbasep, int check_mem_usage, int keep_file_handles);
struct Sequence	*seq_dupli_recursive(struct Scene *scene, struct Scene *scene_to, struct Sequence * seq, int dupe_flag);
int seq_swap(struct Sequence *seq_a, struct Sequence *seq_b, const char **error_str);

void seq_update_sound_bounds_all(struct Scene *scene);
void seq_update_sound_bounds(struct Scene* scene, struct Sequence *seq);
void seq_update_muting(struct Editing *ed);
void seq_update_sound(struct Scene *scene, struct bSound *sound);
void seqbase_unique_name_recursive(ListBase *seqbasep, struct Sequence *seq);
void seqbase_dupli_recursive(struct Scene *scene, struct Scene *scene_to, ListBase *nseqbase, ListBase *seqbase, int dupe_flag);

void clear_scene_in_allseqs(struct Main *bmain, struct Scene *sce);

struct Sequence *get_seq_by_name(struct ListBase *seqbase, const char *name, int recursive);

/* api for adding new sequence strips */
typedef struct SeqLoadInfo {
	int start_frame;
	int end_frame;
	int channel;
	int flag;	/* use sound, replace sel */
	int type;
	int tot_success;
	int tot_error;
	int len;		/* only for image strips */
	char path[512];
	char name[64];
} SeqLoadInfo;

/* SeqLoadInfo.flag */
#define SEQ_LOAD_REPLACE_SEL	(1<<0)
#define SEQ_LOAD_FRAME_ADVANCE	(1<<1)
#define SEQ_LOAD_MOVIE_SOUND	(1<<2)
#define SEQ_LOAD_SOUND_CACHE	(1<<3)


/* seq_dupli' flags */
#define SEQ_DUPE_UNIQUE_NAME	(1<<0)
#define SEQ_DUPE_CONTEXT		(1<<1)
#define SEQ_DUPE_ANIM			(1<<2)
#define SEQ_DUPE_ALL			(1<<3) /* otherwise only selected are copied */

/* use as an api function */
typedef struct Sequence *(*SeqLoadFunc)(struct bContext *, ListBase *, struct SeqLoadInfo *);

struct Sequence *alloc_sequence(ListBase *lb, int cfra, int machine);

void seq_load_apply(struct Scene *scene, struct Sequence *seq, struct SeqLoadInfo *seq_load);

struct Sequence *sequencer_add_image_strip(struct bContext *C, ListBase *seqbasep, struct SeqLoadInfo *seq_load);
struct Sequence *sequencer_add_sound_strip(struct bContext *C, ListBase *seqbasep, struct SeqLoadInfo *seq_load);
struct Sequence *sequencer_add_movie_strip(struct bContext *C, ListBase *seqbasep, struct SeqLoadInfo *seq_load);

/* view3d draw callback, run when not in background view */
typedef struct ImBuf *(*SequencerDrawView)(struct Scene *, struct Object *, int, int, unsigned int, int, int, char[256]);
extern SequencerDrawView sequencer_view3d_cb;

/* copy/paste */
extern ListBase seqbase_clipboard;
extern int seqbase_clipboard_frame;

#endif // __BKE_SEQUENCER_H__
