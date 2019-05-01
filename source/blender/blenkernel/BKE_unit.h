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
 */

#ifndef __BKE_UNIT_H__
#define __BKE_UNIT_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct UnitSettings;

/* in all cases the value is assumed to be scaled by the user preference */

/* humanly readable representation of a value in units (used for button drawing) */
size_t bUnit_AsString(
    char *str, int len_max, double value, int prec, int system, int type, bool split, bool pad);
size_t bUnit_AsString2(char *str,
                       int len_max,
                       double value,
                       int prec,
                       int type,
                       const struct UnitSettings *settings,
                       bool pad);

/* replace units with values, used before python button evaluation */
bool bUnit_ReplaceString(
    char *str, int len_max, const char *str_prev, double scale_pref, int system, int type);

/* return true if the string contains any valid unit for the given type */
bool bUnit_ContainsUnit(const char *str, int type);

/* if user does not specify a unit, multiply with this value */
double bUnit_PreferredInputUnitScalar(const struct UnitSettings *settings, int type);

/* make string keyboard-friendly: 10µm --> 10um */
void bUnit_ToUnitAltName(char *str, int len_max, const char *orig_str, int system, int type);

/* the size of the unit used for this value (used for calculating the ckickstep) */
double bUnit_ClosestScalar(double value, int system, int type);

/* base scale for these units */
double bUnit_BaseScalar(int system, int type);

/* return true is the unit system exists */
bool bUnit_IsValid(int system, int type);

/* loop over scales, could add names later */
// double bUnit_Iter(void **unit, char **name, int system, int type);

void bUnit_GetSystem(int system, int type, void const **r_usys_pt, int *r_len);
int bUnit_GetBaseUnit(const void *usys_pt);
int bUnit_GetBaseUnitOfType(int system, int type);
const char *bUnit_GetName(const void *usys_pt, int index);
const char *bUnit_GetNameDisplay(const void *usys_pt, int index);
const char *bUnit_GetIdentifier(const void *usys_pt, int index);
double bUnit_GetScaler(const void *usys_pt, int index);
bool bUnit_IsSuppressed(const void *usys_pt, int index);

/* aligned with PropertyUnit */
enum {
  B_UNIT_NONE = 0,
  B_UNIT_LENGTH = 1,
  B_UNIT_AREA = 2,
  B_UNIT_VOLUME = 3,
  B_UNIT_MASS = 4,
  B_UNIT_ROTATION = 5,
  B_UNIT_TIME = 6,
  B_UNIT_VELOCITY = 7,
  B_UNIT_ACCELERATION = 8,
  B_UNIT_CAMERA = 9,
  B_UNIT_POWER = 10,
  B_UNIT_TYPE_TOT = 11,
};

#ifdef __cplusplus
}
#endif

#endif /* __BKE_UNIT_H__ */
