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

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "BLI_sys_types.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "DNA_scene_types.h"

#include "BKE_unit.h" /* own include */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* no BKE or DNA includes! */

/* Keep alignment. */
/* clang-format off */

#define TEMP_STR_SIZE 256

#define SEP_CHR     '#'
#define SEP_STR     "#"

#define EPS 0.001

#define UN_SC_KM    1000.0f
#define UN_SC_HM    100.0f
#define UN_SC_DAM   10.0f
#define UN_SC_M     1.0f
#define UN_SC_DM    0.1f
#define UN_SC_CM    0.01f
#define UN_SC_MM    0.001f
#define UN_SC_UM    0.000001f

#define UN_SC_MI    1609.344f
#define UN_SC_FUR   201.168f
#define UN_SC_CH    20.1168f
#define UN_SC_YD    0.9144f
#define UN_SC_FT    0.3048f
#define UN_SC_IN    0.0254f
#define UN_SC_MIL   0.0000254f

#define UN_SC_MTON  1000.0f /* metric ton */
#define UN_SC_QL    100.0f
#define UN_SC_KG    1.0f
#define UN_SC_HG    0.1f
#define UN_SC_DAG   0.01f
#define UN_SC_G     0.001f
#define UN_SC_MG    0.000001f

#define UN_SC_ITON  907.18474f /* imperial ton */
#define UN_SC_CWT   45.359237f
#define UN_SC_ST    6.35029318f
#define UN_SC_LB    0.45359237f
#define UN_SC_OZ    0.028349523125f

/* clang-format on */

/* define a single unit */
typedef struct bUnitDef {
  const char *name;
  /** abused a bit for the display name */
  const char *name_plural;
  /** this is used for display*/
  const char *name_short;
  /** keyboard-friendly ASCII-only version of name_short, can be NULL */
  const char *name_alt;
  /* if name_short has non-ASCII chars, name_alt should be present */

  /** can be NULL */
  const char *name_display;
  /** when NULL, a transformed version of the name will be taken */
  const char *identifier;

  double scalar;
  /** not used yet, needed for converting temperature */
  double bias;
  int flag;
} bUnitDef;

enum {
  B_UNIT_DEF_NONE = 0,
  /** Use for units that are not used enough to be translated into for common use */
  B_UNIT_DEF_SUPPRESS = 1,
  /** Display a unit even if its value is 0.1, eg 0.1mm instead of 100um */
  B_UNIT_DEF_TENTH = 2,
  /** Short unit name is case sensitive, for example to distinguish mW and MW */
  B_UNIT_DEF_CASE_SENSITIVE = 4,
  /** Short unit name does not have space between it and preceding number */
  B_UNIT_DEF_NO_SPACE = 8,
};

/* define a single unit */
typedef struct bUnitCollection {
  const struct bUnitDef *units;
  /** basic unit index (when user doesn't specify unit explicitly) */
  int base_unit;
  /** options for this system */
  int flag;
  /** to quickly find the last item */
  int length;
} bUnitCollection;

/* Keep table lignment. */
/* clang-format off */

#define UNIT_COLLECTION_LENGTH(def) (sizeof(def) / sizeof(bUnitDef) - 1)
#define NULL_UNIT {NULL, NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}

/* Dummy */
static struct bUnitDef buDummyDef[] = { {"", NULL, "", NULL, NULL, NULL, 1.0, 0.0}, NULL_UNIT};
static struct bUnitCollection buDummyCollection = {buDummyDef, 0, 0, sizeof(buDummyDef)};

/* Lengths */
static struct bUnitDef buMetricLenDef[] = {
  {"kilometer",  "kilometers",  "km",  NULL, "Kilometers",     "KILOMETERS",  UN_SC_KM,  0.0, B_UNIT_DEF_NONE},
  {"hectometer", "hectometers", "hm",  NULL, "100 Meters",     "HECTOMETERS", UN_SC_HM,  0.0, B_UNIT_DEF_SUPPRESS},
  {"dekameter",  "dekameters",  "dam", NULL, "10 Meters",      "DEKAMETERS",  UN_SC_DAM, 0.0, B_UNIT_DEF_SUPPRESS},
  {"meter",      "meters",      "m",   NULL, "Meters",         "METERS",      UN_SC_M,   0.0, B_UNIT_DEF_NONE},     /* base unit */
  {"decimeter",  "decimeters",  "dm",  NULL, "10 Centimeters", "DECIMETERS",  UN_SC_DM,  0.0, B_UNIT_DEF_SUPPRESS},
  {"centimeter", "centimeters", "cm",  NULL, "Centimeters",    "CENTIMETERS", UN_SC_CM,  0.0, B_UNIT_DEF_NONE},
  {"millimeter", "millimeters", "mm",  NULL, "Millimeters",    "MILLIMETERS", UN_SC_MM,  0.0, B_UNIT_DEF_NONE | B_UNIT_DEF_TENTH},
  {"micrometer", "micrometers", "µm",  "um", "Micrometers",    "MICROMETERS", UN_SC_UM,  0.0, B_UNIT_DEF_NONE},

  /* These get displayed because of float precision problems in the transform header,
   * could work around, but for now probably people wont use these */
#if 0
  {"nanometer", "Nanometers",     "nm", NULL, 0.000000001, 0.0,   B_UNIT_DEF_NONE},
  {"picometer", "Picometers",     "pm", NULL, 0.000000000001, 0.0, B_UNIT_DEF_NONE},
#endif
  NULL_UNIT,
};
static const struct bUnitCollection buMetricLenCollection = {buMetricLenDef, 3, 0, UNIT_COLLECTION_LENGTH(buMetricLenDef)};

static struct bUnitDef buImperialLenDef[] = {
  {"mile",    "miles",    "mi",   NULL, "Miles",    "MILES",    UN_SC_MI,  0.0, B_UNIT_DEF_NONE},
  {"furlong", "furlongs", "fur",  NULL, "Furlongs", "FURLONGS", UN_SC_FUR, 0.0, B_UNIT_DEF_SUPPRESS},
  {"chain",   "chains",   "ch",   NULL, "Chains",   "CHAINS",   UN_SC_CH,  0.0, B_UNIT_DEF_SUPPRESS},
  {"yard",    "yards",    "yd",   NULL, "Yards",    "YARDS",    UN_SC_YD,  0.0, B_UNIT_DEF_SUPPRESS},
  {"foot",    "feet",     "'",    "ft", "Feet",     "FEET",     UN_SC_FT,  0.0, B_UNIT_DEF_NONE | B_UNIT_DEF_NO_SPACE}, /* base unit */
  {"inch",    "inches",   "\"",   "in", "Inches",   "INCHES",   UN_SC_IN,  0.0, B_UNIT_DEF_NONE | B_UNIT_DEF_NO_SPACE},
  {"thou",    "thou",     "thou", "mil", "Thou",    "THOU",     UN_SC_MIL, 0.0, B_UNIT_DEF_NONE}, /* plural for thou has no 's' */
  NULL_UNIT,
};
static struct bUnitCollection buImperialLenCollection = {buImperialLenDef, 4, 0, UNIT_COLLECTION_LENGTH(buImperialLenDef)};

/* Areas */
static struct bUnitDef buMetricAreaDef[] = {
  {"square kilometer",  "square kilometers",  "km²",  "km2",  "Square Kilometers",  NULL, UN_SC_KM * UN_SC_KM,   0.0, B_UNIT_DEF_NONE},
  {"square hectometer", "square hectometers", "hm²",  "hm2",  "Square Hectometers", NULL, UN_SC_HM * UN_SC_HM,   0.0, B_UNIT_DEF_SUPPRESS},   /* hectare */
  {"square dekameter",  "square dekameters",  "dam²", "dam2", "Square Dekameters",  NULL, UN_SC_DAM * UN_SC_DAM, 0.0, B_UNIT_DEF_SUPPRESS},  /* are */
  {"square meter",      "square meters",      "m²",   "m2",   "Square Meters",      NULL, UN_SC_M * UN_SC_M,     0.0, B_UNIT_DEF_NONE},   /* base unit */
  {"square decimeter",  "square decimetees",  "dm²",  "dm2",  "Square Decimeters",  NULL, UN_SC_DM * UN_SC_DM,   0.0, B_UNIT_DEF_SUPPRESS},
  {"square centimeter", "square centimeters", "cm²",  "cm2",  "Square Centimeters", NULL, UN_SC_CM * UN_SC_CM,   0.0, B_UNIT_DEF_NONE},
  {"square millimeter", "square millimeters", "mm²",  "mm2",  "Square Millimeters", NULL, UN_SC_MM * UN_SC_MM,   0.0, B_UNIT_DEF_NONE | B_UNIT_DEF_TENTH},
  {"square micrometer", "square micrometers", "µm²",  "um2",  "Square Micrometers", NULL, UN_SC_UM * UN_SC_UM,   0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buMetricAreaCollection = {buMetricAreaDef, 3, 0, UNIT_COLLECTION_LENGTH(buMetricAreaDef)};

static struct bUnitDef buImperialAreaDef[] = {
  {"square mile",    "square miles",    "sq mi", "sq m", "Square Miles",    NULL, UN_SC_MI * UN_SC_MI,   0.0, B_UNIT_DEF_NONE},
  {"square furlong", "square furlongs", "sq fur", NULL,  "Square Furlongs", NULL, UN_SC_FUR * UN_SC_FUR, 0.0, B_UNIT_DEF_SUPPRESS},
  {"square chain",   "square chains",   "sq ch",  NULL,  "Square Chains",   NULL, UN_SC_CH * UN_SC_CH,   0.0, B_UNIT_DEF_SUPPRESS},
  {"square yard",    "square yards",    "sq yd",  NULL,  "Square Yards",    NULL, UN_SC_YD * UN_SC_YD,   0.0, B_UNIT_DEF_NONE},
  {"square foot",    "square feet",     "sq ft",  NULL,  "Square Feet",     NULL, UN_SC_FT * UN_SC_FT,   0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"square inch",    "square inches",   "sq in",  NULL,  "Square Inches",   NULL, UN_SC_IN * UN_SC_IN,   0.0, B_UNIT_DEF_NONE},
  {"square thou",    "square thou",     "sq mil", NULL,  "Square Thou",     NULL, UN_SC_MIL * UN_SC_MIL, 0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buImperialAreaCollection = {buImperialAreaDef, 4, 0, UNIT_COLLECTION_LENGTH(buImperialAreaDef)};

/* Volumes */
static struct bUnitDef buMetricVolDef[] = {
  {"cubic kilometer",  "cubic kilometers",  "km³",  "km3",  "Cubic Kilometers",  NULL, UN_SC_KM * UN_SC_KM * UN_SC_KM,    0.0, B_UNIT_DEF_NONE},
  {"cubic hectometer", "cubic hectometers", "hm³",  "hm3",  "Cubic Hectometers", NULL, UN_SC_HM * UN_SC_HM * UN_SC_HM,    0.0, B_UNIT_DEF_SUPPRESS},
  {"cubic dekameter",  "cubic dekameters",  "dam³", "dam3", "Cubic Dekameters",  NULL, UN_SC_DAM * UN_SC_DAM * UN_SC_DAM, 0.0, B_UNIT_DEF_SUPPRESS},
  {"cubic meter",      "cubic meters",      "m³",   "m3",   "Cubic Meters",      NULL, UN_SC_M * UN_SC_M * UN_SC_M,       0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"cubic decimeter",  "cubic decimeters",  "dm³",  "dm3",  "Cubic Decimeters",  NULL, UN_SC_DM * UN_SC_DM * UN_SC_DM,    0.0, B_UNIT_DEF_SUPPRESS},
  {"cubic centimeter", "cubic centimeters", "cm³",  "cm3",  "Cubic Centimeters", NULL, UN_SC_CM * UN_SC_CM * UN_SC_CM,    0.0, B_UNIT_DEF_NONE},
  {"cubic millimeter", "cubic millimeters", "mm³",  "mm3",  "Cubic Millimeters", NULL, UN_SC_MM * UN_SC_MM * UN_SC_MM,    0.0, B_UNIT_DEF_NONE | B_UNIT_DEF_TENTH},
  {"cubic micrometer", "cubic micrometers", "µm³",  "um3",  "Cubic Micrometers", NULL, UN_SC_UM * UN_SC_UM * UN_SC_UM,    0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buMetricVolCollection = {buMetricVolDef, 3, 0, UNIT_COLLECTION_LENGTH(buMetricVolDef)};

static struct bUnitDef buImperialVolDef[] = {
  {"cubic mile",    "cubic miles",    "cu mi",  "cu m", "Cubic Miles",    NULL, UN_SC_MI * UN_SC_MI * UN_SC_MI,    0.0, B_UNIT_DEF_NONE},
  {"cubic furlong", "cubic furlongs", "cu fur", NULL,   "Cubic Furlongs", NULL, UN_SC_FUR * UN_SC_FUR * UN_SC_FUR, 0.0, B_UNIT_DEF_SUPPRESS},
  {"cubic chain",   "cubic chains",   "cu ch",  NULL,   "Cubic Chains",   NULL, UN_SC_CH * UN_SC_CH * UN_SC_CH,    0.0, B_UNIT_DEF_SUPPRESS},
  {"cubic yard",    "cubic yards",    "cu yd",  NULL,   "Cubic Yards",    NULL, UN_SC_YD * UN_SC_YD * UN_SC_YD,    0.0, B_UNIT_DEF_NONE},
  {"cubic foot",    "cubic feet",     "cu ft",  NULL,   "Cubic Feet",     NULL, UN_SC_FT * UN_SC_FT * UN_SC_FT,    0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"cubic inch",    "cubic inches",   "cu in",  NULL,   "Cubic Inches",   NULL, UN_SC_IN * UN_SC_IN * UN_SC_IN,    0.0, B_UNIT_DEF_NONE},
  {"cubic thou",    "cubic thou",     "cu mil", NULL,   "Cubic Thou",     NULL, UN_SC_MIL * UN_SC_MIL * UN_SC_MIL, 0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buImperialVolCollection = {buImperialVolDef, 4, 0, UNIT_COLLECTION_LENGTH(buImperialVolDef)};

/* Mass */
static struct bUnitDef buMetricMassDef[] = {
  {"ton",       "tonnes",     "ton", "t",  "Tonnes",        "TONNES",       UN_SC_MTON, 0.0, B_UNIT_DEF_NONE},
  {"quintal",   "quintals",   "ql",  "q",  "100 Kilograms", "QUINTALS",   UN_SC_QL,   0.0, B_UNIT_DEF_SUPPRESS},
  {"kilogram",  "kilograms",  "kg",  NULL, "Kilograms",     "KILOGRAMS",  UN_SC_KG,   0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"hectogram", "hectograms", "hg",  NULL, "Hectograms",    "HECTOGRAMS", UN_SC_HG,   0.0, B_UNIT_DEF_SUPPRESS},
  {"dekagram",  "dekagrams",  "dag", NULL, "10 Grams",      "DEKAGRAMS",  UN_SC_DAG,  0.0, B_UNIT_DEF_SUPPRESS},
  {"gram",      "grams",      "g",   NULL, "Grams",         "GRAMS",      UN_SC_G,    0.0, B_UNIT_DEF_NONE},
  {"milligram", "milligrams", "mg",  NULL, "Milligrams",    "MILLIGRAMS", UN_SC_MG,   0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buMetricMassCollection = {buMetricMassDef, 2, 0, UNIT_COLLECTION_LENGTH(buMetricMassDef)};

static struct bUnitDef buImperialMassDef[] = {
  {"ton",           "tonnes",         "ton", "t",  "Tonnes",         "TONNES",         UN_SC_ITON, 0.0, B_UNIT_DEF_NONE},
  {"centum weight", "centum weights", "cwt", NULL, "Centum weights", "CENTUM_WEIGHTS", UN_SC_CWT,  0.0, B_UNIT_DEF_NONE},
  {"stone",         "stones",         "st",  NULL, "Stones",         "STONES",         UN_SC_ST,   0.0, B_UNIT_DEF_NONE},
  {"pound",         "pounds",         "lb",  NULL, "Pounds",         "POUNDS",         UN_SC_LB,   0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"ounce",         "ounces",         "oz",  NULL, "Ounces",         "OUNCES",         UN_SC_OZ,   0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buImperialMassCollection = {buImperialMassDef, 3, 0, UNIT_COLLECTION_LENGTH(buImperialMassDef)};

/* Even if user scales the system to a point where km^3 is used, velocity and
 * acceleration aren't scaled: that's why we have so few units for them */

/* Velocity */
static struct bUnitDef buMetricVelDef[] = {
  {"meter per second",   "meters per second",   "m/s",  NULL, "Meters per second",   NULL, UN_SC_M,            0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"kilometer per hour", "kilometers per hour", "km/h", NULL, "Kilometers per hour", NULL, UN_SC_KM / 3600.0f, 0.0, B_UNIT_DEF_SUPPRESS},
  NULL_UNIT,
};
static struct bUnitCollection buMetricVelCollection = {buMetricVelDef, 0, 0, UNIT_COLLECTION_LENGTH(buMetricVelDef)};

static struct bUnitDef buImperialVelDef[] = {
  {"foot per second", "feet per second", "ft/s", "fps", "Feet per second", NULL, UN_SC_FT,           0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"mile per hour",   "miles per hour",  "mph",  NULL,  "Miles per hour",  NULL, UN_SC_MI / 3600.0f, 0.0, B_UNIT_DEF_SUPPRESS},
  NULL_UNIT,
};
static struct bUnitCollection buImperialVelCollection = {buImperialVelDef, 0, 0, UNIT_COLLECTION_LENGTH(buImperialVelDef)};

/* Acceleration */
static struct bUnitDef buMetricAclDef[] = {
  {"meter per second squared", "meters per second squared", "m/s²", "m/s2", "Meters per second squared", NULL, UN_SC_M, 0.0, B_UNIT_DEF_NONE}, /* base unit */
  NULL_UNIT,
};
static struct bUnitCollection buMetricAclCollection = {buMetricAclDef, 0, 0, UNIT_COLLECTION_LENGTH(buMetricAclDef)};

static struct bUnitDef buImperialAclDef[] = {
  {"foot per second squared", "feet per second squared", "ft/s²", "ft/s2", "Feet per second squared", NULL, UN_SC_FT, 0.0, B_UNIT_DEF_NONE}, /* base unit */
  NULL_UNIT,
};
static struct bUnitCollection buImperialAclCollection = {buImperialAclDef, 0, 0, UNIT_COLLECTION_LENGTH(buImperialAclDef)};

/* Time */
static struct bUnitDef buNaturalTimeDef[] = {
  /* weeks? - probably not needed for blender */
  {"day",         "days",         "d",   NULL, "Days",         "DAYS",     90000.0,      0.0, B_UNIT_DEF_NONE},
  {"hour",        "hours",        "hr",  "h",  "Hours",        "HOURS",     3600.0,      0.0, B_UNIT_DEF_NONE},
  {"minute",      "minutes",      "min", "m",  "Minutes",      "MINUTES",     60.0,      0.0, B_UNIT_DEF_NONE},
  {"second",      "seconds",      "sec", "s",  "Seconds",      "SECONDS",      1.0,      0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"millisecond", "milliseconds", "ms",  NULL, "Milliseconds", "MILLISECONDS", 0.001,    0.0, B_UNIT_DEF_NONE},
  {"microsecond", "microseconds", "µs",  "us", "Microseconds", "MICROSECONDS", 0.000001, 0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buNaturalTimeCollection = {buNaturalTimeDef, 3, 0, UNIT_COLLECTION_LENGTH(buNaturalTimeDef)};


static struct bUnitDef buNaturalRotDef[] = {
  {"degree",    "degrees",     "°",  "d",   "Degrees",    "DEGREES",    M_PI / 180.0,             0.0,  B_UNIT_DEF_NONE | B_UNIT_DEF_NO_SPACE},
  /* arcminutes/arcseconds are used in Astronomy/Navigation areas... */
  {"arcminute", "arcminutes",  "'",  NULL,  "Arcminutes", "ARCMINUTES", (M_PI / 180.0) / 60.0,    0.0,  B_UNIT_DEF_SUPPRESS | B_UNIT_DEF_NO_SPACE},
  {"arcsecond", "arcseconds",  "\"", NULL,  "Arcseconds", "ARCSECONDS", (M_PI / 180.0) / 3600.0,  0.0,  B_UNIT_DEF_SUPPRESS | B_UNIT_DEF_NO_SPACE},
  {"radian",    "radians",     "r",  NULL,  "Radians",    "RADIANS",    1.0,                      0.0,  B_UNIT_DEF_NONE},
//  {"turn",      "turns",       "t",  NULL,  "Turns",      NULL, 1.0 / (M_PI * 2.0),       0.0,  B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buNaturalRotCollection = {buNaturalRotDef, 0, 0, UNIT_COLLECTION_LENGTH(buNaturalRotDef)};

/* Camera Lengths */
static struct bUnitDef buCameraLenDef[] = {
  {"meter",      "meters",      "m",   NULL, "Meters",         NULL, UN_SC_KM,  0.0, B_UNIT_DEF_NONE},     /* base unit */
  {"decimeter",  "decimeters",  "dm",  NULL, "10 Centimeters", NULL, UN_SC_HM,  0.0, B_UNIT_DEF_SUPPRESS},
  {"centimeter", "centimeters", "cm",  NULL, "Centimeters",    NULL, UN_SC_DAM, 0.0, B_UNIT_DEF_SUPPRESS},
  {"millimeter", "millimeters", "mm",  NULL, "Millimeters",    NULL, UN_SC_M,   0.0, B_UNIT_DEF_NONE},
  {"micrometer", "micrometers", "µm", "um",  "Micrometers",    NULL, UN_SC_MM,  0.0, B_UNIT_DEF_SUPPRESS},
  NULL_UNIT,
};
static struct bUnitCollection buCameraLenCollection = {buCameraLenDef, 3, 0, UNIT_COLLECTION_LENGTH(buCameraLenDef)};

/* (Light) Power */
static struct bUnitDef buPowerDef[] = {
  {"gigawatt",  "gigawatts",  "GW", NULL, "Gigawatts",  NULL, 1e9f,  0.0, B_UNIT_DEF_NONE},
  {"megawatt",  "megawatts",  "MW", NULL, "Megawatts",  NULL, 1e6f,  0.0, B_UNIT_DEF_CASE_SENSITIVE},
  {"kilowatt",  "kilowatts",  "kW", NULL, "Kilowatts",  NULL, 1e3f,  0.0, B_UNIT_DEF_SUPPRESS},
  {"watt",      "watts",      "W",  NULL, "Watts",      NULL, 1.0f,  0.0, B_UNIT_DEF_NONE}, /* base unit */
  {"milliwatt", "milliwatts", "mW", NULL, "Milliwatts", NULL, 1e-3f, 0.0, B_UNIT_DEF_CASE_SENSITIVE},
  {"microwatt", "microwatts", "µW", "uW", "Microwatts", NULL, 1e-6f, 0.0, B_UNIT_DEF_NONE},
  {"nanowatt",  "nanowatts",  "nW", NULL, "Nanowatts",  NULL, 1e-9f, 0.0, B_UNIT_DEF_NONE},
  NULL_UNIT,
};
static struct bUnitCollection buPowerCollection = {buPowerDef, 3, 0, UNIT_COLLECTION_LENGTH(buPowerDef)};


#define UNIT_SYSTEM_TOT (((sizeof(bUnitSystems) / B_UNIT_TYPE_TOT) / sizeof(void *)) - 1)
static const struct bUnitCollection *bUnitSystems[][B_UNIT_TYPE_TOT] = {
  {NULL, NULL, NULL, NULL, NULL, &buNaturalRotCollection, &buNaturalTimeCollection, NULL, NULL, NULL, NULL},
  {NULL, &buMetricLenCollection, &buMetricAreaCollection, &buMetricVolCollection, &buMetricMassCollection, &buNaturalRotCollection, &buNaturalTimeCollection, &buMetricVelCollection, &buMetricAclCollection, &buCameraLenCollection, &buPowerCollection}, /* metric */
  {NULL, &buImperialLenCollection, &buImperialAreaCollection, &buImperialVolCollection, &buImperialMassCollection, &buNaturalRotCollection, &buNaturalTimeCollection, &buImperialVelCollection, &buImperialAclCollection, &buCameraLenCollection, &buPowerCollection}, /* imperial */
  {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
};

/* clang-format on */

/* internal, has some option not exposed */
static const bUnitCollection *unit_get_system(int system, int type)
{
  assert((system > -1) && (system < UNIT_SYSTEM_TOT) && (type > -1) && (type < B_UNIT_TYPE_TOT));
  return bUnitSystems[system][type]; /* select system to use, metric/imperial/other? */
}

static const bUnitDef *unit_default(const bUnitCollection *usys)
{
  return &usys->units[usys->base_unit];
}

static const bUnitDef *unit_best_fit(double value,
                                     const bUnitCollection *usys,
                                     const bUnitDef *unit_start,
                                     int suppress)
{
  const bUnitDef *unit;
  double value_abs = value > 0.0 ? value : -value;

  for (unit = unit_start ? unit_start : usys->units; unit->name; unit++) {

    if (suppress && (unit->flag & B_UNIT_DEF_SUPPRESS)) {
      continue;
    }

    /* scale down scalar so 1cm doesn't convert to 10mm because of float error */
    if (UNLIKELY(unit->flag & B_UNIT_DEF_TENTH)) {
      if (value_abs >= unit->scalar * (0.1 - EPS)) {
        return unit;
      }
    }
    else {
      if (value_abs >= unit->scalar * (1.0 - EPS)) {
        return unit;
      }
    }
  }

  return unit_default(usys);
}

/* convert into 2 units and 2 values for "2ft, 3inch" syntax */
static void unit_dual_convert(double value,
                              const bUnitCollection *usys,
                              bUnitDef const **r_unit_a,
                              bUnitDef const **r_unit_b,
                              double *r_value_a,
                              double *r_value_b,
                              const bUnitDef *main_unit)
{
  const bUnitDef *unit;
  if (main_unit) {
    unit = main_unit;
  }
  else {
    unit = unit_best_fit(value, usys, NULL, 1);
  }

  *r_value_a = (value < 0.0 ? ceil : floor)(value / unit->scalar) * unit->scalar;
  *r_value_b = value - (*r_value_a);

  *r_unit_a = unit;
  *r_unit_b = unit_best_fit(*r_value_b, usys, *r_unit_a, 1);
}

static size_t unit_as_string(char *str,
                             int len_max,
                             double value,
                             int prec,
                             const bUnitCollection *usys,
                             /* non exposed options */
                             const bUnitDef *unit,
                             char pad)
{
  double value_conv;
  size_t len, i;

  if (unit) {
    /* use unit without finding the best one */
  }
  else if (value == 0.0) {
    /* use the default units since there is no way to convert */
    unit = unit_default(usys);
  }
  else {
    unit = unit_best_fit(value, usys, NULL, 1);
  }

  value_conv = value / unit->scalar;

  /* Adjust precision to expected number of significant digits.
   * Note that here, we shall not have to worry about very big/small numbers, units are expected to
   * replace 'scientific notation' in those cases. */
  prec -= integer_digits_d(value_conv);
  CLAMP(prec, 0, 6);

  /* Convert to a string */
  len = BLI_snprintf_rlen(str, len_max, "%.*f", prec, value_conv);

  /* Add unit prefix and strip zeros */

  /* replace trailing zero's with spaces
   * so the number is less complicated but alignment in a button wont
   * jump about while dragging */
  i = len - 1;

  if (prec > 0) {
    while (i > 0 && str[i] == '0') { /* 4.300 -> 4.3 */
      str[i--] = pad;
    }

    if (i > 0 && str[i] == '.') { /* 10. -> 10 */
      str[i--] = pad;
    }
  }

  /* Now add a space for all units except foot, inch, degree, arcminute, arcsecond */
  if (!(unit->flag & B_UNIT_DEF_NO_SPACE)) {
    str[++i] = ' ';
  }

  /* Now add the suffix */
  if (i < len_max) {
    int j = 0;
    i++;
    while (unit->name_short[j] && (i < len_max)) {
      str[i++] = unit->name_short[j++];
    }
  }

  /* terminate no matter what's done with padding above */
  if (i >= len_max) {
    i = len_max - 1;
  }

  str[i] = '\0';
  return i;
}

static bool unit_should_be_split(int type)
{
  return ELEM(type, B_UNIT_LENGTH, B_UNIT_MASS, B_UNIT_TIME, B_UNIT_CAMERA);
}

typedef struct {
  int system;
  int rotation;
  /* USER_UNIT_ADAPTIVE means none, otherwise the value is the index in the collection */
  int length;
  int mass;
  int time;
} PreferredUnits;

static PreferredUnits preferred_units_from_UnitSettings(const UnitSettings *settings)
{
  PreferredUnits units = {0};
  units.system = settings->system;
  units.rotation = settings->system_rotation;
  units.length = settings->length_unit;
  units.mass = settings->mass_unit;
  units.time = settings->time_unit;
  return units;
}

static size_t unit_as_string_splitted(char *str,
                                      int len_max,
                                      double value,
                                      int prec,
                                      const bUnitCollection *usys,
                                      const bUnitDef *main_unit)
{
  const bUnitDef *unit_a, *unit_b;
  double value_a, value_b;

  unit_dual_convert(value, usys, &unit_a, &unit_b, &value_a, &value_b, main_unit);

  /* check the 2 is a smaller unit */
  if (unit_b > unit_a) {
    size_t i;
    i = unit_as_string(str, len_max, value_a, prec, usys, unit_a, '\0');

    prec -= integer_digits_d(value_a / unit_b->scalar) -
            integer_digits_d(value_b / unit_b->scalar);
    prec = max_ii(prec, 0);

    /* is there enough space for at least 1 char of the next unit? */
    if (i + 2 < len_max) {
      str[i++] = ' ';

      /* use low precision since this is a smaller unit */
      i += unit_as_string(str + i, len_max - i, value_b, prec, usys, unit_b, '\0');
    }
    return i;
  }

  return -1;
}

static bool is_valid_unit_collection(const bUnitCollection *usys)
{
  return usys != NULL && usys->units[0].name != NULL;
}

static const bUnitDef *get_preferred_display_unit_if_used(int type, PreferredUnits units)
{
  const bUnitCollection *usys = unit_get_system(units.system, type);
  if (!is_valid_unit_collection(usys)) {
    return NULL;
  }

  int max_offset = usys->length - 1;

  switch (type) {
    case B_UNIT_LENGTH:
    case B_UNIT_AREA:
    case B_UNIT_VOLUME:
      if (units.length == USER_UNIT_ADAPTIVE) {
        return NULL;
      }
      return usys->units + MIN2(units.length, max_offset);
    case B_UNIT_MASS:
      if (units.mass == USER_UNIT_ADAPTIVE) {
        return NULL;
      }
      return usys->units + MIN2(units.mass, max_offset);
    case B_UNIT_TIME:
      if (units.time == USER_UNIT_ADAPTIVE) {
        return NULL;
      }
      return usys->units + MIN2(units.time, max_offset);
    case B_UNIT_ROTATION:
      if (units.rotation == 0) {
        return usys->units + 0;
      }
      else if (units.rotation == USER_UNIT_ROT_RADIANS) {
        return usys->units + 3;
      }
      break;
    default:
      break;
  }
  return NULL;
}

/* Return the length of the generated string. */
static size_t unit_as_string_main(char *str,
                                  int len_max,
                                  double value,
                                  int prec,
                                  int type,
                                  bool split,
                                  bool pad,
                                  PreferredUnits units)
{
  const bUnitCollection *usys = unit_get_system(units.system, type);
  const bUnitDef *main_unit = NULL;

  if (!is_valid_unit_collection(usys)) {
    usys = &buDummyCollection;
  }
  else {
    main_unit = get_preferred_display_unit_if_used(type, units);
  }

  if (split && unit_should_be_split(type)) {
    int length = unit_as_string_splitted(str, len_max, value, prec, usys, main_unit);
    /* failed when length is negative, fallback to no split */
    if (length >= 0) {
      return length;
    }
  }

  return unit_as_string(str, len_max, value, prec, usys, main_unit, pad ? ' ' : '\0');
}

size_t bUnit_AsString(
    char *str, int len_max, double value, int prec, int system, int type, bool split, bool pad)
{
  PreferredUnits units;
  units.system = system;
  units.rotation = 0;
  units.length = USER_UNIT_ADAPTIVE;
  units.mass = USER_UNIT_ADAPTIVE;
  units.time = USER_UNIT_ADAPTIVE;
  return unit_as_string_main(str, len_max, value, prec, type, split, pad, units);
}

size_t bUnit_AsString2(char *str,
                       int len_max,
                       double value,
                       int prec,
                       int type,
                       const UnitSettings *settings,
                       bool pad)
{
  bool do_split = (settings->flag & USER_UNIT_OPT_SPLIT) != 0;
  PreferredUnits units = preferred_units_from_UnitSettings(settings);
  return unit_as_string_main(str, len_max, value, prec, type, do_split, pad, units);
}

BLI_INLINE bool isalpha_or_utf8(const int ch)
{
  return (ch >= 128 || isalpha(ch));
}

static const char *unit_find_str(const char *str, const char *substr, bool case_sensitive)
{
  if (substr && substr[0] != '\0') {
    while (true) {
      /* Unit detection is case insensitive. */
      const char *str_found;
      if (case_sensitive) {
        str_found = strstr(str, substr);
      }
      else {
        str_found = BLI_strcasestr(str, substr);
      }

      if (str_found) {
        /* Previous char cannot be a letter. */
        if (str_found == str ||
            /* weak unicode support!, so "µm" won't match up be replaced by "m"
             * since non ascii utf8 values will NEVER return true */
            isalpha_or_utf8(*BLI_str_prev_char_utf8(str_found)) == 0) {
          /* next char cannot be alphanum */
          int len_name = strlen(substr);

          if (!isalpha_or_utf8(*(str_found + len_name))) {
            return str_found;
          }
        }
        /* If str_found is not a valid unit, we have to check further in the string... */
        for (str_found++; isalpha_or_utf8(*str_found); str_found++) {
          /* pass */
        }
        str = str_found;
      }
      else {
        break;
      }
    }
  }
  return NULL;
}

/* Note that numbers are added within brackets
 * ") " - is used to detect numbers we added so we can detect if commas need to be added
 *
 * "1m1cm+2mm"              - Original value
 * "1*1#1*0.01#+2*0.001#"   - Replace numbers
 * "1*1+1*0.01 +2*0.001 "   - Add add signs if ( + - * / | & ~ < > ^ ! = % ) not found in between
 */

/* not too strict, (+ - * /) are most common  */
static bool ch_is_op(char op)
{
  switch (op) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '|':
    case '&':
    case '~':
    case '<':
    case '>':
    case '^':
    case '!':
    case '=':
    case '%':
      return true;
    default:
      return false;
  }
}

static int unit_scale_str(char *str,
                          int len_max,
                          char *str_tmp,
                          double scale_pref,
                          const bUnitDef *unit,
                          const char *replace_str,
                          bool case_sensitive)
{
  char *str_found;

  if ((len_max > 0) && (str_found = (char *)unit_find_str(str, replace_str, case_sensitive))) {
    /* XXX - investigate, does not respect len_max properly  */

    int len, len_num, len_name, len_move, found_ofs;

    found_ofs = (int)(str_found - str);

    len = strlen(str);

    len_name = strlen(replace_str);
    len_move = (len - (found_ofs + len_name)) + 1; /* 1+ to copy the string terminator */
    len_num = BLI_snprintf(
        str_tmp, TEMP_STR_SIZE, "*%.9g" SEP_STR, unit->scalar / scale_pref); /* # removed later */

    if (len_num > len_max) {
      len_num = len_max;
    }

    if (found_ofs + len_num + len_move > len_max) {
      /* can't move the whole string, move just as much as will fit */
      len_move -= (found_ofs + len_num + len_move) - len_max;
    }

    if (len_move > 0) {
      /* resize the last part of the string */
      memmove(
          str_found + len_num, str_found + len_name, len_move); /* may grow or shrink the string */
    }

    if (found_ofs + len_num > len_max) {
      /* not even the number will fit into the string, only copy part of it */
      len_num -= (found_ofs + len_num) - len_max;
    }

    if (len_num > 0) {
      /* its possible none of the number could be copied in */
      memcpy(str_found, str_tmp, len_num); /* without the string terminator */
    }

    /* since the null terminator wont be moved if the stringlen_max
     * was not long enough to fit everything in it */
    str[len_max - 1] = '\0';
    return found_ofs + len_num;
  }
  return 0;
}

static int unit_replace(
    char *str, int len_max, char *str_tmp, double scale_pref, const bUnitDef *unit)
{
  const bool case_sensitive = (unit->flag & B_UNIT_DEF_CASE_SENSITIVE) != 0;
  int ofs = 0;
  ofs += unit_scale_str(
      str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name_short, case_sensitive);
  ofs += unit_scale_str(
      str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name_plural, false);
  ofs += unit_scale_str(
      str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name_alt, case_sensitive);
  ofs += unit_scale_str(str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name, false);
  return ofs;
}

static bool unit_find(const char *str, const bUnitDef *unit)
{
  const bool case_sensitive = (unit->flag & B_UNIT_DEF_CASE_SENSITIVE) != 0;
  if (unit_find_str(str, unit->name_short, case_sensitive)) {
    return true;
  }
  if (unit_find_str(str, unit->name_plural, false)) {
    return true;
  }
  if (unit_find_str(str, unit->name_alt, case_sensitive)) {
    return true;
  }
  if (unit_find_str(str, unit->name, false)) {
    return true;
  }

  return false;
}

static const bUnitDef *unit_detect_from_str(const bUnitCollection *usys,
                                            const char *str,
                                            const char *str_prev)
{
  /* Try to find a default unit from current or previous string.
   * This allows us to handle cases like 2 + 2mm, people would expect to get 4mm, not 2.002m!
   * Note this does not handle corner cases like 2 + 2cm + 1 + 2.5mm... We can't support
   * everything. */
  const bUnitDef *unit = NULL;

  /* see which units the new value has */
  for (unit = usys->units; unit->name; unit++) {
    if (unit_find(str, unit)) {
      break;
    }
  }
  /* Else, try to infer the default unit from the previous string. */
  if (str_prev && (unit == NULL || unit->name == NULL)) {
    /* see which units the original value had */
    for (unit = usys->units; unit->name; unit++) {
      if (unit_find(str_prev, unit)) {
        break;
      }
    }
  }
  /* Else, fall back to default unit. */
  if (unit == NULL || unit->name == NULL) {
    unit = unit_default(usys);
  }

  return unit;
}

bool bUnit_ContainsUnit(const char *str, int type)
{
  for (int system = 0; system < UNIT_SYSTEM_TOT; system++) {
    const bUnitCollection *usys = unit_get_system(system, type);
    if (!is_valid_unit_collection(usys)) {
      continue;
    }

    for (int i = 0; i < usys->length; i++) {
      if (unit_find(str, usys->units + i)) {
        return true;
      }
    }
  }
  return false;
}

double bUnit_PreferredInputUnitScalar(const struct UnitSettings *settings, int type)
{
  PreferredUnits units = preferred_units_from_UnitSettings(settings);
  const bUnitDef *unit = get_preferred_display_unit_if_used(type, units);
  if (unit) {
    return unit->scalar;
  }
  else {
    return bUnit_BaseScalar(units.system, type);
  }
}

/* make a copy of the string that replaces the units with numbers
 * this is used before parsing
 * This is only used when evaluating user input and can afford to be a bit slower
 *
 * This is to be used before python evaluation so..
 * 10.1km -> 10.1*1000.0
 * ...will be resolved by python.
 *
 * values will be split by an add sign
 * 5'2" -> 5*0.3048 + 2*0.0254
 *
 * str_prev is optional, when valid it is used to get a base unit when none is set.
 *
 * return true of a change was made.
 */
bool bUnit_ReplaceString(
    char *str, int len_max, const char *str_prev, double scale_pref, int system, int type)
{
  const bUnitCollection *usys = unit_get_system(system, type);
  if (!is_valid_unit_collection(usys)) {
    return false;
  }

  const bUnitDef *unit = NULL, *default_unit;
  double scale_pref_base = scale_pref;
  char str_tmp[TEMP_STR_SIZE];
  bool changed = false;

  /* Try to find a default unit from current or previous string. */
  default_unit = unit_detect_from_str(usys, str, str_prev);

  /* We apply the default unit to the whole expression (default unit is now the reference '1.0'
   * one). */
  scale_pref_base *= default_unit->scalar;

  /* Apply the default unit on the whole expression, this allows to handle nasty cases like
   * '2+2in'. */
  if (BLI_snprintf(str_tmp, sizeof(str_tmp), "(%s)*%.9g", str, default_unit->scalar) <
      sizeof(str_tmp)) {
    strncpy(str, str_tmp, len_max);
  }
  else {
    /* BLI_snprintf would not fit into str_tmp, cant do much in this case
     * check for this because otherwise bUnit_ReplaceString could call its self forever */
    return changed;
  }

  for (unit = usys->units; unit->name; unit++) {
    /* in case there are multiple instances */
    while (unit_replace(str, len_max, str_tmp, scale_pref_base, unit)) {
      changed = true;
    }
  }
  unit = NULL;

  {
    /* try other unit systems now, so we can evaluate imperial when metric is set for eg. */
    /* Note that checking other systems at that point means we do not support their units as
     * 'default' one. In other words, when in metrics, typing '2+2in' will give 2 meters 2 inches,
     * not 4 inches. I do think this is the desired behavior!
     */
    const bUnitCollection *usys_iter;
    int system_iter;

    for (system_iter = 0; system_iter < UNIT_SYSTEM_TOT; system_iter++) {
      if (system_iter != system) {
        usys_iter = unit_get_system(system_iter, type);
        if (usys_iter) {
          for (unit = usys_iter->units; unit->name; unit++) {
            int ofs = 0;
            /* in case there are multiple instances */
            while (
                (ofs = unit_replace(str + ofs, len_max - ofs, str_tmp, scale_pref_base, unit))) {
              changed = true;
            }
          }
        }
      }
    }
  }
  unit = NULL;

  /* replace # with add sign when there is no operator between it and the next number
   *
   * "1*1# 3*100# * 3"  ->  "1*1+ 3*100  * 3"
   *
   * */
  {
    char *str_found = str;
    const char *ch = str;

    while ((str_found = strchr(str_found, SEP_CHR))) {
      bool op_found = false;

      /* any operators after this? */
      for (ch = str_found + 1; *ch != '\0'; ch++) {
        if (*ch == ' ' || *ch == '\t') {
          continue;
        }
        op_found = (ch_is_op(*ch) || ELEM(*ch, ',', ')'));
        break;
      }

      /* If found an op, comma or closing parenthesis, no need to insert a '+', else we need it. */
      *str_found++ = op_found ? ' ' : '+';
    }
  }

  return changed;
}

/* 45µm --> 45um */
void bUnit_ToUnitAltName(char *str, int len_max, const char *orig_str, int system, int type)
{
  const bUnitCollection *usys = unit_get_system(system, type);

  const bUnitDef *unit;

  /* find and substitute all units */
  for (unit = usys->units; unit->name; unit++) {
    if (len_max > 0 && unit->name_alt) {
      const bool case_sensitive = (unit->flag & B_UNIT_DEF_CASE_SENSITIVE) != 0;
      const char *found = unit_find_str(orig_str, unit->name_short, case_sensitive);
      if (found) {
        int offset = (int)(found - orig_str);
        int len_name = 0;

        /* copy everything before the unit */
        offset = (offset < len_max ? offset : len_max);
        strncpy(str, orig_str, offset);

        str += offset;
        orig_str += offset + strlen(unit->name_short);
        len_max -= offset;

        /* print the alt_name */
        if (unit->name_alt) {
          len_name = BLI_strncpy_rlen(str, unit->name_alt, len_max);
        }
        else {
          len_name = 0;
        }

        len_name = (len_name < len_max ? len_name : len_max);
        str += len_name;
        len_max -= len_name;
      }
    }
  }

  /* finally copy the rest of the string */
  strncpy(str, orig_str, len_max);
}

double bUnit_ClosestScalar(double value, int system, int type)
{
  const bUnitCollection *usys = unit_get_system(system, type);
  const bUnitDef *unit;

  if (usys == NULL) {
    return -1;
  }

  unit = unit_best_fit(value, usys, NULL, 1);
  if (unit == NULL) {
    return -1;
  }

  return unit->scalar;
}

double bUnit_BaseScalar(int system, int type)
{
  const bUnitCollection *usys = unit_get_system(system, type);
  if (usys) {
    return unit_default(usys)->scalar;
  }
  else {
    return 1.0;
  }
}

/* external access */
bool bUnit_IsValid(int system, int type)
{
  return !(system < 0 || system > UNIT_SYSTEM_TOT || type < 0 || type > B_UNIT_TYPE_TOT);
}

void bUnit_GetSystem(int system, int type, void const **r_usys_pt, int *r_len)
{
  const bUnitCollection *usys = unit_get_system(system, type);
  *r_usys_pt = usys;

  if (usys == NULL) {
    *r_len = 0;
    return;
  }

  *r_len = usys->length;
}

int bUnit_GetBaseUnit(const void *usys_pt)
{
  return ((bUnitCollection *)usys_pt)->base_unit;
}

int bUnit_GetBaseUnitOfType(int system, int type)
{
  return unit_get_system(system, type)->base_unit;
}

const char *bUnit_GetName(const void *usys_pt, int index)
{
  return ((bUnitCollection *)usys_pt)->units[index].name;
}
const char *bUnit_GetNameDisplay(const void *usys_pt, int index)
{
  return ((bUnitCollection *)usys_pt)->units[index].name_display;
}
const char *bUnit_GetIdentifier(const void *usys_pt, int index)
{
  const bUnitDef *unit = ((const bUnitCollection *)usys_pt)->units + index;
  if (unit->identifier == NULL) {
    BLI_assert(false && "identifier for this unit is not specified yet");
  }
  return unit->identifier;
}

double bUnit_GetScaler(const void *usys_pt, int index)
{
  return ((bUnitCollection *)usys_pt)->units[index].scalar;
}

bool bUnit_IsSuppressed(const void *usys_pt, int index)
{
  return (((bUnitCollection *)usys_pt)->units[index].flag & B_UNIT_DEF_SUPPRESS) != 0;
}
