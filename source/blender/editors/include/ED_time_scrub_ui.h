/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct bDopeSheet;
struct wmEvent;

void ED_time_scrub_draw_current_frame(const struct ARegion *region,
                                      const struct Scene *scene,
                                      bool display_seconds);

void ED_time_scrub_draw(const struct ARegion *region,
                        const struct Scene *scene,
                        bool display_seconds,
                        bool discrete_frames);

bool ED_time_scrub_event_in_region(const struct ARegion *region, const struct wmEvent *event);

void ED_time_scrub_channel_search_draw(const struct bContext *C,
                                       struct ARegion *region,
                                       struct bDopeSheet *dopesheet);

#ifdef __cplusplus
}
#endif
