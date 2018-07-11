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
#include "BKE_scene.h"
#include "BKE_unit.h"

#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "ED_numinput.h"
#include "UI_interface.h"

/* Numeric input which isn't allowing full numeric editing. */
#define USE_FAKE_EDIT

/* NumInput.flag */
enum {
	/* (1 << 8) and below are reserved for public flags! */
	NUM_EDIT_FULL       = (1 << 9),   /* Enable full editing, with units and math operators support. */
#ifdef USE_FAKE_EDIT
	NUM_FAKE_EDITED     = (1 << 10),  /* Fake edited state (temp, avoids issue with backspace). */
#endif
};

/* NumInput.val_flag[] */
enum {
	/* (1 << 8) and below are reserved for public flags! */
	NUM_EDITED          = (1 << 9),    /* User has edited this value somehow. */
	NUM_INVALID         = (1 << 10),   /* Current expression for this value is invalid. */
#ifdef USE_FAKE_EDIT
	NUM_NEGATE          = (1 << 11),   /* Current expression's result has to be negated. */
	NUM_INVERSE         = (1 << 12),   /* Current expression's result has to be inverted. */
#endif
};

/* ************************** Functions *************************** */

/* ************************** NUMINPUT **************************** */

void initNumInput(NumInput *n)
{
	n->idx_max = 0;
	n->unit_sys = USER_UNIT_NONE;
	copy_vn_i(n->unit_type, NUM_MAX_ELEMENTS, B_UNIT_NONE);
	n->unit_use_radians = false;

	n->flag = 0;
	copy_vn_short(n->val_flag, NUM_MAX_ELEMENTS, 0);
	zero_v3(n->val);
	copy_vn_fl(n->val_org, NUM_MAX_ELEMENTS, 0.0f);
	copy_vn_fl(n->val_inc, NUM_MAX_ELEMENTS, 1.0f);

	n->idx = 0;
	n->str[0] = '\0';
	n->str_cur = 0;
}

/* str must be NUM_STR_REP_LEN * (idx_max + 1) length. */
void outputNumInput(NumInput *n, char *str, UnitSettings *unit_settings)
{
	short j;
	const int ln = NUM_STR_REP_LEN;
	int prec = 2; /* draw-only, and avoids too much issues with radian->degrees conversion. */

	for (j = 0; j <= n->idx_max; j++) {
		/* if AFFECTALL and no number typed and cursor not on number, use first number */
		const short i = (n->flag & NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;

		/* Use scale_length if needed! */
		const float fac = (float)BKE_scene_unit_scale(unit_settings, n->unit_type[j], 1.0);

		if (n->val_flag[i] & NUM_EDITED) {
			/* Get the best precision, allows us to draw '10.0001' as '10' instead! */
			prec = UI_calc_float_precision(prec, (double)n->val[i]);
			if (i == n->idx) {
				const char *heading_exp = "", *trailing_exp = "";
				char before_cursor[NUM_STR_REP_LEN];
				char val[16];

#ifdef USE_FAKE_EDIT
				if (n->val_flag[i] & NUM_NEGATE) {
					heading_exp = (n->val_flag[i] & NUM_INVERSE) ? "-1/(" : "-(";
					trailing_exp = ")";
				}
				else if (n->val_flag[i] & NUM_INVERSE) {
					heading_exp = "1/(";
					trailing_exp = ")";
				}
#endif

				if (n->val_flag[i] & NUM_INVALID) {
					BLI_strncpy(val, "Invalid", sizeof(val));
				}
				else {
					bUnit_AsString(val, sizeof(val), (double)(n->val[i] * fac), prec,
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

#ifdef USE_FAKE_EDIT
	if (n->flag & NUM_FAKE_EDITED) {
		return true;
	}
#endif

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
bool applyNumInput(NumInput *n, float *vec)
{
	short i, j;
	float val;

	if (hasNumInput(n)) {
		for (j = 0; j <= n->idx_max; j++) {
#ifdef USE_FAKE_EDIT
			if (n->flag & NUM_FAKE_EDITED) {
				val = n->val[j];
			}
			else
#endif
			{
				/* if AFFECTALL and no number typed and cursor not on number, use first number */
				i = (n->flag & NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;
				val = (!(n->val_flag[i] & NUM_EDITED) && n->val_flag[i] & NUM_NULL_ONE) ? 1.0f : n->val[i];

				if (n->val_flag[i] & NUM_NO_NEGATIVE && val < 0.0f) {
					val = 0.0f;
				}
				if (n->val_flag[i] & NUM_NO_FRACTION && val != floorf(val)) {
					val = floorf(val + 0.5f);
					if (n->val_flag[i] & NUM_NO_ZERO && val == 0.0f) {
						val = 1.0f;
					}
				}
				else if (n->val_flag[i] & NUM_NO_ZERO && val == 0.0f) {
					val = 0.0001f;
				}
			}
			vec[j] = val;
		}
#ifdef USE_FAKE_EDIT
		n->flag &= ~NUM_FAKE_EDITED;
#endif
		return true;
	}
	else {
		/* Else, we set the 'org' values for numinput! */
		for (j = 0; j <= n->idx_max; j++) {
			n->val[j] = n->val_org[j] = vec[j];
		}
		return false;
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

#ifdef USE_FAKE_EDIT
	if (U.flag & USER_FLAG_NUMINPUT_ADVANCED)
#endif
	{
		if ((event->ctrl == 0) && (event->alt == 0) && (event->ascii != '\0') &&
		    strchr("01234567890@%^&*-+/{}()[]<>.|", event->ascii))
		{
			if (!(n->flag & NUM_EDIT_FULL)) {
				n->flag |= NUM_EDITED;
				n->flag |= NUM_EDIT_FULL;
				n->val_flag[idx] |= NUM_EDITED;
			}
		}
	}

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
#ifdef USE_FAKE_EDIT
				n->flag |= NUM_FAKE_EDITED;
#else
				n->flag |= NUM_EDIT_FULL;
#endif
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
			ATTR_FALLTHROUGH;
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
				if (!n->str[0]) {
					n->val[idx] = n->val_org[idx];
				}
			}
			else {
				return false;
			}
			break;
		case LEFTARROWKEY:
			dir = STRCUR_DIR_PREV;
			ATTR_FALLTHROUGH;
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
#ifdef USE_FAKE_EDIT
			n->val_flag[idx] &= ~(NUM_NEGATE | NUM_INVERSE);
#endif

			idx = (idx + idx_max + (event->ctrl ? 0 : 2)) % (idx_max + 1);
			n->idx = idx;
			if (n->val_flag[idx] & NUM_EDITED) {
				value_to_editstr(n, idx);
			}
			else {
				n->str[0] = '\0';
				n->str_cur = 0;
			}
			return true;
		case PADPERIOD:
		case PERIODKEY:
			/* Force numdot, some OSs/countries generate a comma char in this case, sic...  (T37992) */
			ascii[0] = '.';
			utf8_buf = ascii;
			break;
#if 0
		/* Those keys are not directly accessible in all layouts, preventing to generate matching events.
		 * So we use a hack (ascii value) instead, see below.
		 */
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
			break;
#endif

#ifdef USE_FAKE_EDIT
		case PADMINUS:
		case MINUSKEY:
			if (event->ctrl || !(n->flag & NUM_EDIT_FULL)) {
				n->val_flag[idx] ^= NUM_NEGATE;
				updated = true;
			}
			break;
		case PADSLASHKEY:
		case SLASHKEY:
			if (event->ctrl || !(n->flag & NUM_EDIT_FULL)) {
				n->val_flag[idx] ^= NUM_INVERSE;
				updated = true;
			}
			break;
#endif
		case CKEY:
			if (event->ctrl) {
				/* Copy current str to the copypaste buffer. */
				WM_clipboard_text_set(n->str, 0);
				updated = true;
			}
			break;
		case VKEY:
			if (event->ctrl) {
				/* extract the first line from the clipboard */
				int pbuf_len;
				char *pbuf = WM_clipboard_text_get_firstline(false, &pbuf_len);

				if (pbuf) {
					const bool success = editstr_insert_at_cursor(n, pbuf, pbuf_len);

					MEM_freeN(pbuf);
					if (!success) {
						return false;
					}

					n->val_flag[idx] |= NUM_EDITED;
				}
				updated = true;
			}
			break;
		default:
			break;
	}

	if (!updated && !utf8_buf && (event->utf8_buf[0] || event->ascii)) {
		utf8_buf = event->utf8_buf;
		ascii[0] = event->ascii;
	}

#ifdef USE_FAKE_EDIT
	/* XXX Hack around keyboards without direct access to '=' nor '*'... */
	if (ELEM(ascii[0], '=', '*')) {
		if (!(n->flag & NUM_EDIT_FULL)) {
			n->flag |= NUM_EDIT_FULL;
			n->val_flag[idx] |= NUM_EDITED;
			return true;
		}
		else if (event->ctrl) {
			n->flag &= ~NUM_EDIT_FULL;
			return true;
		}
	}
#endif

	/* Up to this point, if we have a ctrl modifier, skip.
	 * This allows to still access most of modals' shortcuts even in numinput mode.
	 */
	if (!updated && event->ctrl) {
		return false;
	}

	if ((!utf8_buf || !utf8_buf[0]) && ascii[0]) {
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
		const float val_prev = n->val[idx];
		double val;
#ifdef WITH_PYTHON
		Scene *sce = CTX_data_scene(C);
		char str_unit_convert[NUM_STR_REP_LEN * 6];  /* Should be more than enough! */
		const char *default_unit = NULL;

		/* Use scale_length if needed! */
		const float fac = (float)BKE_scene_unit_scale(&sce->unit, n->unit_type[idx], 1.0);

		/* Make radian default unit when needed. */
		if (n->unit_use_radians && n->unit_type[idx] == B_UNIT_ROTATION) {
			default_unit = "r";
		}

		BLI_strncpy(str_unit_convert, n->str, sizeof(str_unit_convert));

		bUnit_ReplaceString(str_unit_convert, sizeof(str_unit_convert), default_unit, fac,
		                    n->unit_sys, n->unit_type[idx]);

		/* Note: with angles, we always get values as radians here... */
		if (BPY_execute_string_as_number(C, str_unit_convert, false, &val)) {
			n->val[idx] = (float)val;
			n->val_flag[idx] &= ~NUM_INVALID;
		}
		else {
			n->val_flag[idx] |= NUM_INVALID;
		}
#else  /* Very unlikely, but does not harm... */
		val = atof(n->str);
		n->val[idx] = (float)val;
		UNUSED_VARS(C);
#endif  /* WITH_PYTHON */


#ifdef USE_FAKE_EDIT
		if (n->val_flag[idx] & NUM_NEGATE) {
			n->val[idx] = -n->val[idx];
		}
		if (n->val_flag[idx] & NUM_INVERSE) {
			val = n->val[idx];
			/* If we invert on radians when user is in degrees, you get unexpected results... See T53463. */
			if (!n->unit_use_radians && n->unit_type[idx] == B_UNIT_ROTATION) {
				val = RAD2DEG(val);
			}
			val = 1.0 / val;
			if (!n->unit_use_radians && n->unit_type[idx] == B_UNIT_ROTATION) {
				val = DEG2RAD(val);
			}
			n->val[idx] = (float)val;
		}
#endif

		if (UNLIKELY(!isfinite(n->val[idx]))) {
			n->val[idx] = val_prev;
			n->val_flag[idx] |= NUM_INVALID;
		}
	}

	/* REDRAW SINCE NUMBERS HAVE CHANGED */
	return true;
}
