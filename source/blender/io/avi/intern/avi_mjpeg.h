/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup avi
 */

#pragma once

void *avi_converter_from_mjpeg(AviMovie *movie, int stream, uchar *buffer, const size_t *size);
void *avi_converter_to_mjpeg(AviMovie *movie, int stream, uchar *buffer, size_t *size);
