/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Utility functions for the client-side-decorations (CSD)
 * implementation for WAYLAND.
 */

#pragma once

#ifdef WITH_GHOST_CSD

#  include "GHOST_SystemWayland.hh"

#  include <string_view>

struct GHOST_CSD_Layout;

/**
 * Fill `layout` from the desktop's button layout,
 * a string such as `icon:minimize,maximize,close`.
 *
 * \return false when no known buttons are found (`layout` should be ignored),
 * unknown button names are skipped as GNOME does.
 */
bool GHOST_WindowCSD_LayoutFromString(GHOST_CSD_Layout &layout, std::string_view buttons);
void GHOST_WindowCSD_LayoutDefault(GHOST_CSD_Layout &layout);

/** Return true if CSD should be used. */
bool GHOST_WindowCSD_Check(GWL_CurrentDesktopType desktop_type);

#else
#  error "WITH_GHOST_CSD must be defined"
#endif
