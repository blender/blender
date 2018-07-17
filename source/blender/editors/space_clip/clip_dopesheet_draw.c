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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_dopesheet_draw.c
 *  \ingroup spclip
 */

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLF_api.h"

#include "RNA_access.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_state.h"

#include "clip_intern.h"  /* own include */

static void track_channel_color(MovieTrackingTrack *track, float default_color[3], float color[3])
{
	if (track->flag & TRACK_CUSTOMCOLOR) {
		float bg[3];
		UI_GetThemeColor3fv(TH_HEADER, bg);

		interp_v3_v3v3(color, track->color, bg, 0.5);
	}
	else {
		if (default_color)
			copy_v3_v3(color, default_color);
		else
			UI_GetThemeColor3fv(TH_HEADER, color);
	}
}

static void draw_keyframe_shape(float x, float y, bool sel, float alpha,
                                unsigned int pos_id, unsigned int color_id)
{
	float color[4] = { 0.91f, 0.91f, 0.91f, alpha };
	if (sel) {
		UI_GetThemeColorShadeAlpha4fv(TH_STRIP_SELECT, 50, -255 * (1.0f - alpha), color);
	}

	immAttrib4fv(color_id, color);
	immVertex2f(pos_id, x, y);
}

static void clip_draw_dopesheet_background(ARegion *ar, MovieClip *clip, unsigned int pos_id)
{
	View2D *v2d = &ar->v2d;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
	MovieTrackingDopesheetCoverageSegment *coverage_segment;

	for (coverage_segment = dopesheet->coverage_segments.first;
	     coverage_segment;
	     coverage_segment = coverage_segment->next)
	{
		if (coverage_segment->coverage < TRACKING_COVERAGE_OK) {
			int start_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, coverage_segment->start_frame);
			int end_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, coverage_segment->end_frame);

			if (coverage_segment->coverage == TRACKING_COVERAGE_BAD) {
				immUniformColor4f(1.0f, 0.0f, 0.0f, 0.07f);
			}
			else {
				immUniformColor4f(1.0f, 1.0f, 0.0f, 0.07f);
			}

			immRectf(pos_id, start_frame, v2d->cur.ymin, end_frame, v2d->cur.ymax);
		}
	}
}

void clip_draw_dopesheet_main(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	View2D *v2d = &ar->v2d;

	/* frame range */
	clip_draw_sfra_efra(v2d, scene);

	if (clip) {
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
		MovieTrackingDopesheetChannel *channel;
		float strip[4], selected_strip[4];
		float height = (dopesheet->tot_channel * CHANNEL_STEP) + (CHANNEL_HEIGHT);

		uint keyframe_len = 0;

		GPUVertFormat *format = immVertexFormat();
		uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		/* don't use totrect set, as the width stays the same
		 * (NOTE: this is ok here, the configuration is pretty straightforward)
		 */
		v2d->tot.ymin = (float)(-height);

		float y = (float) CHANNEL_FIRST;

		/* setup colors for regular and selected strips */
		UI_GetThemeColor3fv(TH_STRIP, strip);
		UI_GetThemeColor3fv(TH_STRIP_SELECT, selected_strip);

		strip[3] = 0.5f;
		selected_strip[3] = 1.0f;

		GPU_blend(true);

		clip_draw_dopesheet_background(ar, clip, pos_id);

		for (channel = dopesheet->channels.first; channel; channel = channel->next) {
			float yminc = (float) (y - CHANNEL_HEIGHT_HALF);
			float ymaxc = (float) (y + CHANNEL_HEIGHT_HALF);

			/* check if visible */
			if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
			{
				MovieTrackingTrack *track = channel->track;
				int i;
				bool sel = (track->flag & TRACK_DOPE_SEL) != 0;

				/* selection background */
				if (sel) {
					float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
					float default_color[4] = {0.8f, 0.93f, 0.8f, 0.3f};

					track_channel_color(track, default_color, color);
					immUniformColor4fv(color);

					immRectf(pos_id, v2d->cur.xmin, (float) y - CHANNEL_HEIGHT_HALF,
					         v2d->cur.xmax + EXTRA_SCROLL_PAD, (float) y + CHANNEL_HEIGHT_HALF);
				}

				/* tracked segments */
				for (i = 0; i < channel->tot_segment; i++) {
					int start_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, channel->segments[2 * i]);
					int end_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, channel->segments[2 * i + 1]);

					immUniformColor4fv(sel ? selected_strip : strip);

					if (start_frame != end_frame) {
						immRectf(pos_id, start_frame, (float) y - STRIP_HEIGHT_HALF,
						         end_frame, (float) y + STRIP_HEIGHT_HALF);
						keyframe_len += 2;
					}
					else {
						keyframe_len++;
					}
				}

				/* keyframes */
				i = 0;
				while (i < track->markersnr) {
					MovieTrackingMarker *marker = &track->markers[i];

					if ((marker->flag & (MARKER_DISABLED | MARKER_TRACKED)) == 0) {
						keyframe_len++;
					}

					i++;
				}
			}

			/* adjust y-position for next one */
			y -= CHANNEL_STEP;
		}

		immUnbindProgram();

		if (keyframe_len > 0) {
			/* draw keyframe markers */
			format = immVertexFormat();
			pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			uint size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
			uint color_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
			uint outline_color_id = GPU_vertformat_attr_add(format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

			immBindBuiltinProgram(GPU_SHADER_KEYFRAME_DIAMOND);
			GPU_enable_program_point_size();
			immBegin(GPU_PRIM_POINTS, keyframe_len);

			/* all same size with black outline */
			immAttrib1f(size_id, 2.0f * STRIP_HEIGHT_HALF);
			immAttrib4ub(outline_color_id, 0, 0, 0, 255);

			y = (float) CHANNEL_FIRST; /* start again at the top */
			for (channel = dopesheet->channels.first; channel; channel = channel->next) {
				float yminc = (float) (y - CHANNEL_HEIGHT_HALF);
				float ymaxc = (float) (y + CHANNEL_HEIGHT_HALF);

				/* check if visible */
				if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
				    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
				{
					MovieTrackingTrack *track = channel->track;
					int i;
					bool sel = (track->flag & TRACK_DOPE_SEL) != 0;
					float alpha = (track->flag & TRACK_LOCKED) ? 0.5f : 1.0f;

					/* tracked segments */
					for (i = 0; i < channel->tot_segment; i++) {
						int start_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, channel->segments[2 * i]);
						int end_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, channel->segments[2 * i + 1]);

						if (start_frame != end_frame) {
							draw_keyframe_shape(start_frame, y, sel, alpha, pos_id, color_id);
							draw_keyframe_shape(end_frame, y, sel, alpha, pos_id, color_id);
						}
						else {
							draw_keyframe_shape(start_frame, y, sel, alpha, pos_id, color_id);
						}
					}

					/* keyframes */
					i = 0;
					while (i < track->markersnr) {
						MovieTrackingMarker *marker = &track->markers[i];

						if ((marker->flag & (MARKER_DISABLED | MARKER_TRACKED)) == 0) {
							int framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);

							draw_keyframe_shape(framenr, y, sel, alpha, pos_id, color_id);
						}

						i++;
					}
				}

				/* adjust y-position for next one */
				y -= CHANNEL_STEP;
			}

			immEnd();
			GPU_disable_program_point_size();
			immUnbindProgram();
		}

		GPU_blend(false);
	}
}

void clip_draw_dopesheet_channels(const bContext *C, ARegion *ar)
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	View2D *v2d = &ar->v2d;
	MovieClip *clip = ED_space_clip_get_clip(sc);
	uiStyle *style = UI_style_get();
	int fontid = style->widget.uifont_id;

	if (!clip)
		return;

	MovieTracking *tracking = &clip->tracking;
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
	int height = (dopesheet->tot_channel * CHANNEL_STEP) + (CHANNEL_HEIGHT);

	if (height > BLI_rcti_size_y(&v2d->mask)) {
		/* don't use totrect set, as the width stays the same
		 * (NOTE: this is ok here, the configuration is pretty straightforward)
		 */
		v2d->tot.ymin = (float)(-height);
	}

	/* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
	UI_view2d_sync(NULL, sa, v2d, V2D_LOCK_COPY);

	/* loop through channels, and set up drawing depending on their type
	 * first pass: just the standard GL-drawing for backdrop + text
	 */
	float y = (float) CHANNEL_FIRST;

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	MovieTrackingDopesheetChannel *channel;
	for (channel = dopesheet->channels.first; channel; channel = channel->next) {
		float yminc = (float) (y - CHANNEL_HEIGHT_HALF);
		float ymaxc = (float) (y + CHANNEL_HEIGHT_HALF);

		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
		{
			MovieTrackingTrack *track = channel->track;
			float color[3];
			track_channel_color(track, NULL, color);
			immUniformColor3fv(color);

			immRectf(pos, v2d->cur.xmin, (float) y - CHANNEL_HEIGHT_HALF,
			         v2d->cur.xmax + EXTRA_SCROLL_PAD, (float) y + CHANNEL_HEIGHT_HALF);
		}

		/* adjust y-position for next one */
		y -= CHANNEL_STEP;
	}
	immUnbindProgram();

	/* second pass: text */
	y = (float) CHANNEL_FIRST;

	BLF_size(fontid, 11.0f * U.pixelsize, U.dpi);

	for (channel = dopesheet->channels.first; channel; channel = channel->next) {
		float yminc = (float) (y - CHANNEL_HEIGHT_HALF);
		float ymaxc = (float) (y + CHANNEL_HEIGHT_HALF);

		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
		{
			MovieTrackingTrack *track = channel->track;
			bool sel = (track->flag & TRACK_DOPE_SEL) != 0;

			UI_FontThemeColor(fontid, sel ? TH_TEXT_HI : TH_TEXT);

			float font_height = BLF_height(fontid, channel->name, sizeof(channel->name));
			BLF_position(fontid, v2d->cur.xmin + CHANNEL_PAD,
			             y - font_height / 2.0f, 0.0f);
			BLF_draw(fontid, channel->name, strlen(channel->name));
		}

		/* adjust y-position for next one */
		y -= CHANNEL_STEP;
	}

	/* third pass: widgets */
	uiBlock *block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	y = (float) CHANNEL_FIRST;

	/* get RNA properties (once) */
	PropertyRNA *chan_prop_lock = RNA_struct_type_find_property(&RNA_MovieTrackingTrack, "lock");
	BLI_assert(chan_prop_lock);

	GPU_blend(true);
	for (channel = dopesheet->channels.first; channel; channel = channel->next) {
		float yminc = (float)(y - CHANNEL_HEIGHT_HALF);
		float ymaxc = (float)(y + CHANNEL_HEIGHT_HALF);

		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
		{
			MovieTrackingTrack *track = channel->track;
			const int icon = (track->flag & TRACK_LOCKED) ? ICON_LOCKED : ICON_UNLOCKED;
			PointerRNA ptr;

			RNA_pointer_create(&clip->id, &RNA_MovieTrackingTrack, track, &ptr);

			UI_block_emboss_set(block, UI_EMBOSS_NONE);
			uiDefIconButR_prop(block, UI_BTYPE_ICON_TOGGLE, 1, icon,
			                   v2d->cur.xmax - UI_UNIT_X - CHANNEL_PAD, y - UI_UNIT_Y / 2.0f,
			                   UI_UNIT_X, UI_UNIT_Y, &ptr, chan_prop_lock, 0, 0, 0, 0, 0, NULL);
			UI_block_emboss_set(block, UI_EMBOSS);
		}

		/* adjust y-position for next one */
		y -= CHANNEL_STEP;
	}
	GPU_blend(false);

	UI_block_end(C, block);
	UI_block_draw(C, block);
}
