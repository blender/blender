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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_action/action_draw.c
 *  \ingroup spaction
 */


/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

/* Types --------------------------------------------------------------- */

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_pointcache.h"


/* Everything from source (BIF, BDR, BSE) ------------------------------ */

#include "BIF_gl.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "action_intern.h"

/* ************************************************************************* */
/* Channel List */

/* left hand part */
void draw_channel_names(bContext *C, bAnimContext *ac, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	View2D *v2d = &ar->v2d;
	float y = 0.0f;
	size_t items;
	int height;

	/* build list of channels to draw */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

	height = ((items * ACHANNEL_STEP(ac)) + (ACHANNEL_HEIGHT(ac)));
	if (height > BLI_rcti_size_y(&v2d->mask)) {
		/* don't use totrect set, as the width stays the same
		 * (NOTE: this is ok here, the configuration is pretty straightforward)
		 */
		v2d->tot.ymin = (float)(-height);
	}
	/* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
	UI_view2d_sync(NULL, ac->sa, v2d, V2D_LOCK_COPY);

	/* loop through channels, and set up drawing depending on their type  */
	{   /* first pass: just the standard GL-drawing for backdrop + text */
		size_t channel_index = 0;

		y = (float)ACHANNEL_FIRST(ac);

		for (ale = anim_data.first; ale; ale = ale->next) {
			float yminc = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
			float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF(ac));

			/* check if visible */
			if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw(ac, ale, yminc, ymaxc, channel_index);
			}

			/* adjust y-position for next one */
			y -= ACHANNEL_STEP(ac);
			channel_index++;
		}
	}
	{   /* second pass: widgets */
		uiBlock *block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
		size_t channel_index = 0;

		y = (float)ACHANNEL_FIRST(ac);

		for (ale = anim_data.first; ale; ale = ale->next) {
			float yminc = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
			float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF(ac));

			/* check if visible */
			if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw_widgets(C, ac, ale, block, yminc, ymaxc, channel_index);
			}

			/* adjust y-position for next one */
			y -= ACHANNEL_STEP(ac);
			channel_index++;
		}

		UI_block_end(C, block);
		UI_block_draw(C, block);
	}

	/* free tempolary channels */
	ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************* */
/* Keyframes */

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD    100.0f

/* draw keyframes in each channel */
void draw_channel_strips(bAnimContext *ac, SpaceAction *saction, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;

	View2D *v2d = &ar->v2d;
	bDopeSheet *ads = &saction->ads;
	AnimData *adt = NULL;

	float act_start, act_end, y;

	unsigned char col1[3], col2[3];
	unsigned char col1a[3], col2a[3];
	unsigned char col1b[3], col2b[3];

	const bool show_group_colors = !(saction->flag & SACTION_NODRAWGCOLORS);


	/* get theme colors */
	UI_GetThemeColor3ubv(TH_BACK, col2);
	UI_GetThemeColor3ubv(TH_HILITE, col1);

	UI_GetThemeColor3ubv(TH_GROUP, col2a);
	UI_GetThemeColor3ubv(TH_GROUP_ACTIVE, col1a);

	UI_GetThemeColor3ubv(TH_DOPESHEET_CHANNELOB, col1b);
	UI_GetThemeColor3ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);

	/* set view-mapping rect (only used for x-axis), for NLA-scaling mapping with less calculation */

	/* if in NLA there's a strip active, map the view */
	if (ac->datatype == ANIMCONT_ACTION) {
		/* adt = ANIM_nla_mapping_get(ac, NULL); */ /* UNUSED */

		/* start and end of action itself */
		calc_action_range(ac->data, &act_start, &act_end, 0);
	}

	/* build list of channels to draw */
	int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	size_t items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

	int height = ((items * ACHANNEL_STEP(ac)) + (ACHANNEL_HEIGHT(ac)));
	/* don't use totrect set, as the width stays the same
	 * (NOTE: this is ok here, the configuration is pretty straightforward)
	 */
	v2d->tot.ymin = (float)(-height);

	/* first backdrop strips */
	y = (float)(-ACHANNEL_HEIGHT(ac));

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	GPU_blend(true);

	for (ale = anim_data.first; ale; ale = ale->next) {
		const float yminc = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
		const float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF(ac));

		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
		{
			const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
			int sel = 0;

			/* determine if any need to draw channel */
			if (ale->datatype != ALE_NONE) {
				/* determine if channel is selected */
				if (acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT))
					sel = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT);

				if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY)) {
					switch (ale->type) {
						case ANIMTYPE_SUMMARY:
						{
							/* reddish color from NLA */
							immUniformThemeColor(TH_ANIM_ACTIVE);
							break;
						}
						case ANIMTYPE_SCENE:
						case ANIMTYPE_OBJECT:
						{
							immUniformColor3ubvAlpha(col1b, sel ? 0x45 : 0x22);
							break;
						}
						case ANIMTYPE_FILLACTD:
						case ANIMTYPE_DSSKEY:
						case ANIMTYPE_DSWOR:
						{
							immUniformColor3ubvAlpha(col2b, sel ? 0x45 : 0x22);
							break;
						}
						case ANIMTYPE_GROUP:
						{
							bActionGroup *agrp = ale->data;
							if (show_group_colors && agrp->customCol) {
								if (sel) {
									immUniformColor3ubvAlpha((unsigned char *)agrp->cs.select, 0x45);
								}
								else {
									immUniformColor3ubvAlpha((unsigned char *)agrp->cs.solid, 0x1D);
								}
							}
							else {
								immUniformColor3ubvAlpha(sel ? col1a : col2a, 0x22);
							}
							break;
						}
						case ANIMTYPE_FCURVE:
						{
							FCurve *fcu = ale->data;
							if (show_group_colors && fcu->grp && fcu->grp->customCol) {
								immUniformColor3ubvAlpha((unsigned char *)fcu->grp->cs.active, sel ? 0x65 : 0x0B);
							}
							else {
								immUniformColor3ubvAlpha(sel ? col1 : col2, 0x22);
							}
							break;
						}
						default:
						{
							immUniformColor3ubvAlpha(sel ? col1 : col2, 0x22);
						}
					}

					/* draw region twice: firstly backdrop, then the current range */
					immRectf(pos, v2d->cur.xmin,  (float)y - ACHANNEL_HEIGHT_HALF(ac),  v2d->cur.xmax + EXTRA_SCROLL_PAD,  (float)y + ACHANNEL_HEIGHT_HALF(ac));

					if (ac->datatype == ANIMCONT_ACTION)
						immRectf(pos, act_start,  (float)y - ACHANNEL_HEIGHT_HALF(ac),  act_end,  (float)y + ACHANNEL_HEIGHT_HALF(ac));
				}
				else if (ac->datatype == ANIMCONT_GPENCIL) {
					/* frames less than one get less saturated background */
					immUniformColor3ubvAlpha(sel ? col1 : col2, 0x22);
					immRectf(pos, 0.0f, (float)y - ACHANNEL_HEIGHT_HALF(ac), v2d->cur.xmin, (float)y + ACHANNEL_HEIGHT_HALF(ac));

					/* frames one and higher get a saturated background */
					immUniformColor3ubvAlpha(sel ? col1 : col2, 0x44);
					immRectf(pos, v2d->cur.xmin, (float)y - ACHANNEL_HEIGHT_HALF(ac), v2d->cur.xmax + EXTRA_SCROLL_PAD,  (float)y + ACHANNEL_HEIGHT_HALF(ac));
				}
				else if (ac->datatype == ANIMCONT_MASK) {
					/* TODO --- this is a copy of gpencil */
					/* frames less than one get less saturated background */
					immUniformColor3ubvAlpha(sel ? col1 : col2, 0x22);
					immRectf(pos, 0.0f, (float)y - ACHANNEL_HEIGHT_HALF(ac), v2d->cur.xmin, (float)y + ACHANNEL_HEIGHT_HALF(ac));

					/* frames one and higher get a saturated background */
					immUniformColor3ubvAlpha(sel ? col1 : col2, 0x44);
					immRectf(pos, v2d->cur.xmin, (float)y - ACHANNEL_HEIGHT_HALF(ac), v2d->cur.xmax + EXTRA_SCROLL_PAD,  (float)y + ACHANNEL_HEIGHT_HALF(ac));
				}
			}
		}

		/*	Increment the step */
		y -= ACHANNEL_STEP(ac);
	}
	GPU_blend(false);

	/* black line marking 'current frame' for Time-Slide transform mode */
	if (saction->flag & SACTION_MOVING) {
		immUniformColor3f(0.0f, 0.0f, 0.0f);

		immBegin(GPU_PRIM_LINES, 2);
		immVertex2f(pos, saction->timeslide, v2d->cur.ymin - EXTRA_SCROLL_PAD);
		immVertex2f(pos, saction->timeslide, v2d->cur.ymax);
		immEnd();
	}
	immUnbindProgram();

	/* Draw keyframes
	 *	1) Only channels that are visible in the Action Editor get drawn/evaluated.
	 *	   This is to try to optimize this for heavier data sets
	 *	2) Keyframes which are out of view horizontally are disregarded
	 */
	y = (float)(-ACHANNEL_HEIGHT(ac));

	for (ale = anim_data.first; ale; ale = ale->next) {
		const float yminc = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
		const float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF(ac));

		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
		{
			/* check if anything to show for this channel */
			if (ale->datatype != ALE_NONE) {
				adt = ANIM_nla_mapping_get(ac, ale);

				/* draw 'keyframes' for each specific datatype */
				switch (ale->datatype) {
					case ALE_ALL:
						draw_summary_channel(v2d, ale->data, y, ac->yscale_fac);
						break;
					case ALE_SCE:
						draw_scene_channel(v2d, ads, ale->key_data, y, ac->yscale_fac);
						break;
					case ALE_OB:
						draw_object_channel(v2d, ads, ale->key_data, y, ac->yscale_fac);
						break;
					case ALE_ACT:
						draw_action_channel(v2d, adt, ale->key_data, y, ac->yscale_fac);
						break;
					case ALE_GROUP:
						draw_agroup_channel(v2d, adt, ale->data, y, ac->yscale_fac);
						break;
					case ALE_FCURVE:
						draw_fcurve_channel(v2d, adt, ale->key_data, y, ac->yscale_fac);
						break;
					case ALE_GPFRAME:
						draw_gpl_channel(v2d, ads, ale->data, y, ac->yscale_fac);
						break;
					case ALE_MASKLAY:
						draw_masklay_channel(v2d, ads, ale->data, y, ac->yscale_fac);
						break;
				}
			}
		}

		y -= ACHANNEL_STEP(ac);
	}

	/* free temporary channels used for drawing */
	ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************* */
/* Timeline - Caches */

void timeline_draw_cache(SpaceAction *saction, Object *ob, Scene *scene)
{
	PTCacheID *pid;
	ListBase pidlist;
	const float cache_draw_height = (4.0f * UI_DPI_FAC * U.pixelsize);
	float yoffs = 0.f;

	if (!(saction->cache_display & TIME_CACHE_DISPLAY) || (!ob))
		return;

	BKE_ptcache_ids_from_object(&pidlist, ob, scene, 0);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	/* iterate over pointcaches on the active object, and draw each one's range */
	for (pid = pidlist.first; pid; pid = pid->next) {
		float col[4];

		switch (pid->type) {
			case PTCACHE_TYPE_SOFTBODY:
				if (!(saction->cache_display & TIME_CACHE_SOFTBODY)) continue;
				break;
			case PTCACHE_TYPE_PARTICLES:
				if (!(saction->cache_display & TIME_CACHE_PARTICLES)) continue;
				break;
			case PTCACHE_TYPE_CLOTH:
				if (!(saction->cache_display & TIME_CACHE_CLOTH)) continue;
				break;
			case PTCACHE_TYPE_SMOKE_DOMAIN:
			case PTCACHE_TYPE_SMOKE_HIGHRES:
				if (!(saction->cache_display & TIME_CACHE_SMOKE)) continue;
				break;
			case PTCACHE_TYPE_DYNAMICPAINT:
				if (!(saction->cache_display & TIME_CACHE_DYNAMICPAINT)) continue;
				break;
			case PTCACHE_TYPE_RIGIDBODY:
				if (!(saction->cache_display & TIME_CACHE_RIGIDBODY)) continue;
				break;
		}

		if (pid->cache->cached_frames == NULL)
			continue;

		GPU_matrix_push();
		GPU_matrix_translate_2f(0.0, (float)V2D_SCROLL_HEIGHT_TEXT + yoffs);
		GPU_matrix_scale_2f(1.0, cache_draw_height);

		switch (pid->type) {
			case PTCACHE_TYPE_SOFTBODY:
				col[0] = 1.0;   col[1] = 0.4;   col[2] = 0.02;
				col[3] = 0.1;
				break;
			case PTCACHE_TYPE_PARTICLES:
				col[0] = 1.0;   col[1] = 0.1;   col[2] = 0.02;
				col[3] = 0.1;
				break;
			case PTCACHE_TYPE_CLOTH:
				col[0] = 0.1;   col[1] = 0.1;   col[2] = 0.75;
				col[3] = 0.1;
				break;
			case PTCACHE_TYPE_SMOKE_DOMAIN:
			case PTCACHE_TYPE_SMOKE_HIGHRES:
				col[0] = 0.2;   col[1] = 0.2;   col[2] = 0.2;
				col[3] = 0.1;
				break;
			case PTCACHE_TYPE_DYNAMICPAINT:
				col[0] = 1.0;   col[1] = 0.1;   col[2] = 0.75;
				col[3] = 0.1;
				break;
			case PTCACHE_TYPE_RIGIDBODY:
				col[0] = 1.0;   col[1] = 0.6;   col[2] = 0.0;
				col[3] = 0.1;
				break;
			default:
				col[0] = 1.0;   col[1] = 0.0;   col[2] = 1.0;
				col[3] = 0.1;
				BLI_assert(0);
				break;
		}

		const int sta = pid->cache->startframe, end = pid->cache->endframe;
		const int len = (end - sta + 1) * 6;

		GPU_blend(true);

		immUniformColor4fv(col);
		immRectf(pos, (float)sta, 0.0, (float)end, 1.0);

		col[3] = 0.4f;
		if (pid->cache->flag & PTCACHE_BAKED) {
			col[0] -= 0.4f; col[1] -= 0.4f; col[2] -= 0.4f;
		}
		else if (pid->cache->flag & PTCACHE_OUTDATED) {
			col[0] += 0.4f; col[1] += 0.4f; col[2] += 0.4f;
		}

		immUniformColor4fv(col);

		if (len > 0) {
			immBeginAtMost(GPU_PRIM_TRIS, len);

			/* draw a quad for each cached frame */
			for (int i = sta; i <= end; i++) {
				if (pid->cache->cached_frames[i - sta]) {
					immVertex2f(pos, (float)i - 0.5f, 0.0f);
					immVertex2f(pos, (float)i - 0.5f, 1.0f);
					immVertex2f(pos, (float)i + 0.5f, 1.0f);

					immVertex2f(pos, (float)i - 0.5f, 0.0f);
					immVertex2f(pos, (float)i + 0.5f, 1.0f);
					immVertex2f(pos, (float)i + 0.5f, 0.0f);
				}
			}

			immEnd();
		}

		GPU_blend(false);

		GPU_matrix_pop();

		yoffs += cache_draw_height;
	}

	immUnbindProgram();

	BLI_freelistN(&pidlist);
}

/* ************************************************************************* */
