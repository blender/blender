/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup avi
 *
 * This is external code.
 */

#pragma once

#define AVI_RAW 0
#define AVI_CHUNK 1
#define AVI_LIST 2
#define AVI_MAINH 3
#define AVI_STREAMH 4
#define AVI_BITMAPH 5
#define AVI_INDEXE 6
#define AVI_MJPEGU 7

void awrite(AviMovie *movie, void *datain, int block, int size, FILE *fp, int type);
