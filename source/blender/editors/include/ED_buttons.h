/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ScrArea;
struct SpaceProperties;
struct bContext;

/**
 * Fills an array with the tab context values for the properties editor. -1 signals a separator.
 *
 * \return The total number of items in the array returned.
 */
int ED_buttons_tabs_list(struct SpaceProperties *sbuts, short *context_tabs_array);
bool ED_buttons_tab_has_search_result(struct SpaceProperties *sbuts, int index);

void ED_buttons_search_string_set(struct SpaceProperties *sbuts, const char *value);
int ED_buttons_search_string_length(struct SpaceProperties *sbuts);
const char *ED_buttons_search_string_get(struct SpaceProperties *sbuts);

bool ED_buttons_should_sync_with_outliner(const struct bContext *C,
                                          const struct SpaceProperties *sbuts,
                                          struct ScrArea *area);
void ED_buttons_set_context(const struct bContext *C,
                            struct SpaceProperties *sbuts,
                            PointerRNA *ptr,
                            int context);

#ifdef __cplusplus
}
#endif
