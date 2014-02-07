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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/seqmodifier.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "DNA_sequence_types.h"

#include "BKE_colortools.h"
#include "BKE_sequencer.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

static SequenceModifierTypeInfo *modifiersTypes[NUM_SEQUENCE_MODIFIER_TYPES];
static int modifierTypesInit = FALSE;

/*********************** Modifiers *************************/

typedef void (*modifier_apply_threaded_cb) (int width, int height, unsigned char *rect, float *rect_float,
                                            unsigned char *mask_rect, float *mask_rect_float, void *data_v);

typedef struct ModifierInitData {
	ImBuf *ibuf;
	ImBuf *mask;
	void *user_data;

	modifier_apply_threaded_cb apply_callback;
} ModifierInitData;

typedef struct ModifierThread {
	int width, height;

	unsigned char *rect, *mask_rect;
	float *rect_float, *mask_rect_float;

	void *user_data;

	modifier_apply_threaded_cb apply_callback;
} ModifierThread;


static ImBuf *modifier_mask_get(SequenceModifierData *smd, const SeqRenderData *context, int cfra, int make_float)
{
	return BKE_sequencer_render_mask_input(context, smd->mask_input_type, smd->mask_sequence, smd->mask_id, cfra, make_float);
}

static void modifier_init_handle(void *handle_v, int start_line, int tot_line, void *init_data_v)
{
	ModifierThread *handle = (ModifierThread *) handle_v;
	ModifierInitData *init_data = (ModifierInitData *) init_data_v;
	ImBuf *ibuf = init_data->ibuf;
	ImBuf *mask = init_data->mask;

	int offset = 4 * start_line * ibuf->x;

	memset(handle, 0, sizeof(ModifierThread));

	handle->width = ibuf->x;
	handle->height = tot_line;
	handle->apply_callback = init_data->apply_callback;
	handle->user_data = init_data->user_data;

	if (ibuf->rect)
		handle->rect = (unsigned char *) ibuf->rect + offset;

	if (ibuf->rect_float)
		handle->rect_float = ibuf->rect_float + offset;

	if (mask) {
		if (mask->rect)
			handle->mask_rect = (unsigned char *) mask->rect + offset;

		if (mask->rect_float)
			handle->mask_rect_float = mask->rect_float + offset;
	}
	else {
		handle->mask_rect = NULL;
		handle->mask_rect_float = NULL;
	}
}

static void *modifier_do_thread(void *thread_data_v)
{
	ModifierThread *td = (ModifierThread *) thread_data_v;

	td->apply_callback(td->width, td->height, td->rect, td->rect_float, td->mask_rect, td->mask_rect_float, td->user_data);

	return NULL;
}

static void modifier_apply_threaded(ImBuf *ibuf, ImBuf *mask, modifier_apply_threaded_cb apply_callback, void *user_data)
{
	ModifierInitData init_data;

	init_data.ibuf = ibuf;
	init_data.mask = mask;
	init_data.user_data = user_data;

	init_data.apply_callback = apply_callback;

	IMB_processor_apply_threaded(ibuf->y, sizeof(ModifierThread), &init_data,
	                             modifier_init_handle, modifier_do_thread);
}

/* **** Color Balance Modifier **** */

static void colorBalance_init_data(SequenceModifierData *smd)
{
	ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *) smd;
	int c;

	cbmd->color_multiply = 1.0f;

	for (c = 0; c < 3; c++) {
		cbmd->color_balance.lift[c] = 1.0f;
		cbmd->color_balance.gamma[c] = 1.0f;
		cbmd->color_balance.gain[c] = 1.0f;
	}
}

static void colorBalance_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
	ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *) smd;

	BKE_sequencer_color_balance_apply(&cbmd->color_balance, ibuf, cbmd->color_multiply, FALSE, mask);
}

static SequenceModifierTypeInfo seqModifier_ColorBalance = {
	CTX_N_(BLF_I18NCONTEXT_ID_SEQUENCE, "Color Balance"),  /* name */
	"ColorBalanceModifierData",                            /* struct_name */
	sizeof(ColorBalanceModifierData),                      /* struct_size */
	colorBalance_init_data,                                /* init_data */
	NULL,                                                  /* free_data */
	NULL,                                                  /* copy_data */
	colorBalance_apply                                     /* apply */
};

/* **** Curves Modifier **** */

static void curves_init_data(SequenceModifierData *smd)
{
	CurvesModifierData *cmd = (CurvesModifierData *) smd;

	curvemapping_set_defaults(&cmd->curve_mapping, 4, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void curves_free_data(SequenceModifierData *smd)
{
	CurvesModifierData *cmd = (CurvesModifierData *) smd;

	curvemapping_free_data(&cmd->curve_mapping);
}

static void curves_copy_data(SequenceModifierData *target, SequenceModifierData *smd)
{
	CurvesModifierData *cmd = (CurvesModifierData *) smd;
	CurvesModifierData *cmd_target = (CurvesModifierData *) target;

	curvemapping_copy_data(&cmd_target->curve_mapping, &cmd->curve_mapping);
}

static void curves_apply_threaded(int width, int height, unsigned char *rect, float *rect_float,
                                  unsigned char *mask_rect, float *mask_rect_float, void *data_v)
{
	CurveMapping *curve_mapping = (CurveMapping *) data_v;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int pixel_index = (y * width + x) * 4;

			if (rect_float) {
				float *pixel = rect_float + pixel_index;
				float result[3];

				curvemapping_evaluate_premulRGBF(curve_mapping, result, pixel);

				if (mask_rect_float) {
					float *m = mask_rect_float + pixel_index;

					pixel[0] = pixel[0] * (1.0f - m[0]) + result[0] * m[0];
					pixel[1] = pixel[1] * (1.0f - m[1]) + result[1] * m[1];
					pixel[2] = pixel[2] * (1.0f - m[2]) + result[2] * m[2];
				}
				else {
					pixel[0] = result[0];
					pixel[1] = result[1];
					pixel[2] = result[2];
				}
			}
			if (rect) {
				unsigned char *pixel = rect + pixel_index;
				float result[3], tempc[4];

				straight_uchar_to_premul_float(tempc, pixel);

				curvemapping_evaluate_premulRGBF(curve_mapping, result, tempc);

				if (mask_rect) {
					float t[3];

					rgb_uchar_to_float(t, mask_rect + pixel_index);

					tempc[0] = tempc[0] * (1.0f - t[0]) + result[0] * t[0];
					tempc[1] = tempc[1] * (1.0f - t[1]) + result[1] * t[1];
					tempc[2] = tempc[2] * (1.0f - t[2]) + result[2] * t[2];
				}
				else {
					tempc[0] = result[0];
					tempc[1] = result[1];
					tempc[2] = result[2];
				}

				premul_float_to_straight_uchar(pixel, tempc);
			}
		}
	}
}

static void curves_apply(struct SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
	CurvesModifierData *cmd = (CurvesModifierData *) smd;

	float black[3] = {0.0f, 0.0f, 0.0f};
	float white[3] = {1.0f, 1.0f, 1.0f};

	curvemapping_initialize(&cmd->curve_mapping);

	curvemapping_premultiply(&cmd->curve_mapping, 0);
	curvemapping_set_black_white(&cmd->curve_mapping, black, white);

	modifier_apply_threaded(ibuf, mask, curves_apply_threaded, &cmd->curve_mapping);

	curvemapping_premultiply(&cmd->curve_mapping, 1);
}

static SequenceModifierTypeInfo seqModifier_Curves = {
	CTX_N_(BLF_I18NCONTEXT_ID_SEQUENCE, "Curves"),   /* name */
	"CurvesModifierData",                            /* struct_name */
	sizeof(CurvesModifierData),                      /* struct_size */
	curves_init_data,                                /* init_data */
	curves_free_data,                                /* free_data */
	curves_copy_data,                                /* copy_data */
	curves_apply                                     /* apply */
};

/* **** Hue Correct Modifier **** */

static void hue_correct_init_data(SequenceModifierData *smd)
{
	HueCorrectModifierData *hcmd = (HueCorrectModifierData *) smd;
	int c;

	curvemapping_set_defaults(&hcmd->curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f);
	hcmd->curve_mapping.preset = CURVE_PRESET_MID9;

	for (c = 0; c < 3; c++) {
		CurveMap *cuma = &hcmd->curve_mapping.cm[c];

		curvemap_reset(cuma, &hcmd->curve_mapping.clipr, hcmd->curve_mapping.preset, CURVEMAP_SLOPE_POSITIVE);
	}

	/* default to showing Saturation */
	hcmd->curve_mapping.cur = 1;
}

static void hue_correct_free_data(SequenceModifierData *smd)
{
	HueCorrectModifierData *hcmd = (HueCorrectModifierData *) smd;

	curvemapping_free_data(&hcmd->curve_mapping);
}

static void hue_correct_copy_data(SequenceModifierData *target, SequenceModifierData *smd)
{
	HueCorrectModifierData *hcmd = (HueCorrectModifierData *) smd;
	HueCorrectModifierData *hcmd_target = (HueCorrectModifierData *) target;

	curvemapping_copy_data(&hcmd_target->curve_mapping, &hcmd->curve_mapping);
}

static void hue_correct_apply_threaded(int width, int height, unsigned char *rect, float *rect_float,
                                unsigned char *mask_rect, float *mask_rect_float, void *data_v)
{
	CurveMapping *curve_mapping = (CurveMapping *) data_v;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int pixel_index = (y * width + x) * 4;
			float pixel[3], result[3], mask[3] = {1.0f, 1.0f, 1.0f};
			float hsv[3], f;

			if (rect_float)
				copy_v3_v3(pixel, rect_float + pixel_index);
			else
				rgb_uchar_to_float(pixel, rect + pixel_index);

			rgb_to_hsv(pixel[0], pixel[1], pixel[2], hsv, hsv + 1, hsv + 2);

			/* adjust hue, scaling returned default 0.5 up to 1 */
			f = curvemapping_evaluateF(curve_mapping, 0, hsv[0]);
			hsv[0] += f - 0.5f;

			/* adjust saturation, scaling returned default 0.5 up to 1 */
			f = curvemapping_evaluateF(curve_mapping, 1, hsv[0]);
			hsv[1] *= (f * 2.0f);

			/* adjust value, scaling returned default 0.5 up to 1 */
			f = curvemapping_evaluateF(curve_mapping, 2, hsv[0]);
			hsv[2] *= (f * 2.f);

			hsv[0] = hsv[0] - floorf(hsv[0]); /* mod 1.0 */
			CLAMP(hsv[1], 0.0f, 1.0f);

			/* convert back to rgb */
			hsv_to_rgb(hsv[0], hsv[1], hsv[2], result, result + 1, result + 2);

			if (mask_rect_float)
				copy_v3_v3(mask, mask_rect_float + pixel_index);
			else if (mask_rect)
				rgb_uchar_to_float(mask, mask_rect + pixel_index);

			result[0] = pixel[0] * (1.0f - mask[0]) + result[0] * mask[0];
			result[1] = pixel[1] * (1.0f - mask[1]) + result[1] * mask[1];
			result[2] = pixel[2] * (1.0f - mask[2]) + result[2] * mask[2];

			if (rect_float)
				copy_v3_v3(rect_float + pixel_index, result);
			else
				rgb_float_to_uchar(rect + pixel_index, result);
		}
	}
}

static void hue_correct_apply(struct SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
	HueCorrectModifierData *hcmd = (HueCorrectModifierData *) smd;

	curvemapping_initialize(&hcmd->curve_mapping);

	modifier_apply_threaded(ibuf, mask, hue_correct_apply_threaded, &hcmd->curve_mapping);
}

static SequenceModifierTypeInfo seqModifier_HueCorrect = {
	CTX_N_(BLF_I18NCONTEXT_ID_SEQUENCE, "Hue Correct"),    /* name */
	"HueCorrectModifierData",                              /* struct_name */
	sizeof(HueCorrectModifierData),                        /* struct_size */
	hue_correct_init_data,                                 /* init_data */
	hue_correct_free_data,                                 /* free_data */
	hue_correct_copy_data,                                 /* copy_data */
	hue_correct_apply                                      /* apply */
};

/* **** Bright/Contrast Modifier **** */

typedef struct BrightContrastThreadData {
	float bright;
	float contrast;
} BrightContrastThreadData;

static void brightcontrast_apply_threaded(int width, int height, unsigned char *rect, float *rect_float,
                                          unsigned char *mask_rect, float *mask_rect_float, void *data_v)
{
	BrightContrastThreadData *data = (BrightContrastThreadData *) data_v;
	int x, y;

	float i;
	int c;
	float a, b, v;
	float brightness = data->bright / 100.0f;
	float contrast = data->contrast;
	float delta = contrast / 200.0f;

	a = 1.0f - delta * 2.0f;
	/*
	 * The algorithm is by Werner D. Streidt
	 * (http://visca.com/ffactory/archives/5-99/msg00021.html)
	 * Extracted of OpenCV demhist.c
	 */
	if (contrast > 0) {
		a = 1.0f / a;
		b = a * (brightness - delta);
	}
	else {
		delta *= -1;
		b = a * (brightness + delta);
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int pixel_index = (y * width + x) * 4;

			if (rect) {
				unsigned char *pixel = rect + pixel_index;

				for (c = 0; c < 3; c++) {
					i = (float) pixel[c] / 255.0f;
					v = a * i + b;

					if (mask_rect) {
						unsigned char *m = mask_rect + pixel_index;
						float t = (float) m[c] / 255.0f;

						v = (float) pixel[c] / 255.0f * (1.0f - t) + v * t;
					}

					pixel[c] = FTOCHAR(v);
				}
			}
			else if (rect_float) {
				float *pixel = rect_float + pixel_index;

				for (c = 0; c < 3; c++) {
					i = pixel[c];
					v = a * i + b;

					if (mask_rect_float) {
						float *m = mask_rect_float + pixel_index;

						pixel[c] = pixel[c] * (1.0f - m[c]) + v * m[c];
					}
					else
						pixel[c] = v;
				}
			}
		}
	}
}

static void brightcontrast_apply(struct SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
	BrightContrastModifierData *bcmd = (BrightContrastModifierData *) smd;
	BrightContrastThreadData data;

	data.bright = bcmd->bright;
	data.contrast = bcmd->contrast;

	modifier_apply_threaded(ibuf, mask, brightcontrast_apply_threaded, &data);
}

static SequenceModifierTypeInfo seqModifier_BrightContrast = {
	CTX_N_(BLF_I18NCONTEXT_ID_SEQUENCE, "Bright/Contrast"),   /* name */
	"BrightContrastModifierData",                             /* struct_name */
	sizeof(BrightContrastModifierData),                       /* struct_size */
	NULL,                                                     /* init_data */
	NULL,                                                     /* free_data */
	NULL,                                                     /* copy_data */
	brightcontrast_apply                                      /* apply */
};

/* **** Mask Modifier **** */

static void maskmodifier_apply_threaded(int width, int height, unsigned char *rect, float *rect_float,
                                        unsigned char *mask_rect, float *mask_rect_float, void *UNUSED(data_v))
{
	int x, y;

	if (rect && !mask_rect)
		return;

	if (rect_float && !mask_rect_float)
		return;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int pixel_index = (y * width + x) * 4;

			if (rect) {
				unsigned char *pixel = rect + pixel_index;
				unsigned char *mask_pixel = mask_rect + pixel_index;
				unsigned char mask = min_iii(mask_pixel[0], mask_pixel[1], mask_pixel[2]);

				/* byte buffer is straight, so only affect on alpha itself,
				 * this is the only way to alpha-over byte strip after
				 * applying mask modifier.
				 */
				pixel[3] = (float)(pixel[3] * mask) / 255.0f;
			}
			else if (rect_float) {
				int c;
				float *pixel = rect_float + pixel_index;
				float *mask_pixel = mask_rect_float + pixel_index;
				float mask = min_fff(mask_pixel[0], mask_pixel[1], mask_pixel[2]);

				/* float buffers are premultiplied, so need to premul color
				 * as well to make it easy to alpha-over masted strip.
				 */
				for (c = 0; c < 4; c++)
					pixel[c] = pixel[c] * mask;
			}
		}
	}
}

static void maskmodifier_apply(struct SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
	BrightContrastModifierData *bcmd = (BrightContrastModifierData *) smd;
	BrightContrastThreadData data;

	data.bright = bcmd->bright;
	data.contrast = bcmd->contrast;

	modifier_apply_threaded(ibuf, mask, maskmodifier_apply_threaded, &data);
}

static SequenceModifierTypeInfo seqModifier_Mask = {
	CTX_N_(BLF_I18NCONTEXT_ID_SEQUENCE, "Mask"), /* name */
	"SequencerMaskModifierData",                 /* struct_name */
	sizeof(SequencerMaskModifierData),           /* struct_size */
	NULL,                                        /* init_data */
	NULL,                                        /* free_data */
	NULL,                                        /* copy_data */
	maskmodifier_apply                           /* apply */
};

/*********************** Modifier functions *************************/

static void sequence_modifier_type_info_init(void)
{
#define INIT_TYPE(typeName) (modifiersTypes[seqModifierType_##typeName] = &seqModifier_##typeName)

	INIT_TYPE(ColorBalance);
	INIT_TYPE(Curves);
	INIT_TYPE(HueCorrect);
	INIT_TYPE(BrightContrast);
	INIT_TYPE(Mask);

#undef INIT_TYPE
}

SequenceModifierTypeInfo *BKE_sequence_modifier_type_info_get(int type)
{
	if (!modifierTypesInit) {
		sequence_modifier_type_info_init();
		modifierTypesInit = TRUE;
	}

	return modifiersTypes[type];
}

SequenceModifierData *BKE_sequence_modifier_new(Sequence *seq, const char *name, int type)
{
	SequenceModifierData *smd;
	SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(type);

	smd = MEM_callocN(smti->struct_size, "sequence modifier");

	smd->type = type;
	smd->flag |= SEQUENCE_MODIFIER_EXPANDED;

	if (!name || !name[0])
		BLI_strncpy(smd->name, smti->name, sizeof(smd->name));
	else
		BLI_strncpy(smd->name, name, sizeof(smd->name));

	BLI_addtail(&seq->modifiers, smd);

	BKE_sequence_modifier_unique_name(seq, smd);

	if (smti->init_data)
		smti->init_data(smd);

	return smd;
}

int BKE_sequence_modifier_remove(Sequence *seq, SequenceModifierData *smd)
{
	if (BLI_findindex(&seq->modifiers, smd) == -1)
		return FALSE;

	BLI_remlink(&seq->modifiers, smd);
	BKE_sequence_modifier_free(smd);

	return TRUE;
}

void BKE_sequence_modifier_clear(Sequence *seq)
{
	SequenceModifierData *smd, *smd_next;

	for (smd = seq->modifiers.first; smd; smd = smd_next) {
		smd_next = smd->next;
		BKE_sequence_modifier_free(smd);
	}

	BLI_listbase_clear(&seq->modifiers);
}

void BKE_sequence_modifier_free(SequenceModifierData *smd)
{
	SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

	if (smti && smti->free_data) {
		smti->free_data(smd);
	}

	MEM_freeN(smd);
}

void BKE_sequence_modifier_unique_name(Sequence *seq, SequenceModifierData *smd)
{
	SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

	BLI_uniquename(&seq->modifiers, smd, CTX_DATA_(BLF_I18NCONTEXT_ID_SEQUENCE, smti->name), '.',
	               offsetof(SequenceModifierData, name), sizeof(smd->name));
}

SequenceModifierData *BKE_sequence_modifier_find_by_name(Sequence *seq, const char *name)
{
	return BLI_findstring(&(seq->modifiers), name, offsetof(SequenceModifierData, name));
}

ImBuf *BKE_sequence_modifier_apply_stack(const SeqRenderData *context, Sequence *seq, ImBuf *ibuf, int cfra)
{
	SequenceModifierData *smd;
	ImBuf *processed_ibuf = ibuf;

	if (seq->modifiers.first && (seq->flag & SEQ_USE_LINEAR_MODIFIERS)) {
		processed_ibuf = IMB_dupImBuf(ibuf);
		BKE_sequencer_imbuf_from_sequencer_space(context->scene, processed_ibuf);
	}

	for (smd = seq->modifiers.first; smd; smd = smd->next) {
		SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

		/* could happen if modifier is being removed or not exists in current version of blender */
		if (!smti)
			continue;

		/* modifier is muted, do nothing */
		if (smd->flag & SEQUENCE_MODIFIER_MUTE)
			continue;

		if (smti->apply) {
			ImBuf *mask = modifier_mask_get(smd, context, cfra, ibuf->rect_float != NULL);

			if (processed_ibuf == ibuf)
				processed_ibuf = IMB_dupImBuf(ibuf);

			smti->apply(smd, processed_ibuf, mask);

			if (mask)
				IMB_freeImBuf(mask);
		}
	}

	if (seq->modifiers.first && (seq->flag & SEQ_USE_LINEAR_MODIFIERS)) {
		BKE_sequencer_imbuf_to_sequencer_space(context->scene, processed_ibuf, FALSE);
	}

	return processed_ibuf;
}

void BKE_sequence_modifier_list_copy(Sequence *seqn, Sequence *seq)
{
	SequenceModifierData *smd;

	for (smd = seq->modifiers.first; smd; smd = smd->next) {
		SequenceModifierData *smdn;
		SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

		smdn = MEM_dupallocN(smd);

		if (smti && smti->copy_data)
			smti->copy_data(smdn, smd);

		smdn->next = smdn->prev = NULL;
		BLI_addtail(&seqn->modifiers, smdn);
	}
}

int BKE_sequence_supports_modifiers(Sequence *seq)
{
	return !ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD);
}
