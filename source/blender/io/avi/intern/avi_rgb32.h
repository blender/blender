/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup avi
 */

#pragma once

void *avi_converter_from_rgb32(AviMovie *movie, int stream, unsigned char *buffer, size_t *size);
void *avi_converter_to_rgb32(AviMovie *movie, int stream, unsigned char *buffer, size_t *size);
