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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2003-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_sequencer/sequencer_draw.c
 *  \ingroup spseq
 */


#include <string.h>
#include <math.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_threads.h"

#include "IMB_imbuf_types.h"

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_sound_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"
#include "BKE_scene.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_basic_shader.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_markers.h"
#include "ED_mask.h"
#include "ED_sequencer.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"

#include "MEM_guardedalloc.h"

/* own include */
#include "sequencer_intern.h"


#define SEQ_LEFTHANDLE   1
#define SEQ_RIGHTHANDLE  2

#define SEQ_HANDLE_SIZE_MIN  7.0f
#define SEQ_HANDLE_SIZE_MAX 40.0f


/* Note, Don't use SEQ_BEGIN/SEQ_END while drawing!
 * it messes up transform, - Campbell */
#undef SEQ_BEGIN
#undef SEQP_BEGIN
#undef SEQ_END

static Sequence *special_seq_update = NULL;

void color3ubv_from_seq(Scene *curscene, Sequence *seq, unsigned char col[3])
{
	unsigned char blendcol[3];
	SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;

	switch (seq->type) {
		case SEQ_TYPE_IMAGE:
			UI_GetThemeColor3ubv(TH_SEQ_IMAGE, col);
			break;

		case SEQ_TYPE_META:
			UI_GetThemeColor3ubv(TH_SEQ_META, col);
			break;

		case SEQ_TYPE_MOVIE:
			UI_GetThemeColor3ubv(TH_SEQ_MOVIE, col);
			break;

		case SEQ_TYPE_MOVIECLIP:
			UI_GetThemeColor3ubv(TH_SEQ_MOVIECLIP, col);
			break;

		case SEQ_TYPE_MASK:
			UI_GetThemeColor3ubv(TH_SEQ_MASK, col); /* TODO */
			break;

		case SEQ_TYPE_SCENE:
			UI_GetThemeColor3ubv(TH_SEQ_SCENE, col);
		
			if (seq->scene == curscene) {
				UI_GetColorPtrShade3ubv(col, col, 20);
			}
			break;
		
		/* transitions */
		case SEQ_TYPE_CROSS:
		case SEQ_TYPE_GAMCROSS:
		case SEQ_TYPE_WIPE:
			UI_GetThemeColor3ubv(TH_SEQ_TRANSITION, col);

			/* slightly offset hue to distinguish different effects */
			if (seq->type == SEQ_TYPE_CROSS)    rgb_byte_set_hue_float_offset(col, 0.04);
			if (seq->type == SEQ_TYPE_GAMCROSS) rgb_byte_set_hue_float_offset(col, 0.08);
			if (seq->type == SEQ_TYPE_WIPE)     rgb_byte_set_hue_float_offset(col, 0.12);
			break;

		/* effects */
		case SEQ_TYPE_TRANSFORM:
		case SEQ_TYPE_SPEED:
		case SEQ_TYPE_ADD:
		case SEQ_TYPE_SUB:
		case SEQ_TYPE_MUL:
		case SEQ_TYPE_ALPHAOVER:
		case SEQ_TYPE_ALPHAUNDER:
		case SEQ_TYPE_OVERDROP:
		case SEQ_TYPE_GLOW:
		case SEQ_TYPE_MULTICAM:
		case SEQ_TYPE_ADJUSTMENT:
		case SEQ_TYPE_GAUSSIAN_BLUR:
			UI_GetThemeColor3ubv(TH_SEQ_EFFECT, col);

			/* slightly offset hue to distinguish different effects */
			if      (seq->type == SEQ_TYPE_ADD)           rgb_byte_set_hue_float_offset(col, 0.04);
			else if (seq->type == SEQ_TYPE_SUB)           rgb_byte_set_hue_float_offset(col, 0.08);
			else if (seq->type == SEQ_TYPE_MUL)           rgb_byte_set_hue_float_offset(col, 0.12);
			else if (seq->type == SEQ_TYPE_ALPHAOVER)     rgb_byte_set_hue_float_offset(col, 0.16);
			else if (seq->type == SEQ_TYPE_ALPHAUNDER)    rgb_byte_set_hue_float_offset(col, 0.20);
			else if (seq->type == SEQ_TYPE_OVERDROP)      rgb_byte_set_hue_float_offset(col, 0.24);
			else if (seq->type == SEQ_TYPE_GLOW)          rgb_byte_set_hue_float_offset(col, 0.28);
			else if (seq->type == SEQ_TYPE_TRANSFORM)     rgb_byte_set_hue_float_offset(col, 0.36);
			else if (seq->type == SEQ_TYPE_MULTICAM)      rgb_byte_set_hue_float_offset(col, 0.32);
			else if (seq->type == SEQ_TYPE_ADJUSTMENT)    rgb_byte_set_hue_float_offset(col, 0.40);
			else if (seq->type == SEQ_TYPE_GAUSSIAN_BLUR) rgb_byte_set_hue_float_offset(col, 0.42);
			break;

		case SEQ_TYPE_COLOR:
			rgb_float_to_uchar(col, colvars->col);
			break;

		case SEQ_TYPE_SOUND_RAM:
			UI_GetThemeColor3ubv(TH_SEQ_AUDIO, col);
			blendcol[0] = blendcol[1] = blendcol[2] = 128;
			if (seq->flag & SEQ_MUTE) UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.5, 20);
			break;

		case SEQ_TYPE_TEXT:
			UI_GetThemeColor3ubv(TH_SEQ_TEXT, col);
			break;

		default:
			col[0] = 10; col[1] = 255; col[2] = 40;
			break;
	}
}

static void drawseqwave(const bContext *C, SpaceSeq *sseq, Scene *scene, Sequence *seq, float x1, float y1, float x2, float y2, float stepsize)
{
	/*
	 * x1 is the starting x value to draw the wave,
	 * x2 the end x value, same for y1 and y2
	 * stepsize is width of a pixel.
	 */
	if (seq->sound && ((sseq->flag & SEQ_ALL_WAVEFORMS) || (seq->flag & SEQ_AUDIO_DRAW_WAVEFORM))) {
		int i, j, pos;
		int length = floor((x2 - x1) / stepsize) + 1;
		float ymid = (y1 + y2) / 2;
		float yscale = (y2 - y1) / 2;
		float samplestep;
		float startsample, endsample;
		float value1, value2;
		bSound *sound = seq->sound;
		
		SoundWaveform *waveform;
		
		if (!sound->spinlock) {
			sound->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
			BLI_spin_init(sound->spinlock);
		}
		
		BLI_spin_lock(sound->spinlock);
		if (!sound->waveform) {
			if (!(sound->flags & SOUND_FLAGS_WAVEFORM_LOADING)) {
				/* prevent sounds from reloading */
				sound->flags |= SOUND_FLAGS_WAVEFORM_LOADING;
				BLI_spin_unlock(sound->spinlock);
				sequencer_preview_add_sound(C, seq);
			}
			else {
				BLI_spin_unlock(sound->spinlock);
			}
			return;  /* nothing to draw */
		}
		BLI_spin_unlock(sound->spinlock);
		
		waveform = sound->waveform;

		if (waveform->length == 0) {
			/* BKE_sound_read_waveform() set an empty SoundWaveform data in case it cannot generate a valid one...
			 * See T45726. */
			return;
		}

		startsample = floor((seq->startofs + seq->anim_startofs) / FPS * SOUND_WAVE_SAMPLES_PER_SECOND);
		endsample = ceil((seq->startofs + seq->anim_startofs + seq->enddisp - seq->startdisp) / FPS * SOUND_WAVE_SAMPLES_PER_SECOND);
		samplestep = (endsample - startsample) * stepsize / (x2 - x1);

		if (length > floor((waveform->length - startsample) / samplestep))
			length = floor((waveform->length - startsample) / samplestep);

		glColor4f(1.0f, 1.0f, 1.0f, 0.5);
		glEnable(GL_BLEND);
		glBegin(GL_TRIANGLE_STRIP);
		for (i = 0; i < length; i++) {
			float sampleoffset = startsample + i * samplestep;
			pos = sampleoffset;

			value1 = waveform->data[pos * 3];
			value2 = waveform->data[pos * 3 + 1];

			if (samplestep > 1.0f) {
				for (j = pos + 1; (j < waveform->length) && (j < pos + samplestep); j++) {
					if (value1 > waveform->data[j * 3])
						value1 = waveform->data[j * 3];

					if (value2 < waveform->data[j * 3 + 1])
						value2 = waveform->data[j * 3 + 1];
				}
			}
			else {
				/* use simple linear interpolation */
				float f = sampleoffset - pos;
				value1 = (1.0f - f) * value1 + f * waveform->data[pos * 3 + 3];
				value2 = (1.0f - f) * value2 + f * waveform->data[pos * 3 + 4];
			}

			glVertex2f(x1 + i * stepsize, ymid + value1 * yscale);
			glVertex2f(x1 + i * stepsize, ymid + value2 * yscale);
		}
		glEnd();
		glDisable(GL_BLEND);
	}
}

static void drawmeta_stipple(int value)
{
	if (value) {
		GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);
		GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_HALFTONE);
		
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0x8888);
	}
	else {
		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
		glDisable(GL_LINE_STIPPLE);
	}
}

static void drawmeta_contents(Scene *scene, Sequence *seqm, float x1, float y1, float x2, float y2)
{
	/* note: this used to use SEQ_BEGIN/SEQ_END, but it messes up the
	 * seq->depth value, (needed by transform when doing overlap checks)
	 * so for now, just use the meta's immediate children, could be fixed but
	 * its only drawing - campbell */
	Sequence *seq;
	unsigned char col[4];

	int chan_min = MAXSEQ;
	int chan_max = 0;
	int chan_range = 0;
	float draw_range = y2 - y1;
	float draw_height;
	ListBase *seqbase;
	int offset;

	seqbase = BKE_sequence_seqbase_get(seqm, &offset);
	if (!seqbase || BLI_listbase_is_empty(seqbase)) {
		return;
	}

	if (seqm->type == SEQ_TYPE_SCENE) {
		offset  = seqm->start - offset;
	}
	else {
		offset = 0;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (seqm->flag & SEQ_MUTE)
		drawmeta_stipple(1);

	for (seq = seqbase->first; seq; seq = seq->next) {
		chan_min = min_ii(chan_min, seq->machine);
		chan_max = max_ii(chan_max, seq->machine);
	}

	chan_range = (chan_max - chan_min) + 1;
	draw_height = draw_range / chan_range;

	col[3] = 196; /* alpha, used for all meta children */

	for (seq = seqbase->first; seq; seq = seq->next) {
		const int startdisp = seq->startdisp + offset;
		const int enddisp   = seq->enddisp   + offset;

		if ((startdisp > x2 || enddisp < x1) == 0) {
			float y_chan = (seq->machine - chan_min) / (float)(chan_range) * draw_range;
			float x1_chan = startdisp;
			float x2_chan = enddisp;
			float y1_chan, y2_chan;

			if ((seqm->flag & SEQ_MUTE) == 0 && (seq->flag & SEQ_MUTE))
				drawmeta_stipple(1);

			color3ubv_from_seq(scene, seq, col);

			glColor4ubv(col);
			
			/* clamp within parent sequence strip bounds */
			if (x1_chan < x1) x1_chan = x1;
			if (x2_chan > x2) x2_chan = x2;

			y1_chan = y1 + y_chan + (draw_height * SEQ_STRIP_OFSBOTTOM);
			y2_chan = y1 + y_chan + (draw_height * SEQ_STRIP_OFSTOP);

			glRectf(x1_chan,  y1_chan, x2_chan,  y2_chan);

			UI_GetColorPtrShade3ubv(col, col, -30);
			glColor4ubv(col);
			fdrawbox(x1_chan,  y1_chan, x2_chan,  y2_chan);

			if ((seqm->flag & SEQ_MUTE) == 0 && (seq->flag & SEQ_MUTE))
				drawmeta_stipple(0);
		}
	}

	if (seqm->flag & SEQ_MUTE)
		drawmeta_stipple(0);
	
	glDisable(GL_BLEND);
}

/* clamp handles to defined size in pixel space */
static float draw_seq_handle_size_get_clamped(Sequence *seq, const float pixelx)
{
	const float minhandle = pixelx * SEQ_HANDLE_SIZE_MIN;
	const float maxhandle = pixelx * SEQ_HANDLE_SIZE_MAX;
	float size = CLAMPIS(seq->handsize, minhandle, maxhandle);

	/* ensure we're not greater than half width */
	return min_ff(size, ((float)(seq->enddisp - seq->startdisp) / 2.0f) / pixelx);
}

/* draw a handle, for each end of a sequence strip */
static void draw_seq_handle(View2D *v2d, Sequence *seq, const float handsize_clamped, const short direction)
{
	float v1[2], v2[2], v3[2], rx1 = 0, rx2 = 0; //for triangles and rect
	float x1, x2, y1, y2;
	unsigned int whichsel = 0;
	
	x1 = seq->startdisp;
	x2 = seq->enddisp;
	
	y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
	y2 = seq->machine + SEQ_STRIP_OFSTOP;

	/* set up co-ordinates/dimensions for either left or right handle */
	if (direction == SEQ_LEFTHANDLE) {
		rx1 = x1;
		rx2 = x1 + handsize_clamped * 0.75f;
		
		v1[0] = x1 + handsize_clamped / 4; v1[1] = y1 + ( ((y1 + y2) / 2.0f - y1) / 2);
		v2[0] = x1 + handsize_clamped / 4; v2[1] = y2 - ( ((y1 + y2) / 2.0f - y1) / 2);
		v3[0] = v2[0] + handsize_clamped / 4; v3[1] = (y1 + y2) / 2.0f;
		
		whichsel = SEQ_LEFTSEL;
	}
	else if (direction == SEQ_RIGHTHANDLE) {
		rx1 = x2 - handsize_clamped * 0.75f;
		rx2 = x2;
		
		v1[0] = x2 - handsize_clamped / 4; v1[1] = y1 + ( ((y1 + y2) / 2.0f - y1) / 2);
		v2[0] = x2 - handsize_clamped / 4; v2[1] = y2 - ( ((y1 + y2) / 2.0f - y1) / 2);
		v3[0] = v2[0] - handsize_clamped / 4; v3[1] = (y1 + y2) / 2.0f;
		
		whichsel = SEQ_RIGHTSEL;
	}
	
	/* draw! */
	if (!(seq->type & SEQ_TYPE_EFFECT) ||
	    BKE_sequence_effect_get_num_inputs(seq->type) == 0)
	{
		glEnable(GL_BLEND);
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		if (seq->flag & whichsel) glColor4ub(0, 0, 0, 80);
		else if (seq->flag & SELECT) glColor4ub(255, 255, 255, 30);
		else glColor4ub(0, 0, 0, 22);
		
		glRectf(rx1, y1, rx2, y2);
		
		if (seq->flag & whichsel) glColor4ub(255, 255, 255, 200);
		else glColor4ub(0, 0, 0, 50);
		
		glEnable(GL_POLYGON_SMOOTH);
		glBegin(GL_TRIANGLES);
		glVertex2fv(v1); glVertex2fv(v2); glVertex2fv(v3);
		glEnd();
		
		glDisable(GL_POLYGON_SMOOTH);
		glDisable(GL_BLEND);
	}
	
	if ((G.moving & G_TRANSFORM_SEQ) || (seq->flag & whichsel)) {
		const char col[4] = {255, 255, 255, 255};
		char numstr[32];
		size_t numstr_len;

		if (direction == SEQ_LEFTHANDLE) {
			numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", seq->startdisp);
			x1 = rx1;
			y1 -= 0.45f;
		}
		else {
			numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", seq->enddisp - 1);
			x1 = x2 - handsize_clamped * 0.75f;
			y1 = y2 + 0.05f;
		}
		UI_view2d_text_cache_add(v2d, x1, y1, numstr, numstr_len, col);
	}
}

/* draw info text on a sequence strip */
static void draw_seq_text(View2D *v2d, SpaceSeq *sseq, Sequence *seq, float x1, float x2, float y1, float y2, const unsigned char background_col[3])
{
	rctf rect;
	char str[32 + FILE_MAX];
	size_t str_len;
	const char *name = seq->name + 2;
	char col[4];

	/* note, all strings should include 'name' */
	if (name[0] == '\0')
		name = BKE_sequence_give_name(seq);

	if (seq->type == SEQ_TYPE_META || seq->type == SEQ_TYPE_ADJUSTMENT) {
		str_len = BLI_snprintf(str, sizeof(str), "%s | %d", name, seq->len);
	}
	else if (seq->type == SEQ_TYPE_SCENE) {
		if (seq->scene) {
			if (seq->scene_camera) {
				str_len = BLI_snprintf(str, sizeof(str), "%s: %s (%s) | %d",
				                       name, seq->scene->id.name + 2, ((ID *)seq->scene_camera)->name + 2, seq->len);
			}
			else {
				str_len = BLI_snprintf(str, sizeof(str), "%s: %s | %d",
				                       name, seq->scene->id.name + 2, seq->len);
			}
		}
		else {
			str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
			                       name, seq->len);
		}
	}
	else if (seq->type == SEQ_TYPE_MOVIECLIP) {
		if (seq->clip && !STREQ(name, seq->clip->id.name + 2)) {
			str_len = BLI_snprintf(str, sizeof(str), "%s: %s | %d",
			                       name, seq->clip->id.name + 2, seq->len);
		}
		else {
			str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
			                       name, seq->len);
		}
	}
	else if (seq->type == SEQ_TYPE_MASK) {
		if (seq->mask && !STREQ(name, seq->mask->id.name + 2)) {
			str_len = BLI_snprintf(str, sizeof(str), "%s: %s | %d",
			                       name, seq->mask->id.name + 2, seq->len);
		}
		else {
			str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
			                       name, seq->len);
		}
	}
	else if (seq->type == SEQ_TYPE_MULTICAM) {
		str_len = BLI_snprintf(str, sizeof(str), "Cam %s: %d",
		                       name, seq->multicam_source);
	}
	else if (seq->type == SEQ_TYPE_IMAGE) {
		str_len = BLI_snprintf(str, sizeof(str), "%s: %s%s | %d",
		                       name, seq->strip->dir, seq->strip->stripdata->name, seq->len);
	}
	else if (seq->type == SEQ_TYPE_TEXT) {
		TextVars *textdata = seq->effectdata;
		str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
		                       textdata->text, seq->startdisp);
	}
	else if (seq->type & SEQ_TYPE_EFFECT) {
		str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
		                       name, seq->len);
	}
	else if (seq->type == SEQ_TYPE_SOUND_RAM) {
		/* If a waveform is drawn, we don't want to overlay it with text,
		 * as it would make both hard to read. */
		if ((sseq->flag & SEQ_ALL_WAVEFORMS) || (seq->flag & SEQ_AUDIO_DRAW_WAVEFORM)) {
			str[0] = 0;
			str_len = 0;
		}
		else if (seq->sound) {
			str_len = BLI_snprintf(str, sizeof(str), "%s: %s | %d",
			                       name, seq->sound->name, seq->len);
		}
		else {
			str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
			                       name, seq->len);
		}
	}
	else if (seq->type == SEQ_TYPE_MOVIE) {
		str_len = BLI_snprintf(str, sizeof(str), "%s: %s%s | %d",
		                       name, seq->strip->dir, seq->strip->stripdata->name, seq->len);
	}
	else {
		/* should never get here!, but might with files from future */
		BLI_assert(0);

		str_len = BLI_snprintf(str, sizeof(str), "%s | %d",
		                       name, seq->len);
	}
	
	if (seq->flag & SELECT) {
		col[0] = col[1] = col[2] = 255;
	}
	else if ((((int)background_col[0] + (int)background_col[1] + (int)background_col[2]) / 3) < 50) {
		col[0] = col[1] = col[2] = 80; /* use lighter text color for dark background */
	}
	else {
		col[0] = col[1] = col[2] = 0;
	}
	col[3] = 255;

	rect.xmin = x1;
	rect.ymin = y1;
	rect.xmax = x2;
	rect.ymax = y2;

	UI_view2d_text_cache_add_rectf(v2d, &rect, str, str_len, col);
}

/* draws a shaded strip, made from gradient + flat color + gradient */
void draw_shadedstrip(Sequence *seq, unsigned char col[3], float x1, float y1, float x2, float y2)
{
	float ymid1, ymid2;
	
	if (seq->flag & SEQ_MUTE) {
		GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);
		GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_HALFTONE);
	}
	
	ymid1 = (y2 - y1) * 0.25f + y1;
	ymid2 = (y2 - y1) * 0.65f + y1;
	
	glBegin(GL_QUADS);
	
	if (seq->flag & SEQ_INVALID_EFFECT) { col[0] = 255; col[1] = 0; col[2] = 255; }
	else if (seq->flag & SELECT) UI_GetColorPtrShade3ubv(col, col, -50);
	/* else UI_GetColorPtrShade3ubv(col, col, 0); */ /* DO NOTHING */
	
	glColor3ubv(col);
	
	glVertex2f(x1, y1);
	glVertex2f(x2, y1);

	if (seq->flag & SEQ_INVALID_EFFECT) { col[0] = 255; col[1] = 0; col[2] = 255; }
	else if (seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 5);
	else UI_GetColorPtrShade3ubv(col, col, -5);

	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x2, ymid1);
	glVertex2f(x1, ymid1);
	
	glEnd();
	
	glRectf(x1,  ymid1,  x2,  ymid2);
	
	glBegin(GL_QUADS);
	
	glVertex2f(x1, ymid2);
	glVertex2f(x2, ymid2);
	
	if (seq->flag & SELECT) UI_GetColorPtrShade3ubv(col, col, -15);
	else UI_GetColorPtrShade3ubv(col, col, 25);
	
	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
	
	glEnd();
	
	if (seq->flag & SEQ_MUTE) {
		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
	}
}

void draw_sequence_extensions(Scene *scene, ARegion *ar, Sequence *seq)
{
	float x1, x2, y1, y2, pixely, a;
	unsigned char col[3], blendcol[3];
	View2D *v2d = &ar->v2d;
	
	x1 = seq->startdisp;
	x2 = seq->enddisp;
	
	y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
	y2 = seq->machine + SEQ_STRIP_OFSTOP;
	
	pixely = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);
	
	if (pixely <= 0) return;  /* can happen when the view is split/resized */
	
	blendcol[0] = blendcol[1] = blendcol[2] = 120;
	
	if (seq->startofs) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		color3ubv_from_seq(scene, seq, col);
		
		if (seq->flag & SELECT) {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.3, -40);
			glColor4ub(col[0], col[1], col[2], 170);
		}
		else {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.6, 0);
			glColor4ub(col[0], col[1], col[2], 110);
		}
		
		glRectf((float)(seq->start), y1 - SEQ_STRIP_OFSBOTTOM, x1, y1);
		
		if (seq->flag & SELECT) glColor4ub(col[0], col[1], col[2], 255);
		else glColor4ub(col[0], col[1], col[2], 160);
		
		fdrawbox((float)(seq->start), y1 - SEQ_STRIP_OFSBOTTOM, x1, y1);  //outline
		
		glDisable(GL_BLEND);
	}
	if (seq->endofs) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		color3ubv_from_seq(scene, seq, col);
		
		if (seq->flag & SELECT) {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.3, -40);
			glColor4ub(col[0], col[1], col[2], 170);
		}
		else {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.6, 0);
			glColor4ub(col[0], col[1], col[2], 110);
		}
		
		glRectf(x2, y2, (float)(seq->start + seq->len), y2 + SEQ_STRIP_OFSBOTTOM);
		
		if (seq->flag & SELECT) glColor4ub(col[0], col[1], col[2], 255);
		else glColor4ub(col[0], col[1], col[2], 160);
		
		fdrawbox(x2, y2, (float)(seq->start + seq->len), y2 + SEQ_STRIP_OFSBOTTOM); //outline
		
		glDisable(GL_BLEND);
	}
	if (seq->startstill) {
		color3ubv_from_seq(scene, seq, col);
		UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.75, 40);
		glColor3ubv((GLubyte *)col);
		
		draw_shadedstrip(seq, col, x1, y1, (float)(seq->start), y2);
		
		/* feint pinstripes, helps see exactly which is extended and which isn't,
		 * especially when the extension is very small */
		if (seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 24);
		else UI_GetColorPtrShade3ubv(col, col, -16);
		
		glColor3ubv((GLubyte *)col);
		
		for (a = y1; a < y2; a += pixely * 2.0f) {
			fdrawline(x1,  a,  (float)(seq->start),  a);
		}
	}
	if (seq->endstill) {
		color3ubv_from_seq(scene, seq, col);
		UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.75, 40);
		glColor3ubv((GLubyte *)col);
		
		draw_shadedstrip(seq, col, (float)(seq->start + seq->len), y1, x2, y2);
		
		/* feint pinstripes, helps see exactly which is extended and which isn't,
		 * especially when the extension is very small */
		if (seq->flag & SELECT) UI_GetColorPtrShade3ubv(col, col, 24);
		else UI_GetColorPtrShade3ubv(col, col, -16);
		
		glColor3ubv((GLubyte *)col);
		
		for (a = y1; a < y2; a += pixely * 2.0f) {
			fdrawline((float)(seq->start + seq->len),  a,  x2,  a);
		}
	}
}


/*
 * Draw a sequence strip, bounds check already made
 * ARegion is currently only used to get the windows width in pixels
 * so wave file sample drawing precision is zoom adjusted
 */
static void draw_seq_strip(const bContext *C, SpaceSeq *sseq, Scene *scene, ARegion *ar, Sequence *seq, int outline_tint, float pixelx)
{
	View2D *v2d = &ar->v2d;
	float x1, x2, y1, y2;
	unsigned char col[3], background_col[3], is_single_image;
	const float handsize_clamped = draw_seq_handle_size_get_clamped(seq, pixelx);

	/* we need to know if this is a single image/color or not for drawing */
	is_single_image = (char)BKE_sequence_single_check(seq);
	
	/* body */
	x1 = (seq->startstill) ? seq->start : seq->startdisp;
	y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
	x2 = (seq->endstill) ? (seq->start + seq->len) : seq->enddisp;
	y2 = seq->machine + SEQ_STRIP_OFSTOP;


	/* get the correct color per strip type*/
	//color3ubv_from_seq(scene, seq, col);
	color3ubv_from_seq(scene, seq, background_col);
	
	/* draw the main strip body */
	if (is_single_image) {  /* single image */
		draw_shadedstrip(seq, background_col,
		                 BKE_sequence_tx_get_final_left(seq, false), y1,
		                 BKE_sequence_tx_get_final_right(seq, false), y2);
	}
	else {  /* normal operation */
		draw_shadedstrip(seq, background_col, x1, y1, x2, y2);
	}

	if (!is_single_image) {
		if ((sseq->draw_flag & SEQ_DRAW_OFFSET_EXT) || (seq == special_seq_update)) {
			draw_sequence_extensions(scene, ar, seq);
		}
	}

	draw_seq_handle(v2d, seq, handsize_clamped, SEQ_LEFTHANDLE);
	draw_seq_handle(v2d, seq, handsize_clamped, SEQ_RIGHTHANDLE);
	
	/* draw the strip outline */
	x1 = seq->startdisp;
	x2 = seq->enddisp;
	
	/* draw sound wave */
	if (seq->type == SEQ_TYPE_SOUND_RAM) {
		if (!(sseq->flag & SEQ_NO_WAVEFORMS)) {
			drawseqwave(C, sseq, scene, seq, x1, y1, x2, y2, BLI_rctf_size_x(&ar->v2d.cur) / ar->winx);
		}
	}

	/* draw lock */
	if (seq->flag & SEQ_LOCK) {
		GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);
		glEnable(GL_BLEND);

		/* light stripes */
		glColor4ub(255, 255, 255, 32);
		GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_DIAG_STRIPES);
		glRectf(x1, y1, x2, y2);

		/* dark stripes */
		glColor4ub(0, 0, 0, 32);
		GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_DIAG_STRIPES_SWAP);
		glRectf(x1, y1, x2, y2);

		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
		glDisable(GL_BLEND);
	}

	if (!BKE_sequence_is_valid_check(seq)) {
		GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);

		/* panic! */
		glColor4ub(255, 0, 0, 255);
		GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_DIAG_STRIPES);
		glRectf(x1, y1, x2, y2);

		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
	}

	color3ubv_from_seq(scene, seq, col);
	if ((G.moving & G_TRANSFORM_SEQ) && (seq->flag & SELECT)) {
		if (seq->flag & SEQ_OVERLAP) {
			col[0] = 255; col[1] = col[2] = 40;
		}
		else
			UI_GetColorPtrShade3ubv(col, col, 120 + outline_tint);
	}
	else
		UI_GetColorPtrShade3ubv(col, col, outline_tint);
	
	if ((seq->type == SEQ_TYPE_META) ||
	    ((seq->type == SEQ_TYPE_SCENE) && (seq->flag & SEQ_SCENE_STRIPS)))
	{
		drawmeta_contents(scene, seq, x1, y1, x2, y2);
	}

	if (seq->flag & SEQ_MUTE) {
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0x8888);
	}
	
	glColor3ubv((GLubyte *)col);
	
	UI_draw_roundbox_shade_x(GL_LINE_LOOP, x1, y1, x2, y2, 0.0, 0.1, 0.0);
	
	if (seq->flag & SEQ_MUTE) {
		glDisable(GL_LINE_STIPPLE);
	}
	
	/* calculate if seq is long enough to print a name */
	x1 = seq->startdisp + handsize_clamped;
	x2 = seq->enddisp   - handsize_clamped;

	/* info text on the strip */
	if (x1 < v2d->cur.xmin) x1 = v2d->cur.xmin;
	else if (x1 > v2d->cur.xmax) x1 = v2d->cur.xmax;
	if (x2 < v2d->cur.xmin) x2 = v2d->cur.xmin;
	else if (x2 > v2d->cur.xmax) x2 = v2d->cur.xmax;

	/* nice text here would require changing the view matrix for texture text */
	if ((x2 - x1) / pixelx > 32) {
		draw_seq_text(v2d, sseq, seq, x1, x2, y1, y2, background_col);
	}
}

void sequencer_special_update_set(Sequence *seq)
{
	special_seq_update = seq;
}

Sequence *ED_sequencer_special_preview_get(void)
{
	return special_seq_update;
}

void ED_sequencer_special_preview_set(bContext *C, const int mval[2])
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	int hand;
	Sequence *seq;
	seq = find_nearest_seq(scene, &ar->v2d, &hand, mval);
	sequencer_special_update_set(seq);
}

void ED_sequencer_special_preview_clear(void)
{
	sequencer_special_update_set(NULL);
}

ImBuf *sequencer_ibuf_get(struct Main *bmain, Scene *scene, SpaceSeq *sseq, int cfra, int frame_ofs, const char *viewname)
{
	SeqRenderData context;
	ImBuf *ibuf;
	int rectx, recty;
	float render_size;
	float proxy_size = 100.0;
	short is_break = G.is_break;

	render_size = sseq->render_size;
	if (render_size == 0) {
		render_size = scene->r.size;
	}
	else {
		proxy_size = render_size;
	}

	if (render_size < 0) {
		return NULL;
	}

	rectx = (render_size * (float)scene->r.xsch) / 100.0f + 0.5f;
	recty = (render_size * (float)scene->r.ysch) / 100.0f + 0.5f;

	BKE_sequencer_new_render_data(
	        bmain->eval_ctx, bmain, scene,
	        rectx, recty, proxy_size,
	        &context);
	context.view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);

	/* sequencer could start rendering, in this case we need to be sure it wouldn't be canceled
	 * by Esc pressed somewhere in the past
	 */
	G.is_break = false;

	if (special_seq_update)
		ibuf = BKE_sequencer_give_ibuf_direct(&context, cfra + frame_ofs, special_seq_update);
	else if (!U.prefetchframes) // XXX || (G.f & G_PLAYANIM) == 0) {
		ibuf = BKE_sequencer_give_ibuf(&context, cfra + frame_ofs, sseq->chanshown);
	else
		ibuf = BKE_sequencer_give_ibuf_threaded(&context, cfra + frame_ofs, sseq->chanshown);

	/* restore state so real rendering would be canceled (if needed) */
	G.is_break = is_break;

	return ibuf;
}

static void sequencer_check_scopes(SequencerScopes *scopes, ImBuf *ibuf)
{
	if (scopes->reference_ibuf != ibuf) {
		if (scopes->zebra_ibuf) {
			IMB_freeImBuf(scopes->zebra_ibuf);
			scopes->zebra_ibuf = NULL;
		}

		if (scopes->waveform_ibuf) {
			IMB_freeImBuf(scopes->waveform_ibuf);
			scopes->waveform_ibuf = NULL;
		}

		if (scopes->sep_waveform_ibuf) {
			IMB_freeImBuf(scopes->sep_waveform_ibuf);
			scopes->sep_waveform_ibuf = NULL;
		}

		if (scopes->vector_ibuf) {
			IMB_freeImBuf(scopes->vector_ibuf);
			scopes->vector_ibuf = NULL;
		}

		if (scopes->histogram_ibuf) {
			IMB_freeImBuf(scopes->histogram_ibuf);
			scopes->histogram_ibuf = NULL;
		}
	}
}

static ImBuf *sequencer_make_scope(Scene *scene, ImBuf *ibuf, ImBuf *(*make_scope_cb) (ImBuf *ibuf))
{
	ImBuf *display_ibuf = IMB_dupImBuf(ibuf);
	ImBuf *scope;
	
	IMB_colormanagement_imbuf_make_display_space(display_ibuf, &scene->view_settings,
		                                             &scene->display_settings);

	scope = make_scope_cb(display_ibuf);

	IMB_freeImBuf(display_ibuf);

	return scope;
}

static void sequencer_display_size(Scene *scene, SpaceSeq *sseq, float r_viewrect[2])
{
	float render_size, proxy_size;

	if (sseq->render_size == SEQ_PROXY_RENDER_SIZE_SCENE) {
		render_size = (float)scene->r.size / 100.0f;
		proxy_size = 1.0f;
	}
	else {
		render_size = (float)sseq->render_size / 100.0f;
		proxy_size = render_size;
	}

	r_viewrect[0] = (render_size * (float)scene->r.xsch);
	r_viewrect[1] = (render_size * (float)scene->r.ysch);

	/* rectx = viewrectx + 0.5f; */ /* UNUSED */
	/* recty = viewrecty + 0.5f; */ /* UNUSED */

	if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
		r_viewrect[0] *= scene->r.xasp / scene->r.yasp;
		r_viewrect[0] /= proxy_size;
		r_viewrect[1] /= proxy_size;
	}
}

static void sequencer_draw_gpencil(const bContext *C)
{
	/* draw grease-pencil (image aligned) */
	ED_gpencil_draw_2dimage(C);

	/* ortho at pixel level */
	UI_view2d_view_restore(C);

	/* draw grease-pencil (screen aligned) */
	ED_gpencil_draw_view2d(C, 0);
}

/* draws content borders plus safety borders if needed */
static void sequencer_draw_borders(const SpaceSeq *sseq, const View2D *v2d, const Scene *scene)
{
	float x1 = v2d->tot.xmin;
	float y1 = v2d->tot.ymin;
	float x2 = v2d->tot.xmax;
	float y2 = v2d->tot.ymax;

	glLineWidth(1.0f);

	/* border */
	setlinestyle(3);

	UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 1.0, 0);

	glBegin(GL_LINE_LOOP);
	glVertex2f(x1 - 0.5f, y1 - 0.5f);
	glVertex2f(x1 - 0.5f, y2 + 0.5f);
	glVertex2f(x2 + 0.5f, y2 + 0.5f);
	glVertex2f(x2 + 0.5f, y1 - 0.5f);
	glEnd();

	/* safety border */
	if (sseq->flag & SEQ_SHOW_SAFE_MARGINS) {
		UI_draw_safe_areas(
		        x1, x2, y1, y2,
		        scene->safe_areas.title,
		        scene->safe_areas.action);

		if (sseq->flag & SEQ_SHOW_SAFE_CENTER) {
			UI_draw_safe_areas(
			        x1, x2, y1, y2,
			        scene->safe_areas.title_center,
			        scene->safe_areas.action_center);
		}
	}

	setlinestyle(0);
}

/* draws checkerboard background for transparent content */
static void sequencer_draw_background(
        const SpaceSeq *sseq, View2D *v2d, const float viewrect[2], const bool draw_overlay)
{
	/* setting up the view */
	UI_view2d_totRect_set(v2d, viewrect[0] + 0.5f, viewrect[1] + 0.5f);
	UI_view2d_curRect_validate(v2d);
	UI_view2d_view_ortho(v2d);

	/* only draw alpha for main buffer */
	if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
		if ((sseq->flag & SEQ_USE_ALPHA) && !draw_overlay) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			fdrawcheckerboard(v2d->tot.xmin, v2d->tot.ymin, v2d->tot.xmax, v2d->tot.ymax);
			glColor4f(1.0, 1.0, 1.0, 1.0);
		}
	}
}

void draw_image_seq(const bContext *C, Scene *scene, ARegion *ar, SpaceSeq *sseq, int cfra, int frame_ofs, bool draw_overlay, bool draw_backdrop)
{
	struct Main *bmain = CTX_data_main(C);
	struct ImBuf *ibuf = NULL;
	struct ImBuf *scope = NULL;
	struct View2D *v2d = &ar->v2d;
	/* int rectx, recty; */ /* UNUSED */
	float viewrect[2];
	float col[3];
	GLuint texid;
	void *display_buffer;
	void *cache_handle = NULL;
	const bool is_imbuf = ED_space_sequencer_check_show_imbuf(sseq);
	int format, type;
	bool glsl_used = false;
	const bool draw_gpencil = ((sseq->flag & SEQ_SHOW_GPENCIL) && sseq->gpd);
	const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
	bool draw_metadata = false;

	if (G.is_rendering == false && (scene->r.seq_prev_type) == OB_RENDER) {
		/* stop all running jobs, except screen one. currently previews frustrate Render
		 * needed to make so sequencer's rendering doesn't conflict with compositor
		 */
		WM_jobs_kill_type(CTX_wm_manager(C), NULL, WM_JOB_TYPE_COMPOSITE);

		/* in case of final rendering used for preview, kill all previews,
		 * otherwise threading conflict will happen in rendering module
		 */
		WM_jobs_kill_type(CTX_wm_manager(C), NULL, WM_JOB_TYPE_RENDER_PREVIEW);
	}

	if ((!draw_overlay || sseq->overlay_type == SEQ_DRAW_OVERLAY_REFERENCE) && !draw_backdrop) {
		UI_GetThemeColor3fv(TH_SEQ_PREVIEW, col);
		glClearColor(col[0], col[1], col[2], 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	/* without this colors can flicker from previous opengl state */
	glColor4ub(255, 255, 255, 255);

	/* only initialize the preview if a render is in progress */
	if (G.is_rendering)
		return;

	if (sseq->render_size == SEQ_PROXY_RENDER_SIZE_NONE) {
		return;
	}

	/* for now we only support Left/Right */
	ibuf = sequencer_ibuf_get(bmain, scene, sseq, cfra, frame_ofs, names[sseq->multiview_eye]);

	if ((ibuf == NULL) ||
	    (ibuf->rect == NULL && ibuf->rect_float == NULL))
	{
		/* gpencil can also be drawn without a valid imbuf */
		if ((draw_gpencil && is_imbuf) && !draw_overlay) {
			sequencer_display_size(scene, sseq, viewrect);

			sequencer_draw_background(sseq, v2d, viewrect, false);
			sequencer_draw_borders(sseq, v2d, scene);

			sequencer_draw_gpencil(C);
		}
		return;
	}

	sequencer_display_size(scene, sseq, viewrect);

	if (!draw_backdrop && (sseq->mainb != SEQ_DRAW_IMG_IMBUF || sseq->zebra != 0)) {
		SequencerScopes *scopes = &sseq->scopes;

		sequencer_check_scopes(scopes, ibuf);

		switch (sseq->mainb) {
			case SEQ_DRAW_IMG_IMBUF:
				if (!scopes->zebra_ibuf) {
					ImBuf *display_ibuf = IMB_dupImBuf(ibuf);

					if (display_ibuf->rect_float) {
						IMB_colormanagement_imbuf_make_display_space(display_ibuf, &scene->view_settings,
						                                             &scene->display_settings);
					}
					scopes->zebra_ibuf = make_zebra_view_from_ibuf(display_ibuf, sseq->zebra);
					IMB_freeImBuf(display_ibuf);
				}
				scope = scopes->zebra_ibuf;
				break;
			case SEQ_DRAW_IMG_WAVEFORM:
				if ((sseq->flag & SEQ_DRAW_COLOR_SEPARATED) != 0) {
					if (!scopes->sep_waveform_ibuf)
						scopes->sep_waveform_ibuf = sequencer_make_scope(scene, ibuf, make_sep_waveform_view_from_ibuf);
					scope = scopes->sep_waveform_ibuf;
				}
				else {
					if (!scopes->waveform_ibuf)
						scopes->waveform_ibuf = sequencer_make_scope(scene, ibuf, make_waveform_view_from_ibuf);
					scope = scopes->waveform_ibuf;
				}
				break;
			case SEQ_DRAW_IMG_VECTORSCOPE:
				if (!scopes->vector_ibuf)
					scopes->vector_ibuf = sequencer_make_scope(scene, ibuf, make_vectorscope_view_from_ibuf);
				scope = scopes->vector_ibuf;
				break;
			case SEQ_DRAW_IMG_HISTOGRAM:
				if (!scopes->histogram_ibuf)
					scopes->histogram_ibuf = sequencer_make_scope(scene, ibuf, make_histogram_view_from_ibuf);
				scope = scopes->histogram_ibuf;
				break;
		}

		/* future files may have new scopes we don't catch above */
		if (scope) {
			scopes->reference_ibuf = ibuf;
			if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
				/* scopes drawn in image preview use viewrect from orig ibuf - currently that's only zebra */
			}
			else {
				viewrect[0] = scope->x;
				viewrect[1] = scope->y;
			}
		}
		else {
			scopes->reference_ibuf = NULL;
		}
	}

	if (!draw_backdrop) {
		sequencer_draw_background(sseq, v2d, viewrect, draw_overlay);
	}

	if (scope) {
		IMB_freeImBuf(ibuf);
		ibuf = scope;

		if (ibuf->rect_float && ibuf->rect == NULL) {
			IMB_rect_from_float(ibuf);
		}

		display_buffer = (unsigned char *)ibuf->rect;
		format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
	}
	else {
		bool force_fallback = false;

		force_fallback |= (U.image_draw_method != IMAGE_DRAW_METHOD_GLSL);
		force_fallback |= (ibuf->dither != 0.0f);

		if (force_fallback) {
			/* Fallback to CPU based color space conversion */
			glsl_used = false;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			display_buffer = NULL;
		}
		else if (ibuf->rect_float) {
			display_buffer = ibuf->rect_float;

			if (ibuf->channels == 4) {
				format = GL_RGBA;
			}
			else if (ibuf->channels == 3) {
				format = GL_RGB;
			}
			else {
				BLI_assert(!"Incompatible number of channels for float buffer in sequencer");
				format = GL_RGBA;
				display_buffer = NULL;
			}

			type = GL_FLOAT;

			if (ibuf->float_colorspace) {
				glsl_used = IMB_colormanagement_setup_glsl_draw_from_space_ctx(C, ibuf->float_colorspace, ibuf->dither, true);
			}
			else {
				glsl_used = IMB_colormanagement_setup_glsl_draw_ctx(C, ibuf->dither, true);
			}
		}
		else if (ibuf->rect) {
			display_buffer = ibuf->rect;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;

			glsl_used = IMB_colormanagement_setup_glsl_draw_from_space_ctx(C, ibuf->rect_colorspace, ibuf->dither, false);
		}
		else {
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			display_buffer = NULL;
		}

		/* there's a data to be displayed, but GLSL is not initialized
		 * properly, in this case we fallback to CPU-based display transform
		 */
		if ((ibuf->rect || ibuf->rect_float) && !glsl_used) {
			display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
		}
	}

	glColor4f(1.0, 1.0, 1.0, 1.0);

	GPU_basic_shader_bind(GPU_SHADER_TEXTURE_2D | GPU_SHADER_USE_COLOR);
	glGenTextures(1, (GLuint *)&texid);

	glBindTexture(GL_TEXTURE_2D, texid);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if (type == GL_FLOAT)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, ibuf->x, ibuf->y, 0, format, type, display_buffer);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ibuf->x, ibuf->y, 0, format, type, display_buffer);

	if (draw_backdrop) {
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
	}
	glBegin(GL_QUADS);

	if (draw_overlay) {
		if (sseq->overlay_type == SEQ_DRAW_OVERLAY_RECT) {
			rctf tot_clip;
			tot_clip.xmin = v2d->tot.xmin + (fabsf(BLI_rctf_size_x(&v2d->tot)) * scene->ed->over_border.xmin);
			tot_clip.ymin = v2d->tot.ymin + (fabsf(BLI_rctf_size_y(&v2d->tot)) * scene->ed->over_border.ymin);
			tot_clip.xmax = v2d->tot.xmin + (fabsf(BLI_rctf_size_x(&v2d->tot)) * scene->ed->over_border.xmax);
			tot_clip.ymax = v2d->tot.ymin + (fabsf(BLI_rctf_size_y(&v2d->tot)) * scene->ed->over_border.ymax);

			glTexCoord2f(scene->ed->over_border.xmin, scene->ed->over_border.ymin); glVertex2f(tot_clip.xmin, tot_clip.ymin);
			glTexCoord2f(scene->ed->over_border.xmin, scene->ed->over_border.ymax); glVertex2f(tot_clip.xmin, tot_clip.ymax);
			glTexCoord2f(scene->ed->over_border.xmax, scene->ed->over_border.ymax); glVertex2f(tot_clip.xmax, tot_clip.ymax);
			glTexCoord2f(scene->ed->over_border.xmax, scene->ed->over_border.ymin); glVertex2f(tot_clip.xmax, tot_clip.ymin);
		}
		else if (sseq->overlay_type == SEQ_DRAW_OVERLAY_REFERENCE) {
			glTexCoord2f(0.0f, 0.0f); glVertex2f(v2d->tot.xmin, v2d->tot.ymin);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(v2d->tot.xmin, v2d->tot.ymax);
			glTexCoord2f(1.0f, 1.0f); glVertex2f(v2d->tot.xmax, v2d->tot.ymax);
			glTexCoord2f(1.0f, 0.0f); glVertex2f(v2d->tot.xmax, v2d->tot.ymin);
		}
	}
	else if (draw_backdrop) {
		float aspect;
		float image_aspect = viewrect[0] / viewrect[1];
		float imagex, imagey;

		aspect = BLI_rcti_size_x(&ar->winrct) / (float)BLI_rcti_size_y(&ar->winrct);

		if (aspect >= image_aspect) {
			imagex = image_aspect / aspect;
			imagey = 1.0f;
		}
		else {
			imagex = 1.0f;
			imagey = aspect / image_aspect;
		}

		glTexCoord2f(0.0f, 0.0f); glVertex2f(-imagex, -imagey);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-imagex, imagey);
		glTexCoord2f(1.0f, 1.0f); glVertex2f(imagex, imagey);
		glTexCoord2f(1.0f, 0.0f); glVertex2f(imagex, -imagey);
	}
	else {
		draw_metadata = ((sseq->flag & SEQ_SHOW_METADATA) != 0);

		glTexCoord2f(0.0f, 0.0f); glVertex2f(v2d->tot.xmin, v2d->tot.ymin);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(v2d->tot.xmin, v2d->tot.ymax);
		glTexCoord2f(1.0f, 1.0f); glVertex2f(v2d->tot.xmax, v2d->tot.ymax);
		glTexCoord2f(1.0f, 0.0f); glVertex2f(v2d->tot.xmax, v2d->tot.ymin);
	}
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
	if (sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->flag & SEQ_USE_ALPHA)
		glDisable(GL_BLEND);
	glDeleteTextures(1, &texid);

	if (glsl_used)
		IMB_colormanagement_finish_glsl_draw();

	if (cache_handle)
		IMB_display_buffer_release(cache_handle);

	if (!scope)
		IMB_freeImBuf(ibuf);

	if (draw_metadata) {
		ED_region_image_metadata_draw(0.0, 0.0, ibuf, &v2d->tot, 1.0, 1.0);
	}

	if (draw_backdrop) {
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		return;
	}

	if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
		sequencer_draw_borders(sseq, v2d, scene);
	}

	if (draw_gpencil && is_imbuf) {
		sequencer_draw_gpencil(C);
	}
	else {
		/* ortho at pixel level */
		UI_view2d_view_restore(C);
	}


	/* NOTE: sequencer mask editing isnt finished, the draw code is working but editing not,
	 * for now just disable drawing since the strip frame will likely be offset */

	//if (sc->mode == SC_MODE_MASKEDIT) {
	if (0 && sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
		Mask *mask = BKE_sequencer_mask_get(scene);

		if (mask) {
			int width, height;
			float aspx = 1.0f, aspy = 1.0f;
			// ED_mask_get_size(C, &width, &height);

			//Scene *scene = CTX_data_scene(C);
			width = (scene->r.size * scene->r.xsch) / 100;
			height = (scene->r.size * scene->r.ysch) / 100;

			ED_mask_draw_region(mask, ar,
			                    0, 0, 0,  /* TODO */
			                    width, height,
			                    aspx, aspy,
			                    false, true,
			                    NULL, C);
		}
	}
}

#if 0
void drawprefetchseqspace(Scene *scene, ARegion *UNUSED(ar), SpaceSeq *sseq)
{
	int rectx, recty;
	int render_size = sseq->render_size;
	int proxy_size = 100.0; 
	if (render_size == 0) {
		render_size = scene->r.size;
	}
	else {
		proxy_size = render_size;
	}
	if (render_size < 0) {
		return;
	}

	rectx = (render_size * scene->r.xsch) / 100;
	recty = (render_size * scene->r.ysch) / 100;

	if (sseq->mainb != SEQ_DRAW_SEQUENCE) {
		give_ibuf_prefetch_request(
		    rectx, recty, (scene->r.cfra), sseq->chanshown,
		    proxy_size);
	}
}
#endif

/* draw backdrop of the sequencer strips view */
static void draw_seq_backdrop(View2D *v2d)
{
	int i;
	
	/* darker gray overlay over the view backdrop */
	UI_ThemeColorShade(TH_BACK, -20);
	glRectf(v2d->cur.xmin,  -1.0,  v2d->cur.xmax,  1.0);

	/* Alternating horizontal stripes */
	i = max_ii(1, ((int)v2d->cur.ymin) - 1);

	glBegin(GL_QUADS);
	while (i < v2d->cur.ymax) {
		if (((int)i) & 1)
			UI_ThemeColorShade(TH_BACK, -15);
		else
			UI_ThemeColorShade(TH_BACK, -25);
			
		glVertex2f(v2d->cur.xmax, i);
		glVertex2f(v2d->cur.xmin, i);
		glVertex2f(v2d->cur.xmin, i + 1);
		glVertex2f(v2d->cur.xmax, i + 1);

		i += 1.0;
	}
	glEnd();
	
	/* Darker lines separating the horizontal bands */
	i = max_ii(1, ((int)v2d->cur.ymin) - 1);
	UI_ThemeColor(TH_GRID);
	
	glBegin(GL_LINES);
	while (i < v2d->cur.ymax) {
		glVertex2f(v2d->cur.xmax, i);
		glVertex2f(v2d->cur.xmin, i);
			
		i += 1.0;
	}
	glEnd();
}

/* draw the contents of the sequencer strips view */
static void draw_seq_strips(const bContext *C, Editing *ed, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	View2D *v2d = &ar->v2d;
	SpaceSeq *sseq = CTX_wm_space_seq(C);
	Sequence *last_seq = BKE_sequencer_active_get(scene);
	int sel = 0, j;
	float pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
	
	/* loop through twice, first unselected, then selected */
	for (j = 0; j < 2; j++) {
		Sequence *seq;
		int outline_tint = (j) ? -60 : -150; /* highlighting around strip edges indicating selection */
		
		/* loop through strips, checking for those that are visible */
		for (seq = ed->seqbasep->first; seq; seq = seq->next) {
			/* boundbox and selection tests for NOT drawing the strip... */
			if ((seq->flag & SELECT) != sel) continue;
			else if (seq == last_seq) continue;
			else if (min_ii(seq->startdisp, seq->start) > v2d->cur.xmax) continue;
			else if (max_ii(seq->enddisp, seq->start + seq->len) < v2d->cur.xmin) continue;
			else if (seq->machine + 1.0f < v2d->cur.ymin) continue;
			else if (seq->machine > v2d->cur.ymax) continue;
			
			/* strip passed all tests unscathed... so draw it now */
			draw_seq_strip(C, sseq, scene, ar, seq, outline_tint, pixelx);
		}
		
		/* draw selected next time round */
		sel = SELECT;
	}
	
	/* draw the last selected last (i.e. 'active' in other parts of Blender), removes some overlapping error */
	if (last_seq)
		draw_seq_strip(C, sseq, scene, ar, last_seq, 120, pixelx);

	/* draw highlight when previewing a single strip */
	if (special_seq_update) {
		const Sequence *seq = special_seq_update;
		glEnable(GL_BLEND);
		glColor4ub(255, 255, 255, 48);
		glRectf(seq->startdisp, seq->machine + SEQ_STRIP_OFSBOTTOM, seq->enddisp, seq->machine + SEQ_STRIP_OFSTOP);
		glDisable(GL_BLEND);
	}
}

static void seq_draw_sfra_efra(Scene *scene, View2D *v2d)
{
	const Editing *ed = BKE_sequencer_editing_get(scene, false);
	const int frame_sta = PSFRA;
	const int frame_end = PEFRA + 1;

	glEnable(GL_BLEND);
	
	/* draw darkened area outside of active timeline 
	 * frame range used is preview range or scene range */
	UI_ThemeColorShadeAlpha(TH_BACK, -25, -100);

	if (frame_sta < frame_end) {
		glRectf(v2d->cur.xmin, v2d->cur.ymin, (float)frame_sta, v2d->cur.ymax);
		glRectf((float)frame_end, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
	}
	else {
		glRectf(v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
	}

	UI_ThemeColorShade(TH_BACK, -60);
	/* thin lines where the actual frames are */
	fdrawline(frame_sta, v2d->cur.ymin, frame_sta, v2d->cur.ymax);
	fdrawline(frame_end, v2d->cur.ymin, frame_end, v2d->cur.ymax);

	if (ed && !BLI_listbase_is_empty(&ed->metastack)) {
		MetaStack *ms = ed->metastack.last;

		glColor4ub(255, 255, 255, 8);
		glRectf(ms->disp_range[0], v2d->cur.ymin, ms->disp_range[1], v2d->cur.ymax);

		UI_ThemeColorShade(TH_BACK, -40);

		fdrawline(ms->disp_range[0], v2d->cur.ymin, ms->disp_range[0], v2d->cur.ymax);
		fdrawline(ms->disp_range[1], v2d->cur.ymin, ms->disp_range[1], v2d->cur.ymax);
	}

	glDisable(GL_BLEND);
}

/* Draw Timeline/Strip Editor Mode for Sequencer */
void draw_timeline_seq(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	SpaceSeq *sseq = CTX_wm_space_seq(C);
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;
	short unit = 0, flag = 0;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	if (ed && ed->metastack.first) 
		glClearColor(col[0], col[1], col[2] - 0.1f, 0.0f);
	else 
		glClearColor(col[0], col[1], col[2], 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);
	
	
	/* calculate extents of sequencer strips/data 
	 * NOTE: needed for the scrollers later
	 */
	boundbox_seq(scene, &v2d->tot);
	
	
	/* draw backdrop */
	draw_seq_backdrop(v2d);
	
	/* regular grid-pattern over the rest of the view (i.e. 25-frame grid lines) */
	// NOTE: the gridlines are currently spaced every 25 frames, which is only fine for 25 fps, but maybe not for 30...
	UI_view2d_constant_grid_draw(v2d);

	/* Only draw backdrop in pure sequence view. */
	if (sseq->view == SEQ_VIEW_SEQUENCE && sseq->draw_flag & SEQ_DRAW_BACKDROP) {
		draw_image_seq(C, scene, ar, sseq, scene->r.cfra, 0, false, true);
		UI_view2d_view_ortho(v2d);
	}
		
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);
	
	seq_draw_sfra_efra(scene, v2d);

	/* sequence strips (if there is data available to be drawn) */
	if (ed) {
		/* draw the data */
		draw_seq_strips(C, ed, ar);
		
		/* text draw cached (for sequence names), in pixelspace now */
		UI_view2d_text_cache_draw(ar);
	}
	
	/* current frame */
	UI_view2d_view_ortho(v2d);
	if ((sseq->flag & SEQ_DRAWFRAMES) == 0)      flag |= DRAWCFRA_UNIT_SECONDS;
	if ((sseq->flag & SEQ_NO_DRAW_CFRANUM) == 0) flag |= DRAWCFRA_SHOW_NUMBOX;
	ANIM_draw_cfra(C, v2d, flag);
	
	/* markers */
	UI_view2d_view_orthoSpecial(ar, v2d, 1);
	ED_markers_draw(C, DRAW_MARKERS_LINES | DRAW_MARKERS_MARGIN);
	
	/* preview range */
	UI_view2d_view_ortho(v2d);
	ANIM_draw_previewrange(C, v2d, 1);

	/* overlap playhead */
	if (scene->ed && scene->ed->over_flag & SEQ_EDIT_OVERLAY_SHOW) {
		int cfra_over = (scene->ed->over_flag & SEQ_EDIT_OVERLAY_ABS) ? scene->ed->over_cfra : scene->r.cfra + scene->ed->over_ofs;
		glColor3f(0.2, 0.2, 0.2);
		// glRectf(cfra_over, v2d->cur.ymin, scene->ed->over_ofs + scene->r.cfra + 1, v2d->cur.ymax);

		glBegin(GL_LINES);
		glVertex2f(cfra_over, v2d->cur.ymin);
		glVertex2f(cfra_over, v2d->cur.ymax);
		glEnd();

	}
	
	/* callback */
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	unit = (sseq->flag & SEQ_DRAWFRAMES) ? V2D_UNIT_FRAMES : V2D_UNIT_SECONDS;
	scrollers = UI_view2d_scrollers_calc(C, v2d, unit, V2D_GRID_CLAMP, V2D_UNIT_VALUES, V2D_GRID_CLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}


