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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef BSE_SEQUENCE_H
#define BSE_SEQUENCE_H


struct PluginSeq;
struct StripElem;
struct TStripElem;
struct Strip;
struct Sequence;
struct ListBase;
struct Editing;
struct ImBuf;
struct Scene;

void free_tstripdata(int len, struct TStripElem *se);
void free_strip(struct Strip *strip);
void new_tstripdata(struct Sequence *seq);
void free_sequence(struct Sequence *seq);
void build_seqar(struct ListBase *seqbase, struct Sequence  ***seqar, int *totseq);

#define BUILD_SEQAR_COUNT_NOTHING  0
#define BUILD_SEQAR_COUNT_CURRENT  1
#define BUILD_SEQAR_COUNT_CHILDREN 2

void build_seqar_cb(struct ListBase *seqbase, struct Sequence  ***seqar, 
		    int *totseq, int (*test_func)(struct Sequence * seq));
void free_editing(struct Editing *ed);
void calc_sequence(struct Sequence *seq);
void calc_sequence_disp(struct Sequence *seq);
void reload_sequence_new_file(struct Sequence * seq);
void seq_proxy_rebuild(struct Sequence * seq);
void sort_seq(void);
void clear_scene_in_allseqs(struct Scene *sce);

char *give_seqname_by_type(int type);
char *give_seqname(struct Sequence *seq);

int evaluate_seq_frame(int cfra);
struct StripElem *give_stripelem(struct Sequence *seq, int cfra);
struct TStripElem *give_tstripelem(struct Sequence *seq, int cfra);
struct ImBuf *give_ibuf_seq(int rectx, int recty, int cfra, int chansel); 
/* chansel: render this channel. Default=0 (renders end result)*/
struct ImBuf *give_ibuf_seq_direct(int rectx, int recty, int cfra,
				   struct Sequence * seq);

/* sequence prefetch API */
void seq_start_threads();
void seq_stop_threads();
void give_ibuf_prefetch_request(int rectx, int recty, int cfra, int chanshown);
void seq_wait_for_prefetch_ready();
struct ImBuf * give_ibuf_seq_threaded(int rectx, int recty, int cfra, 
				      int chanshown);


void free_imbuf_seq_except(int cfra);
void free_imbuf_seq_with_ipo(struct Ipo * ipo);
void free_imbuf_seq(void);

void update_changed_seq_and_deps(struct Sequence *seq, int len_change, int ibuf_change);

/* still bad level call... */
struct RenderResult;
void do_render_seq(struct RenderResult *rr, int cfra);

#define SEQ_HAS_PATH(seq) (seq->type==SEQ_MOVIE || seq->type==SEQ_HD_SOUND || seq->type==SEQ_RAM_SOUND || seq->type==SEQ_IMAGE)

#endif

