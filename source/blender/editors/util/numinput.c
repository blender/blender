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
#include "UI_interface.h"


/* NumInput.flag */
enum {
	/* (1 << 8) and below are reserved for public flags! */
	NUM_EDIT_FULL       = (1 << 9),   /* Enable full editing, with units and math operators support. */
};

/* NumInput.val_flag[] */
enum {
	/* (1 << 8) and below are reserved for public flags! */
	NUM_EDITED          = (1 << 9),    /* User has edited this value somehow. */
	NUM_INVALID         = (1 << 10),   /* Current expression for this value is invalid. */
	NUM_NEGATE          = (1 << 11),   /* Current expression's result has to be negated. */
	NUM_INVERSE         = (1 << 12),   /* Current expression's result has to be inverted. */
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
	int prec = 2; /* draw-only, and avoids too much issues with radian->degrees conversion. */

	for (j = 0; j <= n->idx_max; j++) {
		/* if AFFECTALL and no number typed and cursor not on number, use first number */
		i = (n->flag & NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;

		if (n->val_flag[i] & NUM_EDITED) {
			/* Get the best precision, allows us to draw '10.0001' as '10' instead! */
			prec = uiFloatPrecisionCalc(prec, (double)n->val[i]);
			if (i == n->idx) {
				const char *heading_exp = "", *trailing_exp = "";
				char before_cursor[NUM_STR_REP_LEN];
				char val[16];

				if (n->val_flag[i] & NUM_NEGATE) {
					heading_exp = (n->val_flag[i] & NUM_INVERSE) ? "-1/(" : "-(";
					trailing_exp = ")";
				}
				else if (n->val_flag[i] & NUM_INVERSE) {
					heading_exp = "1/(";
					trailing_exp = ")";
				}

				if (n->val_flag[i] & NUM_INVALID) {
					BLI_strncpy(val, "Invalid", sizeof(val));
				}
				else {
					bUnit_AsString(val, sizeof(val), (double)n->val[i], prec,
					               n->unit_sys, n->unit_type[i], true, false);
				}

				BLI_strncpy(before_cursor, n->str, n->str_cur + 1);  /* +1 because of trailing '\0' */
				BLI_snprintf(&str[j * ln], ln, "[%s%s|%s%s] = %s",
				             heading_exp, before_cursor, &n->str[n->str_cur], trailing_exp, val);
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
		/* We might have cut some multi-bytes utf8 chars (e.g. trailing '°' of degrees values can become only 'A')... */
		BLI_utf8_invalid_strip(&str[j * ln], strlen(&str[j * ln]));
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
	n->str_cur = bUnit_AsString(n->str, NUM_STR_REP_LEN, (double)n->val[idx], prec,
	                            n->unit_sys, n->unit_type[idx], true, false);
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

static bool editstr_is_simple_numinput(const char ascii)
{
	if (ascii >= '0' && ascii <= '9') {
		return true;
	}
	else if (ascii == '.') {
		return true;
	}
	else {
		return false;
	}
}

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
			n->val_flag[idx] &= ~(NUM_NEGATE | NUM_INVERSE);

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
		case EQUALKEY:
		case PADASTERKEY:
			if (!(n->flag & NUM_EDIT_FULL)) {
				n->flag |= NUM_EDIT_FULL;
				n->val_flag[idx] |= NUM_EDITED;
				return true;
			}
			else if (event->ctrl) {
				n->flag &= ~NUM_EDIT_FULL;
				return true;
			}
			/* fall-through */
		case PADMINUS:
		case MINUSKEY:
			if (event->ctrl || !(n->flag & NUM_EDIT_FULL)) {
				n->val_flag[idx] ^= NUM_NEGATE;
				updated = true;
				break;
			}
			/* fall-through */
		case PADSLASHKEY:
		case SLASHKEY:
			if (event->ctrl || !(n->flag & NUM_EDIT_FULL)) {
				n->val_flag[idx] ^= NUM_INVERSE;
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
				int pbuf_len;
				char *pbuf = WM_clipboard_text_get_firstline(false, &pbuf_len);

				if (pbuf) {
					bool success;

					success = editstr_insert_at_cursor(n, pbuf, pbuf_len);

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
		if (!(n->flag & NUM_EDIT_FULL)) {
			/* In simple edit mode, we only keep a few chars as valid! */
			/* no need to decode unicode, ascii is first char only */
			if (!editstr_is_simple_numinput(utf8_buf[0])) {
				return false;
			}
		}

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

		if (n->val_flag[idx] & NUM_NEGATE) {
			n->val[idx] = -n->val[idx];
		}
		if (n->val_flag[idx] & NUM_INVERSE) {
			n->val[idx] = 1.0f / n->val[idx];
		}
	}

	/* REDRAW SINCE NUMBERS HAVE CHANGED */
	return true;
}
