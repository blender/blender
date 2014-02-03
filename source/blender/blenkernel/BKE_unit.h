/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_UNIT_H__
#define __BKE_UNIT_H__

/** \file BKE_unit.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

/* in all cases the value is assumed to be scaled by the user preference */

/* humanly readable representation of a value in units (used for button drawing) */
size_t  bUnit_AsString(char *str, int len_max, double value, int prec, int system, int type, bool split, bool pad);

/* replace units with values, used before python button evaluation */
int     bUnit_ReplaceString(char *str, int len_max, const char *str_prev, double scale_pref, int system, int type);

/* make string keyboard-friendly: 10µm --> 10um */
void bUnit_ToUnitAltName(char *str, int len_max, const char *orig_str, int system, int type);

/* the size of the unit used for this value (used for calculating the ckickstep) */
double bUnit_ClosestScalar(double value, int system, int type);

/* base scale for these units */
double bUnit_BaseScalar(int system, int type);

/* return true is the unit system exists */
int bUnit_IsValid(int system, int type);

/* loop over scales, coudl add names later */
//double bUnit_Iter(void **unit, char **name, int system, int type);

void        bUnit_GetSystem(void **usys_pt, int *len, int system, int type);
int         bUnit_GetBaseUnit(void *usys_pt);
const char *bUnit_GetName(void *usys_pt, int index);
const char *bUnit_GetNameDisplay(void *usys_pt, int index);
double      bUnit_GetScaler(void *usys_pt, int index);

/* aligned with PropertyUnit */
#define     B_UNIT_NONE 0
#define     B_UNIT_LENGTH 1
#define     B_UNIT_AREA 2
#define     B_UNIT_VOLUME 3
#define     B_UNIT_MASS 4
#define     B_UNIT_ROTATION 5
#define     B_UNIT_TIME 6
#define     B_UNIT_VELOCITY 7
#define     B_UNIT_ACCELERATION 8
#define     B_UNIT_CAMERA 9
#define     B_UNIT_TYPE_TOT 10

#ifdef __cplusplus
}
#endif

#endif /* __BKE_UNIT_H__ */
