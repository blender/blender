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

struct GHOST_CSD_Layout;
bool GHOST_WindowCSD_LayoutFromSystem(GHOST_CSD_Layout &layout);
void GHOST_WindowCSD_LayoutDefault(GHOST_CSD_Layout &layout);

/** Return true if CSD should be used. */
bool GHOST_WindowCSD_Check(GWL_CurrentDesktopType desktop_type);

#else
#  error "WITH_GHOST_CSD must be defined"
#endif
