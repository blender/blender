/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_SEQUENCE_H
#define BKE_SEQUENCE_H

struct Editing;
struct Sequence;
struct Strip;
struct StripElem;
struct ImBuf;
struct Scene;
struct bContext;

#define MAXSEQ          32

#define BUILD_SEQAR_COUNT_NOTHING  0
#define BUILD_SEQAR_COUNT_CURRENT  1
#define BUILD_SEQAR_COUNT_CHILDREN 2

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

#define SEQP_BEGIN(ed, seq) \
{ \
	SeqIterator iter;\
		for(seq_begin(ed, &iter, 1); iter.valid; seq_next(&iter)) { \
			seq= iter.seq;
			
#define SEQ_BEGIN(ed, seq) \
	{ \
		SeqIterator iter;\
		for(seq_begin(ed, &iter, 0); iter.valid; seq_next(&iter)) { \
			seq= iter.seq;

#define SEQ_END \
		} \
		seq_end(&iter); \
	}

#endif


/* Wipe effect */
enum {DO_SINGLE_WIPE, DO_DOUBLE_WIPE, DO_BOX_WIPE, DO_CROSS_WIPE,
	DO_IRIS_WIPE,DO_CLOCK_WIPE};


struct SeqEffectHandle {
	/* constructors & destructor */
	/* init & init_plugin are _only_ called on first creation */
	void (*init)(struct Sequence *seq);
	void (*init_plugin)(struct Sequence *seq, const char *fname);
	
	/* number of input strips needed 
		(called directly after construction) */
	int (*num_inputs)();
	
	/* load is called first time after readblenfile in
		get_sequence_effect automatically */
	void (*load)(struct Sequence *seq);
	
	/* duplicate */
	void (*copy)(struct Sequence *dst, struct Sequence *src);
	
	/* destruct */
	void (*free)(struct Sequence *seq);
	
	/* returns: -1: no input needed,
	0: no early out, 
	1: out = ibuf1, 
	2: out = ibuf2 */
	int (*early_out)(struct Sequence *seq,
					 float facf0, float facf1); 
	
	/* stores the y-range of the effect IPO */
	void (*store_icu_yrange)(struct Sequence * seq,
							 short adrcode, float *ymin, float *ymax);
	
	/* stores the default facf0 and facf1 if no IPO is present */
	void (*get_default_fac)(struct Sequence *seq, int cfra,
							float * facf0, float * facf1);
	
	/* execute the effect
		sequence effects are only required to either support
		float-rects or byte-rects 
		(mixed cases are handled one layer up...) */
	
	void (*execute)(struct Sequence *seq, int cfra,
					float facf0, float facf1,
					int x, int y,
					struct ImBuf *ibuf1, struct ImBuf *ibuf2,
					struct ImBuf *ibuf3, struct ImBuf *out);
};

/* ********************* prototypes *************** */

/* sequence.c */
void printf_strip(struct Sequence *seq);

// extern
void seq_free_sequence(struct Scene *scene, struct Sequence *seq);
void seq_free_strip(struct Strip *strip);
void seq_free_editing(struct Scene *scene);
struct Editing *seq_give_editing(struct Scene *scene, int alloc);
char *give_seqname(struct Sequence *seq);
struct ImBuf *give_ibuf_seq(struct Scene *scene, int rectx, int recty, int cfra, int chanshown, int render_size);
struct ImBuf *give_ibuf_seq_threaded(struct Scene *scene, int rectx, int recty, int cfra, int chanshown, int render_size);
struct ImBuf *give_ibuf_seq_direct(struct Scene *scene, int rectx, int recty, int cfra, int render_size, struct Sequence *seq);
void give_ibuf_prefetch_request(int rectx, int recty, int cfra, int chanshown, int render_size);
void calc_sequence(struct Sequence *seq);
void calc_sequence_disp(struct Sequence *seq);
void new_tstripdata(struct Sequence *seq);
void reload_sequence_new_file(struct Scene *scene, struct Sequence * seq);
void sort_seq(struct Scene *scene);
void build_seqar_cb(struct ListBase *seqbase, struct Sequence  ***seqar, int *totseq,
					int (*test_func)(struct Sequence * seq));
int evaluate_seq_frame(struct Scene *scene, int cfra);
struct StripElem *give_stripelem(struct Sequence *seq, int cfra);

// intern?
void update_changed_seq_and_deps(struct Scene *scene, struct Sequence *changed_seq, int len_change, int ibuf_change);

/* seqeffects.c */
// intern?
struct SeqEffectHandle get_sequence_blend(struct Sequence *seq);
void sequence_effect_speed_rebuild_map(struct Sequence *seq, int force);

// extern
struct SeqEffectHandle get_sequence_effect(struct Sequence *seq);
int get_sequence_effect_num_inputs(int seq_type);

/* for transform but also could use elsewhere */
int seq_tx_get_start(struct Sequence *seq);
int seq_tx_get_end(struct Sequence *seq);
int seq_tx_get_final_left(struct Sequence *seq, int metaclip);
int seq_tx_get_final_right(struct Sequence *seq, int metaclip);
void seq_tx_set_final_left(struct Sequence *seq, int val);
void seq_tx_set_final_right(struct Sequence *seq, int val);
void seq_tx_handle_xlimits(struct Sequence *seq, int leftflag, int rightflag);
int seq_tx_test(struct Sequence * seq);
int check_single_seq(struct Sequence *seq);
void fix_single_seq(struct Sequence *seq);
int seq_test_overlap(struct ListBase * seqbasep, struct Sequence *test);
int shuffle_seq(struct ListBase * seqbasep, struct Sequence *test);
int shuffle_seq_time(ListBase * seqbasep);
void free_imbuf_seq(struct ListBase * seqbasep, int check_mem_usage);

void seq_update_sound(struct Sequence *seq);

void clear_scene_in_allseqs(struct Scene *sce);


