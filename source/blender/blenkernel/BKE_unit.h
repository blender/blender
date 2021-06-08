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

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct UnitSettings;

/* in all cases the value is assumed to be scaled by the user preference */

/* humanly readable representation of a value in units (used for button drawing) */
size_t BKE_unit_value_as_string_adaptive(
    char *str, int len_max, double value, int prec, int system, int type, bool split, bool pad);
size_t BKE_unit_value_as_string(char *str,
                                int len_max,
                                double value,
                                int prec,
                                int type,
                                const struct UnitSettings *settings,
                                bool pad);

/* replace units with values, used before python button evaluation */
bool BKE_unit_replace_string(
    char *str, int len_max, const char *str_prev, double scale_pref, int system, int type);

/* return true if the string contains any valid unit for the given type */
bool BKE_unit_string_contains_unit(const char *str, int type);

/* If user does not specify a unit, this converts it to the unit from the settings. */
double BKE_unit_apply_preferred_unit(const struct UnitSettings *settings, int type, double value);

/* make string keyboard-friendly: 10µm --> 10um */
void BKE_unit_name_to_alt(char *str, int len_max, const char *orig_str, int system, int type);

/* the size of the unit used for this value (used for calculating the ckickstep) */
double BKE_unit_closest_scalar(double value, int system, int type);

/* base scale for these units */
double BKE_unit_base_scalar(int system, int type);

/* return true is the unit system exists */
bool BKE_unit_is_valid(int system, int type);

/* loop over scales, could add names later */
// double bUnit_Iter(void **unit, char **name, int system, int type);

void BKE_unit_system_get(int system, int type, const void **r_usys_pt, int *r_len);
int BKE_unit_base_get(const void *usys_pt);
int BKE_unit_base_of_type_get(int system, int type);
const char *BKE_unit_name_get(const void *usys_pt, int index);
const char *BKE_unit_display_name_get(const void *usys_pt, int index);
const char *BKE_unit_identifier_get(const void *usys_pt, int index);
double BKE_unit_scalar_get(const void *usys_pt, int index);
bool BKE_unit_is_suppressed(const void *usys_pt, int index);

/* aligned with PropertyUnit */
enum {
  B_UNIT_NONE = 0,
  B_UNIT_LENGTH = 1,
  B_UNIT_AREA = 2,
  B_UNIT_VOLUME = 3,
  B_UNIT_MASS = 4,
  B_UNIT_ROTATION = 5,
  B_UNIT_TIME = 6,
  B_UNIT_TIME_ABSOLUTE = 7,
  B_UNIT_VELOCITY = 8,
  B_UNIT_ACCELERATION = 9,
  B_UNIT_CAMERA = 10,
  B_UNIT_POWER = 11,
  B_UNIT_TEMPERATURE = 12,
  B_UNIT_TYPE_TOT = 13,
};

#ifdef __cplusplus
}
#endif
