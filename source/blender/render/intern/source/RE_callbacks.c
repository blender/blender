/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Callbacks to make the renderer interact with calling modules.
 */

#include <stdlib.h> /* for NULL??? */
#include "render.h"
#include "render_intern.h"
#include "RE_callbacks.h"

/*
 * The callbacks are done in three parts:
 *
 * - a local static pointer to the eventual function. NULL if not
 * defined, or if the behaviour is not required.
 *
 * - a hook that can be called locally
 *
 * - a hook that can be called externally, to set an external function
 * to provide said functionality.
 *
 * These might be generated from a spec such as:
 * 
 * callback {
 *    local    = <local name>
 *    external = <external name>
 *    type     = <ret_type> (<args>,...)
 * }
 *
 * Should generate:
 * - a static var
 * - an internal loop, plus decl.
 * - an external setter, plus decl.
 * 
 */

/* Part 1: ------------------------------------------------------------- */

static int (*RE_local_test_break_function)(void) = NULL;

static void (*RE_local_timecursor_function)(int) = NULL;

static void (*RE_local_renderdisplay_function)(int i, 
											   int j, 
											   int k, 
											   int l, 
											   unsigned int *m) 
	 = NULL;

static void (*RE_local_initrenderdisplay_function)(void) = NULL;
static void (*RE_local_clearrenderdisplay_function)(short) = NULL;

static void (*RE_local_printrenderinfo_function)(double,int) = NULL;

static void (*RE_local_getrenderdata_function)(void) = NULL;
static void (*RE_local_freerenderdata_function)(void) = NULL;

/* Part 2: ------------------------------------------------------------- */

int RE_local_test_break(void) {
	if (RE_local_test_break_function) {
		return RE_local_test_break_function();
	} else {
		/* transparant behaviour: proceed */
		return 0;
	}
}

void RE_local_timecursor(int i) {
	if (RE_local_timecursor_function) RE_local_timecursor_function(i);
}

void RE_local_render_display(int i, int j, int k, int l, unsigned int* m) {
	if (RE_local_renderdisplay_function) RE_local_renderdisplay_function(i, j, k, l, m);
}
void RE_local_init_render_display(void) {
	if (RE_local_initrenderdisplay_function) RE_local_initrenderdisplay_function();
}
void RE_local_clear_render_display(short i) {
	if (RE_local_clearrenderdisplay_function) RE_local_clearrenderdisplay_function(i);
}

void RE_local_printrenderinfo(double time, int i) {
	if (RE_local_printrenderinfo_function) RE_local_printrenderinfo_function(time, i);
}

void RE_local_get_renderdata(void) {
	if (RE_local_getrenderdata_function) RE_local_getrenderdata_function();
}
void RE_local_free_renderdata(void) {
	if (RE_local_freerenderdata_function) RE_local_freerenderdata_function();
}

/* Part 3: ------------------------------------------------------------- */

void RE_set_test_break_callback(int (*f)(void)) {
	RE_local_test_break_function = f;
}

void RE_set_timecursor_callback(void (*f)(int)) {
	RE_local_timecursor_function = f;
}

void RE_set_renderdisplay_callback(void (*f)(int i, 
											 int j, 
											 int k, 
											 int l, 
											 unsigned int *)) 
{
	RE_local_renderdisplay_function = f;
}

void RE_set_initrenderdisplay_callback(void (*f)(void)) {
	RE_local_initrenderdisplay_function = f;
}

void RE_set_clearrenderdisplay_callback(void (*f)(short)) {
	RE_local_clearrenderdisplay_function = f;
}

void RE_set_printrenderinfo_callback(void (*f)(double,int)) {
	RE_local_printrenderinfo_function = f;
}

void RE_set_getrenderdata_callback(void (*f)(void)) {
	RE_local_getrenderdata_function = f;
}

void RE_set_freerenderdata_callback(void (*f)(void)) {
	RE_local_freerenderdata_function = f;
}
