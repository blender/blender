/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#ifndef __BKE_COLORBAND_H__
#define __BKE_COLORBAND_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ColorBand;

/** #ColorBand.data length. */
#define MAXCOLORBAND 32

void BKE_colorband_init(struct ColorBand *coba, bool rangetype);
void BKE_colorband_init_from_table_rgba(struct ColorBand *coba,
                                        const float (*array)[4],
                                        const int array_len,
                                        bool filter_sample);
struct ColorBand *BKE_colorband_add(bool rangetype);
bool BKE_colorband_evaluate(const struct ColorBand *coba, float in, float out[4]);
void BKE_colorband_evaluate_table_rgba(const struct ColorBand *coba, float **array, int *size);
struct CBData *BKE_colorband_element_add(struct ColorBand *coba, float position);
int BKE_colorband_element_remove(struct ColorBand *coba, int index);
void BKE_colorband_update_sort(struct ColorBand *coba);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_COLORBAND_H__ */
