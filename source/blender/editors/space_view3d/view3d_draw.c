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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_draw.c
 *  \ingroup spview3d
 */

#include "BIF_gl.h"

#include "ED_screen.h"

#include "BKE_context.h"

#include "view3d_intern.h"  /* own include */

/* ******************** solid plates ***************** */

/**
 *
 */
static void view3d_draw_background(const bContext *C)
{
	/* TODO viewport */
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 *
 */
static void view3d_draw_render_solid_surfaces(const bContext *C, const bool run_screen_shaders)
{
	/* TODO viewport */
}

/**
 *
 */
static void view3d_draw_render_transparent_surfaces(const bContext *C)
{
	/* TODO viewport */
}

/**
 *
 */
static void view3d_draw_post_draw(const bContext *C)
{
	/* TODO viewport */
}

/* ******************** geometry overlay ***************** */

/**
* Front/back wire frames
*/
static void view3d_draw_wire_plates(const bContext *C)
{
	/* TODO viewport */
}

/**
* Special treatment for selected objects
*/
static void view3d_draw_outline_plates(const bContext *C)
{
	/* TODO viewport */
}

/* ******************** view loop ***************** */

/**
* Required if the shaders need it or external engines
* (e.g., Cycles requires depth buffer handled separately).
*/
static void view3d_draw_prerender_buffers(const bContext *C)
{
	/* TODO viewport */
}

/**
 * Draw all the plates that will fill the RGBD buffer
 */
static void view3d_draw_solid_plates(const bContext *C)
{
	view3d_draw_background(C);
	view3d_draw_render_solid_surfaces(C, true);
	view3d_draw_render_transparent_surfaces(C);
	view3d_draw_post_draw(C);
}

/**
 * Wires, outline, ...
 */
static void view3d_draw_geometry_overlay(const bContext *C)
{
	view3d_draw_wire_plates(C);
	view3d_draw_outline_plates(C);
}

/**
* Empties, lamps, parent lines, grid, ...
*/
static void view3d_draw_other_elements(const bContext *C)
{
	/* TODO viewport */
}

/**
* Paint brushes, armatures, ...
*/
static void view3d_draw_tool_ui(const bContext *C)
{
	/* TODO viewport */
}

/**
* Blueprint images
*/
static void view3d_draw_reference_images(const bContext *C)
{
	/* TODO viewport */
}

/**
* Grease Pencil
*/
static void view3d_draw_grease_pencil(const bContext *C)
{
	/* TODO viewport */
}

/**
* This could run once per view, or even in parallel
* for each of them. What is a "view"?
* - a viewport with the camera elsewhere
* - left/right stereo
* - panorama / fisheye individual cubemap faces
*/
static void view3d_draw_view(const bContext *C)
{
	/* TODO - Technically this should be drawn to a few FBO, so we can handle
	 * compositing better, but for now this will get the ball rolling (dfelinto) */

	view3d_draw_prerender_buffers(C);
	view3d_draw_solid_plates(C);
	view3d_draw_geometry_overlay(C);
	view3d_draw_other_elements(C);
	view3d_draw_tool_ui(C);
	view3d_draw_reference_images(C);
	view3d_draw_grease_pencil(C);
}

void view3d_main_region_draw(const bContext *C, ARegion *ar)
{
	View3D *v3d = CTX_wm_view3d(C);

	if (IS_VIEWPORT_LEGACY(v3d)) {
		view3d_main_region_draw_legacy(C, ar);
		return;
	}

	/* TODO viewport - there is so much to be done, in fact a lot will need to happen in the space_view3d.c
	 * before we even call the drawing routine, but let's move on for now (dfelinto)
	 * but this is a provisory way to start seeing things in the viewport */
	view3d_draw_view(C);
}

