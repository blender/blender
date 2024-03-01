/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
bool BLO_main_validate_libraries(struct Main *bmain, struct ReportList *reports);
/**
 * * Check (and fix if needed) that shape key's 'from' pointer is valid.
 */
bool BLO_main_validate_shapekeys(struct Main *bmain, struct ReportList *reports);
/**
 * Check that the `LIB_EMBEDDED_DATA_LIB_OVERRIDE` flag for embedded IDs actually matches reality
 * of embedded IDs being used by a liboverride ID.
 *
 * This is needed because embedded IDs did not get their flag properly cleared when runtime data
 * was split in `ID.tag`, which can create crashing situations in some rare cases, see #117795.
 */
void BLO_main_validate_embedded_liboverrides(struct Main *bmain, struct ReportList *reports);
