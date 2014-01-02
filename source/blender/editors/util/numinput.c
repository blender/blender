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
 * Contributor(s): Jonathan Smith
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/numinput.c
 *  \ingroup edutil
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_cursor_utf8.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "ED_numinput.h"


/* NumInput.val_flag[] */
enum {
	/* (1 << 8) and below are reserved for public flags! */
	NUM_EDITED          = (1 << 9),    /* User has edited this value somehow. */
	NUM_INVALID         = (1 << 10),   /* Current expression for this value is invalid. */
};

/* ************************** Functions *************************** */

/* ************************** NUMINPUT **************************** */

void initNumInput(NumInput *n)
{
	n->unit_sys = USER_UNIT_NONE;
	n->unit_type[0] = n->unit_type[1] = n->unit_type[2] = B_UNIT_NONE;
	n->idx = 0;
	n->idx_max = 0;
	n->flag = 0;
	n->val_flag[0] = n->val_flag[1] = n->val_flag[2] = 0;
	zero_v3(n->val_org);
	zero_v3(n->val);
	n->str[0] = '\0';
	n->str_cur = 0;
	copy_v3_fl(n->val_inc, 1.0f);
}

/* str must be NUM_STR_REP_LEN * (idx_max + 1) length. */
void outputNumInput(NumInput *n, char *str)
{
	short i, j;
	const int ln = NUM_STR_REP_LEN;
	const int prec = 4; /* draw-only, and avoids too much issues with radian->degrees conversion. */

	for (j = 0; j <= n->idx_max; j++) {
		/* if AFFECTALL and no number typed and cursor not on number, use first number */
		i = (n->flag & NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;

		if (n->val_flag[i] & NUM_EDITED) {
			if (i == n->idx && n->str[0]) {
				char before_cursor[NUM_STR_REP_LEN];
				char val[16];
				if (n->val_flag[i] & NUM_INVALID) {
					BLI_strncpy(val, "Invalid", sizeof(val));
				}
				else {
					bUnit_AsString(val, sizeof(val), (double)n->val[i], prec,
					               n->unit_sys, n->unit_type[i], true, false);
				}
				BLI_strncpy(before_cursor, n->str, n->str_cur + 1);  /* +1 because of trailing '\0' */
				BLI_snprintf(&str[j * ln], ln, "[%s|%s] = %s", before_cursor, &n->str[n->str_cur], val);
			}
			else {
				const char *cur = (i == n->idx) ? "|" : "";
				if (n->unit_use_radians && n->unit_type[i] == B_UNIT_ROTATION) {
					/* Radian exception... */
					BLI_snprintf(&str[j * ln], ln, "%s%.6gr%s", cur, n->val[i], cur);
				}
				else {
					char tstr[NUM_STR_REP_LEN];
					bUnit_AsString(tstr, ln, (double)n->val[i], prec, n->unit_sys, n->unit_type[i], true, false);
					BLI_snprintf(&str[j * ln], ln, "%s%s%s", cur, tstr, cur);
				}
			}
		}
		else {
			const char *cur = (i == n->idx) ? "|" : "";
			BLI_snprintf(&str[j * ln], ln, "%sNONE%s", cur, cur);
		}
	}
}

bool hasNumInput(const NumInput *n)
{
	short i;

	for (i = 0; i <= n->idx_max; i++) {
		if (n->val_flag[i] & NUM_EDITED) {
			return true;
		}
	}

	return false;
}

/**
 * \warning \a vec must be set beforehand otherwise we risk uninitialized vars.
 */
void applyNumInput(NumInput *n, float *vec)
{
	short i, j;
	float val;

	if (hasNumInput(n)) {
		for (j = 0; j <= n->idx_max; j++) {
			/* if AFFECTALL and no number typed and cursor not on number, use first number */
			i = (n->flag & NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;
			val = (!(n->val_flag[i] & NUM_EDITED) && n->val_flag[i] & NUM_NULL_ONE) ? 1.0f : n->val[i];

			if (n->val_flag[i] & NUM_NO_NEGATIVE && val < 0.0f) {
				val = 0.0f;
			}
			if (n->val_flag[i] & NUM_NO_ZERO && val == 0.0f) {
				val = 0.0001f;
			}
			if (n->val_flag[i] & NUM_NO_FRACTION && val != floorf(val)) {
				val = floorf(val + 0.5f);
				if (n->val_flag[i] & NUM_NO_ZERO && val == 0.0f) {
					val = 1.0f;
				}
			}
			vec[j] = val;
		}
	}
}


static void value_to_editstr(NumInput *n, int idx)
{
	const int prec = 6; /* editing, higher precision needed. */
	bUnit_AsString(n->str, NUM_STR_REP_LEN, (double)n->val[idx], prec,
	               n->unit_sys, n->unit_type[idx], true, false);
	n->str_cur = strlen(n->str);
}

static bool editstr_insert_at_cursor(NumInput *n, const char *buf, const int buf_len)
{
	int cur = n->str_cur;
	int len = strlen(&n->str[cur]) + 1;  /* +1 for the trailing '\0'. */
	int n_cur = cur + buf_len;

	if (n_cur + len >= NUM_STR_REP_LEN) {
		return false;
	}

	memmove(&n->str[n_cur], &n->str[cur], len);
	memcpy(&n->str[cur], buf, sizeof(char) * buf_len);

	n->str_cur = n_cur;
	return true;
}

#define NUM_REVERSE_START "-("
#define NUM_REVERSE_END ")"
#define NUM_INVERSE_START "1/("
#define NUM_INVERSE_END ")"

static bool editstr_reverse_inverse_toggle(NumInput *n, const bool reverse, const bool inverse)
{
	/* This function just add or remove -(...) or 1/(...) around current expression. */
	size_t len = strlen(n->str);
	const size_t len_rev_start = strlen(NUM_REVERSE_START);
	const size_t len_rev_end = strlen(NUM_REVERSE_END);
	const size_t len_inv_start = strlen(NUM_INVERSE_START);
	const size_t len_inv_end = strlen(NUM_INVERSE_END);
	int len_start = 0, len_end = 0;
	size_t off_start, off_end;

	bool is_reversed = ((strncmp(n->str, NUM_REVERSE_START, len_rev_start) == 0) &&
	                    (strncmp(n->str + len - len_rev_end, NUM_REVERSE_END, len_rev_end) == 0)) ||
	                   ((strncmp(n->str + len_inv_start, NUM_REVERSE_START, len_rev_start) == 0) &&
	                    (strncmp(n->str + len - len_rev_end - len_inv_end, NUM_REVERSE_END, len_rev_end) == 0));
	bool is_inversed = ((strncmp(n->str, NUM_INVERSE_START, len_inv_start) == 0) &&
	                    (strncmp(n->str + len - len_inv_end, NUM_INVERSE_END, len_inv_end) == 0)) ||
	                   ((strncmp(n->str + len_rev_start, NUM_INVERSE_START, len_inv_start) == 0) &&
	                    (strncmp(n->str + len - len_inv_end - len_rev_end, NUM_INVERSE_END, len_inv_end) == 0));

	if ((!reverse && !inverse) || n->str[0] == '\0') {
		return false;
	}

	if (reverse) {
		if (is_reversed) {
			len_start -= len_rev_start;
			len_end -= len_rev_end;
		}
		else {
			len_start += len_rev_start;
			len_end += len_rev_end;
		}
	}
	if (inverse) {
		if (is_inversed) {
			len_start -= len_inv_start;
			len_end -= len_inv_end;
		}
		else {
			len_start += len_inv_start;
			len_end += len_inv_end;
		}
	}

	if (len_start < 0) {
		len -= (size_t)(-(len_start + len_end));
		memmove(n->str, n->str + (size_t)(-len_start), len);
	}
	else if (len_start > 0) {
		if (len + len_start + len_end > sizeof(n->str)) {
			return false;  /* Not enough room in buffer... */
		}
		memmove(n->str + (size_t)len_start, n->str, len);
		len += (size_t)(len_start + len_end);
	}

	if (reverse) {
		is_reversed = !is_reversed;
	}
	if (inverse) {
		is_inversed = !is_inversed;
	}

	off_start = 0;
	off_end = len;
	if (is_reversed) {
		off_end -= len_rev_end;
		memcpy(n->str + off_start, NUM_REVERSE_START, len_rev_start);
		memcpy(n->str + off_end, NUM_REVERSE_END, len_rev_end);
		off_start += len_rev_start;
	}
	if (is_inversed) {
		off_end -= len_inv_end;
		memcpy(n->str + off_start, NUM_INVERSE_START, len_inv_start);
		memcpy(n->str + off_end, NUM_INVERSE_END, len_inv_end);
		off_start += len_inv_start;
	}

	n->str[len] = '\0';
	n->str_cur += len_start;
	return true;
}

#undef NUM_REVERSE_START
#undef NUM_REVERSE_END
#undef NUM_INVERSE_START
#undef NUM_INVERSE_END

bool handleNumInput(bContext *C, NumInput *n, const wmEvent *event)
{
	const char *utf8_buf = NULL;
	char ascii[2] = {'\0', '\0'};
	bool updated = false;
	short idx = n->idx, idx_max = n->idx_max;
	short dir = STRCUR_DIR_NEXT, mode = STRCUR_JUMP_NONE;
	int cur;
	double val;

	switch (event->type) {
		case EVT_MODAL_MAP:
			if (ELEM(event->val, NUM_MODAL_INCREMENT_UP, NUM_MODAL_INCREMENT_DOWN)) {
				n->val[idx] += (event->val == NUM_MODAL_INCREMENT_UP) ? n->val_inc[idx] : -n->val_inc[idx];
				value_to_editstr(n, idx);
				n->val_flag[idx] |= NUM_EDITED;
				updated = true;
			}
			else {
				/* might be a char too... */
				utf8_buf = event->utf8_buf;
				ascii[0] = event->ascii;
			}
			break;
		case BACKSPACEKEY:
			/* Part specific to backspace... */
			if (!(n->val_flag[idx] & NUM_EDITED)) {
				copy_v3_v3(n->val, n->val_org);
				n->val_flag[0] &= ~NUM_EDITED;
				n->val_flag[1] &= ~NUM_EDITED;
				n->val_flag[2] &= ~NUM_EDITED;
				updated = true;
				break;
			}
			else if (event->shift || !n->str[0]) {
				n->val[idx] = n->val_org[idx];
				n->val_flag[idx] &= ~NUM_EDITED;
				n->str[0] = '\0';
				n->str_cur = 0;
				updated = true;
				break;
			}
			/* Else, common behavior with DELKEY, only difference is remove char(s) before/after the cursor. */
			dir = STRCUR_DIR_PREV;
			/* fall-through */
		case DELKEY:
			if ((n->val_flag[idx] & NUM_EDITED) && n->str[0]) {
				int t_cur = cur = n->str_cur;
				if (event->ctrl) {
					mode = STRCUR_JUMP_DELIM;
				}
				BLI_str_cursor_step_utf8(n->str, strlen(n->str), &t_cur, dir, mode, true);
				if (t_cur != cur) {
					if (t_cur < cur) {
						SWAP(int, t_cur, cur);
						n->str_cur = cur;
					}
					memmove(&n->str[cur], &n->str[t_cur], strlen(&n->str[t_cur]) + 1);  /* +1 for trailing '\0'. */
					updated = true;
				}
			}
			else {
				return false;
			}
			break;
		case LEFTARROWKEY:
			dir = STRCUR_DIR_PREV;
			/* fall-through */
		case RIGHTARROWKEY:
			cur = n->str_cur;
			if (event->ctrl) {
				mode = STRCUR_JUMP_DELIM;
			}
			BLI_str_cursor_step_utf8(n->str, strlen(n->str), &cur, dir, mode, true);
			if (cur != n->str_cur) {
				n->str_cur = cur;
				return true;
			}
			return false;
		case HOMEKEY:
			if (n->str[0]) {
				n->str_cur = 0;
				return true;
			}
			return false;
		case ENDKEY:
			if (n->str[0]) {
				n->str_cur = strlen(n->str);
				return true;
			}
			return false;
		case TABKEY:
			n->val_org[idx] = n->val[idx];

			idx += event->ctrl ? -1 : 1;
			idx %= idx_max + 1;
			n->idx = idx;
			n->val[idx] = n->val_org[idx];
			if (n->val_flag[idx] & NUM_EDITED) {
				value_to_editstr(n, idx);
			}
			else {
				n->str[0] = '\0';
				n->str_cur = 0;
			}
			return true;
		case PADPERIOD:
			/* Force numdot, some OSs/countries generate a comma char in this case, sic...  (T37992) */
			ascii[0] = '.';
			utf8_buf = ascii;
			break;
		case PADMINUS:
			if (event->ctrl && editstr_reverse_inverse_toggle(n, true, false)) {
				updated = true;
				break;
			}
			/* fall-through */
		case PADSLASHKEY:
			if (event->ctrl && editstr_reverse_inverse_toggle(n, false, true)) {
				updated = true;
				break;
			}
			/* fall-through */
		case CKEY:
			if (event->ctrl) {
				/* Copy current str to the copypaste buffer. */
				WM_clipboard_text_set(n->str, 0);
				updated = true;
				break;
			}
			/* fall-through */
		case VKEY:
			if (event->ctrl) {
				/* extract the first line from the clipboard */
				char *pbuf = WM_clipboard_text_get(0);

				if (pbuf) {
					bool success;
					/* Only copy string until first of this char. */
					char *cr = strchr(pbuf, '\r');
					char *cn = strchr(pbuf, '\n');
					if (cn && cn < cr) cr = cn;
					if (cr) *cr = '\0';

					success = editstr_insert_at_cursor(n, pbuf, strlen(pbuf));

					MEM_freeN(pbuf);
					if (!success) {
						return false;
					}

					n->val_flag[idx] |= NUM_EDITED;
				}
				updated = true;
				break;
			}
			/* fall-through */
		default:
			utf8_buf = event->utf8_buf;
			ascii[0] = event->ascii;
			break;
	}

	if (utf8_buf && !utf8_buf[0] && ascii[0]) {
		/* Fallback to ascii. */
		utf8_buf = ascii;
	}

	if (utf8_buf && utf8_buf[0]) {
		if (!editstr_insert_at_cursor(n, utf8_buf, BLI_str_utf8_size(utf8_buf))) {
			return false;
		}

		n->val_flag[idx] |= NUM_EDITED;
	}
	else if (!updated) {
		return false;
	}

	/* At this point, our value has changed, try to interpret it with python (if str is not empty!). */
	if (n->str[0]) {
#ifdef WITH_PYTHON
		char str_unit_convert[NUM_STR_REP_LEN * 6];  /* Should be more than enough! */
		const char *default_unit = NULL;

		/* Make radian default unit when needed. */
		if (n->unit_use_radians && n->unit_type[idx] == B_UNIT_ROTATION)
			default_unit = "r";

		BLI_strncpy(str_unit_convert, n->str, sizeof(str_unit_convert));

		bUnit_ReplaceString(str_unit_convert, sizeof(str_unit_convert), default_unit, 1.0,
		                    n->unit_sys, n->unit_type[idx]);

		/* Note: with angles, we always get values as radians here... */
		if (BPY_button_exec(C, str_unit_convert, &val, false) != -1) {
			n->val[idx] = (float)val;
			n->val_flag[idx] &= ~NUM_INVALID;
		}
		else {
			n->val_flag[idx] |= NUM_INVALID;
		}
#else  /* Very unlikely, but does not harm... */
		n->val[idx] = (float)atof(n->str);
#endif  /* WITH_PYTHON */
	}

	/* REDRAW SINCE NUMBERS HAVE CHANGED */
	return true;
}
