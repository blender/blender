/**
 * $Id:
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your opt ion) any later version. The Blender
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BSE_TIME_H
#define BSE_TIME_H

struct ListBase;
struct View2D;

/* ******** Markers - General Api ********* */
void add_marker(int frame);
void duplicate_marker(void);
void remove_marker(void);
void rename_marker(void);
void transform_markers(int mode, int smode);

void borderselect_markers(void);
void deselect_markers(short test, short sel);
struct TimeMarker *find_nearest_marker(int clip_y);

void nextprev_marker(short dir);
void get_minmax_markers(short sel, float *first, float *last);
int find_nearest_marker_time(float dx);

void add_marker_to_cfra_elem(struct ListBase *lb, struct TimeMarker *marker, short only_sel);
void make_marker_cfra_list(struct ListBase *lb, short only_sel);

void draw_markers_timespace(int lines);
TimeMarker *get_frame_marker(int frame);

/* ******** Animation - Preview Range ************* */
void anim_previewrange_set(void);
void anim_previewrange_clear(void);

void draw_anim_preview_timespace(void);

/* *********** TimeLine Specific  ***************/
void timeline_frame_to_center(void);
void nextprev_timeline_key(short dir);

#endif

