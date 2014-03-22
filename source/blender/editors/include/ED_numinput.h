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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_numinput.h
 *  \ingroup editors
 */

#ifndef __ED_NUMINPUT_H__
#define __ED_NUMINPUT_H__

#define NUM_STR_REP_LEN 64
#define NUM_MAX_ELEMENTS 3

typedef struct NumInput {
	short  idx_max;                      /* idx_max < NUM_MAX_ELEMENTS */
	int    unit_sys;
	int    unit_type[NUM_MAX_ELEMENTS];  /* Each value can have a different type */
	bool   unit_use_radians;

	short  flag;                         /* Flags affecting all values' behavior */
	short  val_flag[NUM_MAX_ELEMENTS];   /* Per-value flags */
	float  val[NUM_MAX_ELEMENTS];        /* Direct value of the input */
	float  val_org[NUM_MAX_ELEMENTS];    /* Original value of the input, for reset */
	float  val_inc[NUM_MAX_ELEMENTS];    /* Increment steps */

	short  idx;                          /* Active element/value */
	char   str[NUM_STR_REP_LEN];         /* String as typed by user for edited value (we assume ASCII world!) */
	/* Current position of cursor in edited value str (first byte of "current" letter, so 0 for an empty str) */
	int    str_cur;
} NumInput;

/* NumInput.flag */
enum {
	NUM_AFFECT_ALL      = (1 << 0),
	/* (1 << 9) and above are reserved for internal flags! */
};

/* NumInput.val_flag[] */
enum {
	/* Public! */
	NUM_NULL_ONE        = (1 << 0),
	NUM_NO_NEGATIVE     = (1 << 1),
	NUM_NO_ZERO         = (1 << 2),
	NUM_NO_FRACTION     = (1 << 3),
	/* (1 << 9) and above are reserved for internal flags! */
};

/*********************** NumInput ********************************/

/* There are important things to note here for code using numinput:
 * * Values passed to applyNumInput() should be valid and are stored as default ones (val_org), if it is not EDITED.
 * * bool returned by applyNumInput should be used to decide whether to apply numinput-specific post-process to data.
 * * *Once applyNumInput has been called*, hasNumInput returns a valid value to decide whether to use numinput
 *   as drawstr source or not (i.e. to call outputNumInput).
 *
 * Those two steps have to be separated (so do not use a common call to hasNumInput() to do both in the same time!).
 */

void initNumInput(NumInput *n);
void outputNumInput(NumInput *n, char *str);
bool hasNumInput(const NumInput *n);
bool applyNumInput(NumInput *n, float *vec);
bool handleNumInput(struct bContext *C, NumInput *n, const struct wmEvent *event);

#define NUM_MODAL_INCREMENT_UP   18
#define NUM_MODAL_INCREMENT_DOWN 19

#endif  /* __ED_NUMINPUT_H__ */
