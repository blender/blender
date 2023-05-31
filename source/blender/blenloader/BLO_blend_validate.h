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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check (but do *not* fix) that all linked data-blocks are still valid
 * (i.e. pointing to the right library).
 */
bool BLO_main_validate_libraries(struct Main *bmain, struct ReportList *reports);
/**
 * * Check (and fix if needed) that shape key's 'from' pointer is valid.
 */
bool BLO_main_validate_shapekeys(struct Main *bmain, struct ReportList *reports);

#ifdef __cplusplus
}
#endif
