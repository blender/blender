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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_graph_draw.c
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
#include "BIF_glutil.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLF_api.h"

#include "clip_intern.h"	// own include

static void draw_curve_knot(float x, float y, float xscale, float yscale, float hsize)
{
	static GLuint displist=0;

	/* initialize round circle shape */
	if (displist == 0) {
		GLUquadricObj *qobj;

		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE);

		qobj= gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_SILHOUETTE);
		gluDisk(qobj, 0,  0.7, 8, 1);
		gluDeleteQuadric(qobj);

		glEndList();
	}

	glPushMatrix();

	glTranslatef(x, y, 0.0f);
	glScalef(1.0f/xscale*hsize, 1.0f/yscale*hsize, 1.0f);
	glCallList(displist);

	glPopMatrix();
}

static void draw_graph_cfra(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	View2D *v2d= &ar->v2d;
	float xscale, yscale;
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

	clip_draw_curfra_label(sc, (float)sc->user.framenr * xscale, 18);

	/* restore view transform */
	glScalef(xscale, 1.0, 1.0);
}

static void draw_graph_sfra_efra(Scene *scene, View2D *v2d)
{
	UI_view2d_view_ortho(v2d);

	/* currently clip editor supposes that editing clip length is equal to scene frame range */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.4f);

		glRectf(v2d->cur.xmin, v2d->cur.ymin, (float)SFRA, v2d->cur.ymax);
		glRectf((float)EFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
	glDisable(GL_BLEND);

	UI_ThemeColorShade(TH_BACK, -60);

	/* thin lines where the actual frames are */
	fdrawline((float)SFRA, v2d->cur.ymin, (float)SFRA, v2d->cur.ymax);
	fdrawline((float)EFRA, v2d->cur.ymin, (float)EFRA, v2d->cur.ymax);
}

static void tracking_segment_point_cb(void *UNUSED(userdata), MovieTrackingTrack *UNUSED(track),
			MovieTrackingMarker *marker, int UNUSED(coord), float val)
{
	glVertex2f(marker->framenr, val);
}

void tracking_segment_start_cb(void *userdata, MovieTrackingTrack *track, int coord)
{
	static float colors[2][3] = {{1.0f, 0.0f, 0.0f},
	                             {0.0f, 1.0f, 0.0f}};
	float col[4];

	copy_v3_v3(col, colors[coord]);

	if(track==userdata) {
		col[3]= 1.0f;
		glLineWidth(2.0f);
	} else {
		col[3]= 0.5f;
		glLineWidth(1.0f);
	}

	glColor4fv(col);

	glBegin(GL_LINE_STRIP);
}

void tracking_segment_end_cb(void *UNUSED(userdata))
{
	glEnd();

	glLineWidth(1.0f);
}

static void tracking_segment_knot_cb(void *userdata, MovieTrackingTrack *track,
			MovieTrackingMarker *marker, int coord, float val)
{
	struct { MovieTrackingTrack *act_track; int sel; float xscale, yscale, hsize; } *data = userdata;
	int sel= 0, sel_flag;

	if(track!=data->act_track)
		return;

	sel_flag= coord == 0 ? MARKER_GRAPH_SEL_X : MARKER_GRAPH_SEL_Y;
	sel= (marker->flag & sel_flag) ? 1 : 0;

	if(sel == data->sel) {
		if(sel) UI_ThemeColor(TH_HANDLE_VERTEX_SELECT);
		else UI_ThemeColor(TH_HANDLE_VERTEX);

		draw_curve_knot(marker->framenr, val, data->xscale, data->yscale, data->hsize);
	}
}

static void draw_tracks_curves(View2D *v2d, SpaceClip *sc)
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingTrack *act_track= BKE_tracking_active_track(tracking);
	int width, height;
	struct { MovieTrackingTrack *act_track; int sel; float xscale, yscale, hsize; } userdata;

	BKE_movieclip_get_size(clip, &sc->user, &width, &height);

	if(!width || !height)
		return;

	/* non-selected knot handles */
	userdata.hsize= UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE);
	userdata.sel= 0;
	userdata.act_track= act_track;
	UI_view2d_getscale(v2d, &userdata.xscale, &userdata.yscale);
	clip_graph_tracking_values_iterate(sc, &userdata, tracking_segment_knot_cb, NULL, NULL);

	/* draw graph lines */
	glEnable(GL_BLEND);
	clip_graph_tracking_values_iterate(sc, act_track, tracking_segment_point_cb, tracking_segment_start_cb, tracking_segment_end_cb);
	glDisable(GL_BLEND);

	/* selected knot handles on top of curves */
	userdata.sel= 1;
	clip_graph_tracking_values_iterate(sc, &userdata, tracking_segment_knot_cb, NULL, NULL);
}

static void draw_frame_curves(SpaceClip *sc)
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingReconstruction *reconstruction= BKE_tracking_get_reconstruction(tracking);
	int i, lines= 0, prevfra= 0;

	glColor3f(0.0f, 0.0f, 1.0f);

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

void clip_draw_graph(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	MovieClip *clip= ED_space_clip(sc);
	View2D *v2d= &ar->v2d;
	View2DGrid *grid;
	short unitx= V2D_UNIT_FRAMESCALE, unity= V2D_UNIT_VALUES;

	/* grid */
	grid= UI_view2d_grid_calc(scene, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP, ar->winx, ar->winy);
	UI_view2d_grid_draw(v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);

	if(clip) {
		if(sc->flag&SC_SHOW_GRAPH_TRACKS)
			draw_tracks_curves(v2d, sc);

		if(sc->flag&SC_SHOW_GRAPH_FRAMES)
			draw_frame_curves(sc);
	}

	/* frame range */
	draw_graph_sfra_efra(scene, v2d);

	/* current frame */
	draw_graph_cfra(sc, ar, scene);
}
