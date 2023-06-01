/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup avi
 */

#pragma once

#include <stdio.h> /* for FILE */

unsigned int GET_FCC(FILE *fp);
unsigned int GET_TCC(FILE *fp);

#define PUT_FCC(ch4, fp) \
  { \
    putc(ch4[0], fp); \
    putc(ch4[1], fp); \
    putc(ch4[2], fp); \
    putc(ch4[3], fp); \
  } \
  (void)0

#define PUT_FCCN(num, fp) \
  { \
    putc((num >> 0) & 0377, fp); \
    putc((num >> 8) & 0377, fp); \
    putc((num >> 16) & 0377, fp); \
    putc((num >> 24) & 0377, fp); \
  } \
  (void)0

#define PUT_TCC(ch2, fp) \
  { \
    putc(ch2[0], fp); \
    putc(ch2[1], fp); \
  } \
  (void)0

void *avi_format_convert(
    AviMovie *movie, int stream, void *buffer, AviFormat from, AviFormat to, size_t *size);

int avi_get_data_id(AviFormat format, int stream);
int avi_get_format_type(AviFormat format);
int avi_get_format_fcc(AviFormat format);
int avi_get_format_compression(AviFormat format);
