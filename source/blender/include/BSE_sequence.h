/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#ifndef BSE_SEQUENCE_H
#define BSE_SEQUENCE_H

struct PluginSeq;
struct StripElem;
struct Strip;
struct Sequence;
struct ListBase;
struct Editing;
struct ImBuf;
struct Scene;

void open_plugin_seq(struct PluginSeq *pis, char *seqname);
struct PluginSeq *add_plugin_seq(char *str, char *seqname);
void free_plugin_seq(struct PluginSeq *pis);
void free_stripdata(int len, struct StripElem *se);
void free_strip(struct Strip *strip);
void new_stripdata(struct Sequence *seq);
void free_sequence(struct Sequence *seq);
void do_seq_count(struct ListBase *seqbase, int *totseq);
void do_build_seqar(struct ListBase *seqbase, struct Sequence ***seqar, int depth);
void build_seqar(struct ListBase *seqbase, struct Sequence  ***seqar, int *totseq);
void free_editing(struct Editing *ed);
void calc_sequence(struct Sequence *seq);
void sort_seq(void);
void clear_scene_in_allseqs(struct Scene *sce);
void do_alphaover_effect(float facf0,
						 float facf1,
						 int x, int y,
						 unsigned int *rect1,
						 unsigned int *rect2,
						 unsigned int *out);
void do_alphaunder_effect(float facf0, float facf1,
						  int x, int y,
						  unsigned int *rect1, unsigned int *rect2,
						  unsigned int *out);
void do_cross_effect(float facf0, float facf1,
					 int x, int y,
					 unsigned int *rect1, unsigned int *rect2,
					 unsigned int *out);
void do_gammacross_effect(float facf0, float facf1,
						  int x, int y,
						  unsigned int *rect1, unsigned int *rect2,
						  unsigned int *out);
void do_add_effect(float facf0, float facf1,
				   int x, int y,
				   unsigned int *rect1, unsigned int *rect2,
				   unsigned int *out);
void do_sub_effect(float facf0, float facf1,
				   int x, int y,
				   unsigned int *rect1, unsigned int *rect2,
				   unsigned int *out);
void do_drop_effect(float facf0, float facf1,
					int x, int y,
					unsigned int *rect2i, unsigned int *rect1i,
					unsigned int *outi);
void do_drop_effect2(float facf0, float facf1,
					 int x, int y,
					 unsigned int *rect2, unsigned int *rect1,
					 unsigned int *out);
void do_mul_effect(float facf0, float facf1,
				   int x, int y,
				   unsigned int *rect1, unsigned int *rect2,
				   unsigned int *out);
/* Sweep effect */
enum {DO_LEFT_RIGHT, DO_RIGHT_LEFT, DO_DOWN_UP, DO_UP_DOWN,
      DO_LOWER_LEFT_UPPER_RIGHT, DO_UPPER_RIGHT_LOWER_LEFT,
      DO_UPPER_LEFT_LOWER_RIGHT, DO_LOWER_RIGHT_UPPER_LEFT,
      DO_HORZ_OUT, DO_HORZ_IN, DO_VERT_OUT, DO_VERT_IN,
      DO_HORZ_VERT_OUT, DO_HORZ_VERT_IN, DO_LEFT_DOWN_RIGHT_UP_OUT,
      DO_LEFT_DOWN_RIGHT_UP_IN, DO_LEFT_UP_RIGHT_DOWN_OUT,
      DO_LEFT_UP_RIGHT_DOWN_IN, DO_DIAG_OUT, DO_DIAG_IN, DO_DIAG_OUT_2,
      DO_DIAG_IN_2};
int check_zone(int x, int y, int xo, int yo, struct Sequence *seq, float facf0);
void init_sweep_effect(struct Sequence *seq);
void do_sweep_effect(struct Sequence *seq, float facf0, float facf1, int x, int y, unsigned int *rect1, unsigned int *rect2, unsigned int *out);

/* Glow effect */
enum {
	GlowR=0,
	GlowG=1,
	GlowB=2,
	GlowA=3
};
void RVBlurBitmap2( unsigned char* map, int width, int height, float blur, int quality);
void RVIsolateHighlights (unsigned char* in, unsigned char* out, int width, int height, int threshold, float boost, float clamp);
void RVAddBitmaps (unsigned char* a,unsigned char* b, unsigned char* c, int width, int height);
void init_glow_effect(struct Sequence *seq);
void do_glow_effect(struct Sequence *seq, float facf0, float facf1, int x, int y, unsigned int *rect1, unsigned int *rect2, unsigned int *out);

void make_black_ibuf(struct ImBuf *ibuf);
void multibuf(struct ImBuf *ibuf, float fmul);
void do_effect(int cfra, struct Sequence *seq, struct StripElem *se);
int evaluate_seq_frame(int cfra);
struct StripElem *give_stripelem(struct Sequence *seq, int cfra);
void set_meta_stripdata(struct Sequence *seqm);
void do_seq_count_cfra(struct ListBase *seqbase, int *totseq, int cfra);
void do_build_seqar_cfra(struct ListBase *seqbase, struct Sequence ***seqar, int cfra);
struct ImBuf *give_ibuf_seq(int cfra);
void free_imbuf_effect_spec(int cfra);
void free_imbuf_seq_except(int cfra);
void free_imbuf_seq(void);
void do_render_seq(void);


#endif

