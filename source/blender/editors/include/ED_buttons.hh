/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ScrArea;
struct SpaceProperties;
struct bContext;
struct PointerRNA;

/**
 * Fills an array with the tab context values for the properties editor. -1 signals a separator.
 *
 * \return The total number of items in the array returned.
 */
int ED_buttons_tabs_list(SpaceProperties *sbuts, short *context_tabs_array);
bool ED_buttons_tab_has_search_result(SpaceProperties *sbuts, int index);

void ED_buttons_search_string_set(SpaceProperties *sbuts, const char *value);
int ED_buttons_search_string_length(SpaceProperties *sbuts);
const char *ED_buttons_search_string_get(SpaceProperties *sbuts);

bool ED_buttons_should_sync_with_outliner(const bContext *C,
                                          const SpaceProperties *sbuts,
                                          ScrArea *area);
void ED_buttons_set_context(const bContext *C,
                            SpaceProperties *sbuts,
                            PointerRNA *ptr,
                            int context);
