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
 */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_STR_REP_LEN 64
#define NUM_MAX_ELEMENTS 3

struct wmEvent;

typedef struct NumInput {
  /** idx_max < NUM_MAX_ELEMENTS */
  short idx_max;
  int unit_sys;
  /** Each value can have a different type */
  int unit_type[NUM_MAX_ELEMENTS];
  bool unit_use_radians;

  /** Flags affecting all values' behavior */
  short flag;
  /** Per-value flags */
  short val_flag[NUM_MAX_ELEMENTS];
  /** Direct value of the input */
  float val[NUM_MAX_ELEMENTS];
  /** Original value of the input, for reset */
  float val_org[NUM_MAX_ELEMENTS];
  /** Increment steps */
  float val_inc[NUM_MAX_ELEMENTS];

  /** Active element/value */
  short idx;
  /** String as typed by user for edited value (we assume ASCII world!) */
  char str[NUM_STR_REP_LEN];
  /** Current position of cursor in edited value str
   * (first byte of "current" letter, so 0 for an empty str) */
  int str_cur;
} NumInput;

/* NumInput.flag */
enum {
  NUM_AFFECT_ALL = (1 << 0),
  /* (1 << 9) and above are reserved for internal flags! */
};

/* NumInput.val_flag[] */
enum {
  /* Public! */
  NUM_NULL_ONE = (1 << 0),
  NUM_NO_NEGATIVE = (1 << 1),
  NUM_NO_ZERO = (1 << 2),
  NUM_NO_FRACTION = (1 << 3),
  /* (1 << 9) and above are reserved for internal flags! */
};

struct UnitSettings;

/* -------------------------------------------------------------------- */
/** \name NumInput
 * \{ */

/**
 * There are important things to note here for code using numinput:
 * - Values passed to #applyNumInput() should be valid and are stored as default ones (val_org),
 *   if it is not EDITED.
 * - bool returned by #applyNumInput should be used to decide whether to apply
 *   numinput-specific post-process to data.
 * - Once #applyNumInput has been called,
 *   #hasNumInput returns a valid value to decide whether to use numinput as drawstr source or not
 *   (i.e. to call #outputNumInput).
 *
 * Those two steps have to be separated
 * (so do not use a common call to #hasNumInput() to do both in the same time!).
 */

void initNumInput(NumInput *n);
/**
 * \param str: Must be NUM_STR_REP_LEN * (idx_max + 1) length.
 */
void outputNumInput(NumInput *n, char *str, struct UnitSettings *unit_settings);
bool hasNumInput(const NumInput *n);
/**
 * \warning \a vec must be set beforehand otherwise we risk uninitialized vars.
 */
bool applyNumInput(NumInput *n, float *vec);
bool handleNumInput(struct bContext *C, NumInput *n, const struct wmEvent *event);

/** Share with `TFM_MODAL_CANCEL` in `transform.h`. */
#define NUM_MODAL_INCREMENT_UP 18
#define NUM_MODAL_INCREMENT_DOWN 19

bool user_string_to_number(bContext *C,
                           const char *str,
                           const struct UnitSettings *unit,
                           int type,
                           double *r_value,
                           bool use_single_line_error,
                           char **r_error);

/** \} */

#ifdef __cplusplus
}
#endif
