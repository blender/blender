/**
 * $Id: BSE_seqeffects.h 9554 2006-12-31 15:38:14Z schlaile $
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
 * Contributor(s): Peter Schlaile < peter [at] schlaile [dot] de >
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#ifndef BSE_SEQUENCE_EFFECTS_H
#define BSE_SEQUENCE_EFFECTS_H

/* Wipe effect */
enum {DO_SINGLE_WIPE, DO_DOUBLE_WIPE, DO_BOX_WIPE, DO_CROSS_WIPE,
      DO_IRIS_WIPE,DO_CLOCK_WIPE};

struct Sequence;
struct ImBuf;

struct SeqEffectHandle {
	/* constructors & destructor */
	/* init & init_plugin are _only_ called on first creation */
	void (*init)(struct Sequence *seq);
	void (*init_plugin)(struct Sequence * seq, const char * fname);

	/* number of input strips needed 
	   (called directly after construction) */
	int (*num_inputs)();

        /* load is called first time after readblenfile in
           get_sequence_effect automatically */
	void (*load)(struct Sequence *seq);

	/* duplicate */
	void (*copy)(struct Sequence *dst, struct Sequence * src);

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
				 short adrcode, float * ymin, float * ymax);

	/* stores the default facf0 and facf1 if no IPO is present */
	void (*get_default_fac)(struct Sequence * seq, int cfra,
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

struct SeqEffectHandle get_sequence_effect(struct Sequence * seq);
int get_sequence_effect_num_inputs(int seq_type);
void sequence_effect_speed_rebuild_map(struct Sequence * seq, int force);

#endif

