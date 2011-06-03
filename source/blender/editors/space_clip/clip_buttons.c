/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_buttons.c
 *  \ingroup spclip
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "ED_clip.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#define B_MARKER_POS			3
#define B_MARKER_PAT_POS		4
#define B_MARKER_PAT_DIM		5
#define B_MARKER_SEARCH_POS		6
#define B_MARKER_SEARCH_DIM		7

static void to_pixel_space(float r[2], float a[2], int width, int height)
{
	copy_v2_v2(r, a);
	r[0]*= width;
	r[1]*= height;
}

static void trackingMarker_buttons(const bContext *C, uiBlock *block)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	int width, height, step, digits, type;
	MovieTrackingMarker *marker;
	float pat_dim[2], pat_pos[2], search_dim[2], search_pos[2];

	ED_space_clip_size(sc, &width, &height);
	BKE_movieclip_last_selection(clip, &type, (void**)&marker);

	step= 100;
	digits= 2;

	sub_v2_v2v2(pat_dim, marker->pat_max, marker->pat_min);
	sub_v2_v2v2(search_dim, marker->search_max, marker->search_min);

	add_v2_v2v2(search_pos, marker->search_max, marker->search_min);
	mul_v2_fl(search_pos, 0.5);

	add_v2_v2v2(pat_pos, marker->pat_max, marker->pat_min);
	mul_v2_fl(pat_pos, 0.5);

	to_pixel_space(sc->marker_pos, marker->pos, width, height);
	to_pixel_space(sc->marker_pat, pat_dim, width, height);
	to_pixel_space(sc->marker_pat_pos, pat_pos, width, height);
	to_pixel_space(sc->marker_search, search_dim, width, height);
	to_pixel_space(sc->marker_search_pos, search_pos, width, height);

	uiBlockBeginAlign(block);

	uiDefBut(block, LABEL, 0, "Position:", 0, 171, 145, 19, NULL, 0, 0, 0, 0, "");
	uiDefButF(block, NUM, B_MARKER_POS, "X:", 10, 152, 145, 19, &sc->marker_pos[0],
		-10*width, 10.0*width, step, digits, "X-position of marker at frame in screen coordinates.");
	uiDefButF(block, NUM, B_MARKER_POS, "Y:", 165, 152, 145, 19, &sc->marker_pos[1],
		-10*height, 10.0*height, step, digits, "Y-position of marker at frame in screen coordinates.");

	uiDefBut(block, LABEL, 0, "Pattern Area:", 0, 133, 145, 19, NULL, 0, 0, 0, 0, "");
	uiDefButF(block, NUM, B_MARKER_PAT_POS, "X:", 10, 114, 145, 19, &sc->marker_pat_pos[0],
		-width, width, step, digits, "X-position of pattern at frame in screen coordinates relative to marker's position.");
	uiDefButF(block, NUM, B_MARKER_PAT_POS, "Y:", 165, 114, 145, 19, &sc->marker_pat_pos[1],
		-height, height, step, digits, "Y-position of pattern at frame in screen coordinates relative to marker's position.");
	uiDefButF(block, NUM, B_MARKER_PAT_DIM, "Width:", 10, 95, 300, 19, &sc->marker_pat[0], 3.0f,
		10.0*width, step, digits, "Width of marker's pattern in screen soordinates.");
	uiDefButF(block, NUM, B_MARKER_PAT_DIM, "Height:", 10, 76, 300, 19, &sc->marker_pat[1], 3.0f,
		10.0*height, step, digits, "Height of marker's pattern in screen soordinates.");

	uiDefBut(block, LABEL, 0, "Search Area:", 0, 57, 145, 19, NULL, 0, 0, 0, 0, "");
	uiDefButF(block, NUM, B_MARKER_SEARCH_POS, "X:", 10, 38, 145, 19, &sc->marker_search_pos[0],
		-width, width, step, digits, "X-position of search at frame relative to marker's position");
	uiDefButF(block, NUM, B_MARKER_SEARCH_POS, "Y:", 165, 38, 145, 19, &sc->marker_search_pos[1],
		-height, height, step, digits, "X-position of search at frame relative to marker's position");
	uiDefButF(block, NUM, B_MARKER_SEARCH_DIM, "Width:", 10, 19, 300, 19, &sc->marker_search[0], 3.0f,
		10.0*width, step, digits, "Width of marker's search in screen soordinates.");
	uiDefButF(block, NUM, B_MARKER_SEARCH_DIM, "Height:", 10, 0, 300, 19, &sc->marker_search[1], 3.0f,
		10.0*height, step, digits, "Height of marker's search in screen soordinates.");
	uiBlockEndAlign(block);
}

static void do_tracking_marker(bContext *C, void *UNUSED(arg), int event)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingMarker *marker;
	int width, height, type, ok= 0;

	ED_space_clip_size(sc, &width, &height);
	BKE_movieclip_last_selection(clip, &type, (void**)&marker);

	if(event==B_MARKER_POS) {
		marker->pos[0]= sc->marker_pos[0]/width;
		marker->pos[1]= sc->marker_pos[1]/height;

		ok= 1;
	}
	else if(event==B_MARKER_PAT_POS) {
		float delta[2], side[2];

		sub_v2_v2v2(side, marker->pat_max, marker->pat_min);
		mul_v2_fl(side, 0.5f);

		delta[0]= sc->marker_pat_pos[0]/width;
		delta[1]= sc->marker_pat_pos[1]/height;

		sub_v2_v2v2(marker->pat_min, delta, side);
		add_v2_v2v2(marker->pat_max, delta, side);

		BKE_tracking_clamp_marker(marker, CLAMP_PAT_POS);

		ok= 1;
	}
	else if(event==B_MARKER_PAT_DIM) {
		float dim[2];

		dim[0]= sc->marker_pat[0]/width;
		dim[1]= sc->marker_pat[1]/height;

		mul_v2_v2fl(marker->pat_min, dim, -0.5);
		mul_v2_v2fl(marker->pat_max, dim, 0.5);

		BKE_tracking_clamp_marker(marker, CLAMP_PAT_DIM);

		ok= 1;
	}
	else if(event==B_MARKER_SEARCH_POS) {
		float delta[2], side[2];

		sub_v2_v2v2(side, marker->search_max, marker->search_min);
		mul_v2_fl(side, 0.5f);

		delta[0]= sc->marker_search_pos[0]/width;
		delta[1]= sc->marker_search_pos[1]/height;

		sub_v2_v2v2(marker->search_min, delta, side);
		add_v2_v2v2(marker->search_max, delta, side);

		BKE_tracking_clamp_marker(marker, CLAMP_SEARCH_POS);

		ok= 1;
	}
	else if(event==B_MARKER_SEARCH_DIM) {
		float dim[2];

		dim[0]= sc->marker_search[0]/width;
		dim[1]= sc->marker_search[1]/height;

		mul_v2_v2fl(marker->search_min, dim, -0.5);
		mul_v2_v2fl(marker->search_max, dim, 0.5);

		BKE_tracking_clamp_marker(marker, CLAMP_SEARCH_DIM);

		ok= 1;
	}

	if(ok)
		WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);
}

/* Panels */

static int clip_panel_marker_poll(const bContext *C, PanelType *UNUSED(pt))
{
	Scene *scene= CTX_data_scene(C);
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip;
	int type;
	MovieTrackingMarker *marker;

	if(scene->toolsettings->movieclip.tool != MCLIP_TOOL_MARKER || !sc)
		return 0;

	if(sc->mode != SC_MODE_TRACKING)
		return 0;

	clip= ED_space_clip(sc);

	if(!clip || !BKE_movieclip_has_frame(clip, &sc->user))
		return 0;

	BKE_movieclip_last_selection(clip, &type, (void**)&marker);

	return type==MCLIP_SEL_MARKER;
}

static void clip_panel_marker(const bContext *C, Panel *pa)
{
	uiBlock *block;

	block= uiLayoutAbsoluteBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_tracking_marker, NULL);

	trackingMarker_buttons(C, block);
}

void ED_clip_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype clip panel marker");
	strcpy(pt->idname, "CLIP_PT_active_marker");
	strcpy(pt->label, "Active Marker");
	pt->draw= clip_panel_marker;
	pt->poll= clip_panel_marker_poll;

	BLI_addtail(&art->paneltypes, pt);
}

