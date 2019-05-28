/*
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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_TIME_SCRUB_UI_H__
#define __ED_TIME_SCRUB_UI_H__

struct View2DGrid;
struct bContext;
struct bDopeSheet;
struct wmEvent;

void ED_time_scrub_draw(const struct ARegion *ar,
                        const struct Scene *scene,
                        bool display_seconds,
                        bool discrete_frames);

bool ED_time_scrub_event_in_region(const struct ARegion *ar, const struct wmEvent *event);

void ED_time_scrub_channel_search_draw(const struct bContext *C,
                                       struct ARegion *ar,
                                       struct bDopeSheet *dopesheet);

#endif /* __ED_TIME_SCRUB_UI_H__ */
