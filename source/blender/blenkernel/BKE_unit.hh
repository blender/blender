/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sys_types.h"

/** \file
 * \ingroup bke
 */

struct UnitSettings;

/* In all cases the value is assumed to be scaled by the user-preference. */

/**
 * Representation of a value in units.
 *
 * \param prec: Decimal places to show,
 * Use a negative number to prevent trailing zeros being stripped.
 * \param pad: When true & `prec` is positive, stripped zeroes will be replaced with a space
 * instead of being removed.
 *
 * \note Disabling stripping or enabling padding reduces text "jittering" when changing values,
 * especially with mono-spaced fonts. However, the values will take up more space,
 * so the default is good when there is little movement (e.g. numerical input or mesh overlays).
 */
size_t BKE_unit_value_as_string(char *str,
                                int str_maxncpy,
                                double value,
                                int prec,
                                int type,
                                const UnitSettings &settings,
                                bool pad);

/**
 * Humanly readable representation of a value in units (used for button drawing).
 *
 * \copydoc #BKE_unit_value_as_string.
 */
size_t BKE_unit_value_as_string_adaptive(char *str,
                                         int str_maxncpy,
                                         double value,
                                         int prec,
                                         int system,
                                         int type,
                                         bool split,
                                         bool pad);

/**
 * A version of #BKE_unit_value_as_string with the `value` scaled by #BKE_unit_value_scale.
 *
 * \copydoc #BKE_unit_value_as_string.
 */
size_t BKE_unit_value_as_string_scaled(char *str,
                                       int str_maxncpy,
                                       double value,
                                       int prec,
                                       int type,
                                       const UnitSettings &settings,
                                       bool pad);

/**
 * Replace units with values, used before python button evaluation.
 *
 * Make a copy of the string that replaces the units with numbers.
 * This is only used when evaluating user input and can afford to be a bit slower
 *
 * This is to be used before python evaluation so:
 * `10.1km -> 10.1*1000.0`
 * ...will be resolved by Python.
 *
 * Values will be split by an add sign:
 * `5'2" -> 5*0.3048 + 2*0.0254`
 *
 * \param str_prev: is optional, when valid it is used to get a base unit when none is set.
 *
 * \return True of a change was made.
 */
bool BKE_unit_replace_string(
    char *str, int str_maxncpy, const char *str_prev, double scale_pref, int system, int type);

/**
 * \return true if the string contains any valid unit for the given type.
 */
bool BKE_unit_string_contains_unit(const char *str, int type);

/**
 * If user does not specify a unit, this converts it to the unit from the settings.
 */
double BKE_unit_apply_preferred_unit(const UnitSettings &settings, int type, double value);

/**
 * Make string keyboard-friendly, e.g: `10µm -> 10um`.
 */
void BKE_unit_name_to_alt(char *str, int str_maxncpy, const char *orig_str, int system, int type);

/**
 * The size of the unit used for this value (used for calculating the click-step).
 */
double BKE_unit_closest_scalar(double value, int system, int type);

/**
 * Base scale for these units.
 */
double BKE_unit_base_scalar(int system, int type);

/**
 * \return true is the unit system exists.
 */
bool BKE_unit_is_valid(int system, int type);

/**
 * Apply the needed correction factor to value, based on unit_type
 * (only length-related are affected currently) and `unit->scale_length`.
 */
double BKE_unit_value_scale(const UnitSettings &settings, int unit_type, double value);

/**
 * Loop over scales, could add names later.
 */
// double bUnit_Iter(void **unit, char **name, int system, int type);

void BKE_unit_system_get(int system, int type, const void **r_usys_pt, int *r_len);
int BKE_unit_base_get(const void *usys_pt);
int BKE_unit_base_of_type_get(int system, int type);
const char *BKE_unit_name_get(const void *usys_pt, int index);
const char *BKE_unit_display_name_get(const void *usys_pt, int index);
const char *BKE_unit_identifier_get(const void *usys_pt, int index);
double BKE_unit_scalar_get(const void *usys_pt, int index);
bool BKE_unit_is_suppressed(const void *usys_pt, int index);

/** Aligned with #PropertyUnit and `bpyunits_ucategories_items` in `bpy_utils_units.cc`. */
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
  B_UNIT_WAVELENGTH = 13,
  B_UNIT_COLOR_TEMPERATURE = 14,
  B_UNIT_FREQUENCY = 15,
  B_UNIT_TYPE_TOT = 16,
};
