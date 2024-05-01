/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup blenloader
 * \brief Utilities ensuring `.blend` file (i.e. Main)
 * is in valid state during write and/or read process.
 */

struct Main;
struct ReportList;

/**
 * Check (but do *not* fix) that all linked data-blocks are still valid
 * (i.e. pointing to the right library).
 */
bool BLO_main_validate_libraries(Main *bmain, ReportList *reports);
/**
 * * Check (and fix if needed) that shape key's 'from' pointer is valid.
 */
bool BLO_main_validate_shapekeys(Main *bmain, ReportList *reports);

/**
 * Check that the `LIB_EMBEDDED_DATA_LIB_OVERRIDE` flag for embedded IDs actually matches reality
 * of embedded IDs being used by a liboverride ID.
 *
 * This is needed because embedded IDs did not get their flag properly cleared when runtime data
 * was split in `ID.tag`, which can create crashing situations in some rare cases, see #117795.
 */
void BLO_main_validate_embedded_liboverrides(Main *bmain, ReportList *reports);

/**
 * Check that the `LIB_EMBEDDED_DATA` flag is correctly set for embedded IDs, and not for any Main
 * ID.
 *
 * NOTE: It is unknown why/how this can happen, but there are some files out there that have e.g.
 * Objects flagged as embedded data... See e.g. the `(Anim) Hero p23 for 2.blend` file from our
 * cloud gallery (https://cloud.blender.org/p/gallery/5b642e25bf419c1042056fc6).
 */
void BLO_main_validate_embedded_flag(Main *bmain, ReportList *reports);
