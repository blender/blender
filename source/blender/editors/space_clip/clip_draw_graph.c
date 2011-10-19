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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_draw_graph.c
 *  \ingroup spclip
 */

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"	/* SELECT */

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "BIF_gl.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLF_api.h"

#include "clip_intern.h"	// own include

static void draw_graph_cfra(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	View2D *v2d= &ar->v2d;
	uiStyle *style= UI_GetStyle();
	int fontid= style->widget.uifont_id, fontsize;
	float xscale, yscale, x, y;
	char str[32] = "    t";	/* t is the character to start replacing from */
	short slen;
	float vec[2];

	/* Draw a light green line to indicate current frame */
	vec[0]= (float)(sc->user.framenr * scene->r.framelen);

	UI_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);

	glBegin(GL_LINE_STRIP);
		vec[1]= v2d->cur.ymin;
		glVertex2fv(vec);

		vec[1]= v2d->cur.ymax;
		glVertex2fv(vec);
	glEnd();

	glLineWidth(1.0);

	UI_view2d_view_orthoSpecial(ar, v2d, 1);

	/* because the frame number text is subject to the same scaling as the contents of the view */
	UI_view2d_getscale(v2d, &xscale, &yscale);
	glScalef(1.0f/xscale, 1.0f, 1.0f);

	BLI_snprintf(&str[4], sizeof(str)-4, "%d", sc->user.framenr);
	slen= BLF_width(fontid, str);
	fontsize= BLF_height(fontid, str);

	/* get starting coordinates for drawing */
	x= (float)sc->user.framenr * xscale;
	y= 18;

	/* draw green box around/behind text */
	UI_ThemeColorShade(TH_CFRAME, 0);
	glRectf(x, y,  x+slen,  y+fontsize+4);

	/* draw current frame number - black text */
	UI_ThemeColor(TH_TEXT);
	BLF_position(fontid, x-5, y+2, 0.f);
	BLF_draw(fontid, str, strlen(str));

	/* restore view transform */
	glScalef(xscale, 1.0, 1.0);
}

static void draw_clip_tracks_curves(SpaceClip *sc)
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingTrack *track;
	int size[2];

	static float colors[2][3] = {{1.f, 0.f, 0.f},
								{0.f, 1.f, 0.f}};


	BKE_movieclip_acquire_size(clip, &sc->user, &size[0], &size[1]);

	if(!size[0] || !size[1])
		return;

	track= tracking->tracks.first;
	while(track) {
		if(TRACK_VIEW_SELECTED(sc, track)) {
			int coord;

			for(coord= 0; coord<2; coord++) {
				int i, lines= 0, prevfra= 0;
				float prevval= 0.f;

				glColor3fv(colors[coord]);

				for(i= 0; i<track->markersnr; i++) {
					MovieTrackingMarker *marker= &track->markers[i];

					if(marker->flag&MARKER_DISABLED)
						continue;

					if(lines && marker->framenr!=prevfra+1) {
						glEnd();
						lines= 0;
					}

					if(!lines) {
						glBegin(GL_LINE_STRIP);
						lines= 1;
						prevval= marker->pos[coord];
					}

					glVertex2f(marker->framenr, (marker->pos[coord] - prevval) * size[coord]);

					prevval= marker->pos[coord];
					prevfra= marker->framenr;
				}

				if(lines)
					glEnd();
			}
		}

		track= track->next;
	}
}

static void draw_clip_frame_curves(SpaceClip *sc)
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingReconstruction *reconstruction= &tracking->reconstruction;
	int i, lines= 0, prevfra= 0;

	glColor3f(0.f, 0.f, 1.f);

	for(i= 0; i<reconstruction->camnr; i++) {
		MovieReconstructedCamera *camera= &reconstruction->cameras[i];

		if(lines && camera->framenr!=prevfra+1) {
			glEnd();
			lines= 0;
		}

		if(!lines) {
			glBegin(GL_LINE_STRIP);
			lines= 1;
		}

		glVertex2f(camera->framenr, camera->error);

		prevfra= camera->framenr;
	}

	if(lines)
		glEnd();
}

void draw_clip_graph(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	View2D *v2d= &ar->v2d;
	View2DGrid *grid;
	short unitx= V2D_UNIT_FRAMESCALE, unity= V2D_UNIT_VALUES;

	/* grid */
	grid= UI_view2d_grid_calc(scene, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP, ar->winx, ar->winy);
	UI_view2d_grid_draw(v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);

	if(sc->flag&SC_SHOW_GRAPH_TRACKS)
		draw_clip_tracks_curves(sc);

	if(sc->flag&SC_SHOW_GRAPH_FRAMES)
		draw_clip_frame_curves(sc);

	/* current frame */
	draw_graph_cfra(sc, ar, scene);
}
