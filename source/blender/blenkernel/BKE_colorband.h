/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

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
                                        int array_len,
                                        bool filter_sample);
struct ColorBand *BKE_colorband_add(bool rangetype);
bool BKE_colorband_evaluate(const struct ColorBand *coba, float in, float out[4]);
void BKE_colorband_evaluate_table_rgba(const struct ColorBand *coba, float **array, int *size);
struct CBData *BKE_colorband_element_add(struct ColorBand *coba, float position);
bool BKE_colorband_element_remove(struct ColorBand *coba, int index);
void BKE_colorband_update_sort(struct ColorBand *coba);

#ifdef __cplusplus
}
#endif
