/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegion;
struct Scene;
struct ScrArea;
struct bContext;
struct bDopeSheet;
struct rcti;
struct wmEvent;
struct wmWindow;

void ED_time_scrub_draw_current_frame(const ARegion *region,
                                      const Scene *scene,
                                      bool display_seconds,
                                      bool display_stalk = true);

void ED_time_scrub_draw(const ARegion *region,
                        const Scene *scene,
                        bool display_seconds,
                        bool discrete_frames);
/**
 * Scroll-bars shouldn't overlap the time scrub UI. So this returns a mask adjusted to exclude it,
 * which can be passed to #UI_view2d_scrollers_draw().
 *
 * \param scroller_mask: Typically #View2D.mask (or something smaller, if further parts have been
 * masked out already).
 */
rcti ED_time_scrub_clamp_scroller_mask(const rcti &scroller_mask);

bool ED_time_scrub_event_in_region(const ARegion *region, const wmEvent *event);
/** Follow the #EventHandlerPoll function signature. */
bool ED_time_scrub_event_in_region_poll(const wmWindow *win,
                                        const ScrArea *area,
                                        const ARegion *region,
                                        const wmEvent *event);

void ED_time_scrub_channel_search_draw(const bContext *C, ARegion *region, bDopeSheet *dopesheet);
void ED_time_scrub_region_rect_get(const ARegion *region, rcti *r_rect);
