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

/** \file blender/blenkernel/intern/unit.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "BKE_unit.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#define TEMP_STR_SIZE 256

#define SEP_CHR		'#'
#define SEP_STR		"#"

#define EPS 0.00001

#define UN_SC_KM	1000.0f
#define UN_SC_HM	100.0f
#define UN_SC_DAM	10.0f
#define UN_SC_M		1.0f
#define UN_SC_DM	0.1f
#define UN_SC_CM	0.01f
#define UN_SC_MM	0.001f
#define UN_SC_UM	0.000001f

#define UN_SC_MI	1609.344f
#define UN_SC_FUR	201.168f
#define UN_SC_CH	20.1168f
#define UN_SC_YD	0.9144f
#define UN_SC_FT	0.3048f
#define UN_SC_IN	0.0254f
#define UN_SC_MIL	0.0000254f

#define UN_SC_MTON	1000.0f /* metric ton */
#define UN_SC_QL	100.0f
#define UN_SC_KG	1.0f
#define UN_SC_HG	0.1f
#define UN_SC_DAG	0.01f
#define UN_SC_G		0.001f

#define UN_SC_ITON	907.18474f /* imperial ton */
#define UN_SC_CWT	45.359237f
#define UN_SC_ST	6.35029318f
#define UN_SC_LB	0.45359237f
#define UN_SC_OZ	0.028349523125f

/* define a single unit */
typedef struct bUnitDef {
	const char *name;
	const char *name_plural; /* abused a bit for the display name */
	const char *name_short; /* this is used for display*/
	const char *name_alt; /* keyboard-friendly ASCII-only version of name_short, can be NULL */
	/* if name_short has non-ASCII chars, name_alt should be present */

	const char *name_display; /* can be NULL */

	double scalar;
	double bias; /* not used yet, needed for converting temperature */
	int flag;
} bUnitDef;

#define B_UNIT_DEF_NONE 0
#define B_UNIT_DEF_SUPPRESS 1 /* Use for units that are not used enough to be translated into for common use */

/* define a single unit */
typedef struct bUnitCollection {
	struct bUnitDef *units;
	int base_unit; /* basic unit index (when user doesn't specify unit explicitly) */
	int flag; /* options for this system */
	int length; /* to quickly find the last item */
} bUnitCollection;

/* Dummy */
static struct bUnitDef buDummyDef[] = { {"", NULL, "", NULL, NULL, 1.0, 0.0}, {NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}};
static struct bUnitCollection buDummyCollecton = {buDummyDef, 0, 0, sizeof(buDummyDef)};

/* Lengths */
static struct bUnitDef buMetricLenDef[] = {
	{"kilometer", "kilometers",     "km",  NULL, "Kilometers", UN_SC_KM, 0.0,     B_UNIT_DEF_NONE},
	{"hectometer", "hectometers",   "hm",  NULL, "100 Meters", UN_SC_HM, 0.0,     B_UNIT_DEF_SUPPRESS},
	{"dekameter", "dekameters",     "dam", NULL, "10 Meters",  UN_SC_DAM, 0.0,    B_UNIT_DEF_SUPPRESS},
	{"meter", "meters",             "m",   NULL, "Meters",     UN_SC_M, 0.0,      B_UNIT_DEF_NONE},     /* base unit */
	{"decimeter", "decimeters",     "dm",  NULL, "10 Centimeters", UN_SC_DM, 0.0, B_UNIT_DEF_SUPPRESS},
	{"centimeter", "centimeters",   "cm",  NULL, "Centimeters", UN_SC_CM, 0.0,    B_UNIT_DEF_NONE},
	{"millimeter", "millimeters",   "mm",  NULL, "Millimeters", UN_SC_MM, 0.0,    B_UNIT_DEF_NONE},
	{"micrometer", "micrometers",   "µm",  "um", "Micrometers", UN_SC_UM, 0.0,    B_UNIT_DEF_NONE},     // micron too?

	/* These get displayed because of float precision problems in the transform header,
	 * could work around, but for now probably people wont use these */
#if 0
	{"nanometer", "Nanometers",     "nm", NULL, 0.000000001, 0.0,   B_UNIT_DEF_NONE},
	{"picometer", "Picometers",     "pm", NULL, 0.000000000001, 0.0, B_UNIT_DEF_NONE},
#endif
	{NULL, NULL, NULL,	NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricLenCollecton = {buMetricLenDef, 3, 0, sizeof(buMetricLenDef) / sizeof(bUnitDef)};

static struct bUnitDef buImperialLenDef[] = {
	{"mile", "miles",       "mi", "m", "Miles",      UN_SC_MI, 0.0,  B_UNIT_DEF_NONE},
	{"furlong", "furlongs", "fur", NULL, "Furlongs", UN_SC_FUR, 0.0, B_UNIT_DEF_SUPPRESS},
	{"chain", "chains",     "ch", NULL, "Chains",    UN_SC_CH, 0.0,  B_UNIT_DEF_SUPPRESS},
	{"yard", "yards",       "yd", NULL, "Yards",     UN_SC_YD, 0.0,  B_UNIT_DEF_SUPPRESS},
	{"foot", "feet",        "'", "ft", "Feet",       UN_SC_FT, 0.0,  B_UNIT_DEF_NONE}, /* base unit */
	{"inch", "inches",      "\"", "in", "Inches",    UN_SC_IN, 0.0,  B_UNIT_DEF_NONE},
	{"thou", "thou",        "thou", "mil", "Thou",   UN_SC_MIL, 0.0, B_UNIT_DEF_NONE}, /* plural for thou has no 's' */
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialLenCollecton = {buImperialLenDef, 4, 0, sizeof(buImperialLenDef) / sizeof(bUnitDef)};

/* Areas */
static struct bUnitDef buMetricAreaDef[] = {
	{"square kilometer",  "square kilometers",  "km²", "km2",   "Square Kilometers", UN_SC_KM * UN_SC_KM, 0.0,    B_UNIT_DEF_NONE},
	{"square hectometer", "square hectometers", "hm²", "hm2",   "Square Hectometers", UN_SC_HM * UN_SC_HM, 0.0,   B_UNIT_DEF_NONE},   /* hectare */
	{"square dekameter",  "square dekameters",  "dam²", "dam2",  "Square Dekameters", UN_SC_DAM * UN_SC_DAM, 0.0, B_UNIT_DEF_SUPPRESS},  /* are */
	{"square meter",      "square meters",      "m²",  "m2",    "Square Meters", UN_SC_M * UN_SC_M, 0.0,          B_UNIT_DEF_NONE},   /* base unit */
	{"square decimeter",  "square decimetees",  "dm²", "dm2",   "Square Decimeters", UN_SC_DM * UN_SC_DM, 0.0,    B_UNIT_DEF_SUPPRESS},
	{"square centimeter", "square centimeters", "cm²", "cm2",   "Square Centimeters", UN_SC_CM * UN_SC_CM, 0.0,   B_UNIT_DEF_NONE},
	{"square millimeter", "square millimeters", "mm²", "mm2",   "Square Millimeters", UN_SC_MM * UN_SC_MM, 0.0,   B_UNIT_DEF_NONE},
	{"square micrometer", "square micrometers", "µm²", "um2",   "Square Micrometers", UN_SC_UM * UN_SC_UM, 0.0,   B_UNIT_DEF_NONE},
	{NULL, NULL, NULL,  NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricAreaCollecton = {buMetricAreaDef, 3, 0, sizeof(buMetricAreaDef) / sizeof(bUnitDef)};

static struct bUnitDef buImperialAreaDef[] = {
	{"square mile", "square miles",       "sq mi", "sq m", "Square Miles", UN_SC_MI * UN_SC_MI, 0.0,      B_UNIT_DEF_NONE},
	{"square furlong", "square furlongs", "sq fur", NULL,  "Square Furlongs", UN_SC_FUR * UN_SC_FUR, 0.0, B_UNIT_DEF_SUPPRESS},
	{"square chain", "square chains",     "sq ch",  NULL,  "Square Chains", UN_SC_CH * UN_SC_CH, 0.0,     B_UNIT_DEF_SUPPRESS},
	{"square yard", "square yards",       "sq yd",  NULL,  "Square Yards", UN_SC_YD * UN_SC_YD, 0.0,      B_UNIT_DEF_NONE},
	{"square foot", "square feet",        "sq ft",  NULL,  "Square Feet", UN_SC_FT * UN_SC_FT, 0.0,       B_UNIT_DEF_NONE}, /* base unit */
	{"square inch", "square inches",      "sq in",  NULL,  "Square Inches", UN_SC_IN * UN_SC_IN, 0.0,     B_UNIT_DEF_NONE},
	{"square thou", "square thous",       "sq mil", NULL,  "Square Thous", UN_SC_MIL * UN_SC_MIL, 0.0,    B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialAreaCollecton = {buImperialAreaDef, 4, 0, sizeof(buImperialAreaDef) / sizeof(bUnitDef)};

/* Volumes */
static struct bUnitDef buMetricVolDef[] = {
	{"cubic kilometer",  "cubic kilometers",  "km³",  "km3",  "Cubic Kilometers", UN_SC_KM * UN_SC_KM * UN_SC_KM, 0.0,    B_UNIT_DEF_NONE},
	{"cubic hectometer", "cubic hectometers", "hm³",  "hm3",  "Cubic Hectometers", UN_SC_HM * UN_SC_HM * UN_SC_HM, 0.0,   B_UNIT_DEF_NONE},
	{"cubic dekameter",  "cubic dekameters",  "dam³", "dam3", "Cubic Dekameters", UN_SC_DAM * UN_SC_DAM * UN_SC_DAM, 0.0, B_UNIT_DEF_SUPPRESS},
	{"cubic meter",      "cubic meters",      "m³",   "m3",   "Cubic Meters", UN_SC_M * UN_SC_M * UN_SC_M, 0.0,           B_UNIT_DEF_NONE}, /* base unit */
	{"cubic decimeter",  "cubic decimeters",  "dm³",  "dm3",  "Cubic Decimeters", UN_SC_DM * UN_SC_DM * UN_SC_DM, 0.0,    B_UNIT_DEF_SUPPRESS},
	{"cubic centimeter", "cubic centimeters", "cm³",  "cm3",  "Cubic Centimeters", UN_SC_CM * UN_SC_CM * UN_SC_CM, 0.0,   B_UNIT_DEF_NONE},
	{"cubic millimeter", "cubic millimeters", "mm³",  "mm3",  "Cubic Millimeters", UN_SC_MM * UN_SC_MM * UN_SC_MM, 0.0,   B_UNIT_DEF_NONE},
	{"cubic micrometer", "cubic micrometers", "µm³",  "um3",  "Cubic Micrometers", UN_SC_UM * UN_SC_UM * UN_SC_UM, 0.0,   B_UNIT_DEF_NONE},
	{NULL, NULL, NULL,  NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricVolCollecton = {buMetricVolDef, 3, 0, sizeof(buMetricVolDef) / sizeof(bUnitDef)};

static struct bUnitDef buImperialVolDef[] = {
	{"cubic mile", "cubic miles",       "cu mi", "cu m", "Cubic Miles", UN_SC_MI * UN_SC_MI * UN_SC_MI, 0.0,     B_UNIT_DEF_NONE},
	{"cubic furlong", "cubic furlongs", "cu fur", NULL,  "Cubic Furlongs", UN_SC_FUR * UN_SC_FUR * UN_SC_FUR, 0.0, B_UNIT_DEF_SUPPRESS},
	{"cubic chain", "cubic chains",     "cu ch", NULL,   "Cubic Chains", UN_SC_CH * UN_SC_CH * UN_SC_CH, 0.0,    B_UNIT_DEF_SUPPRESS},
	{"cubic yard", "cubic yards",       "cu yd", NULL,   "Cubic Yards", UN_SC_YD * UN_SC_YD * UN_SC_YD, 0.0,     B_UNIT_DEF_NONE},
	{"cubic foot", "cubic feet",        "cu ft", NULL,   "Cubic Feet", UN_SC_FT * UN_SC_FT * UN_SC_FT, 0.0,      B_UNIT_DEF_NONE}, /* base unit */
	{"cubic inch", "cubic inches",      "cu in", NULL ,  "Cubic Inches", UN_SC_IN * UN_SC_IN * UN_SC_IN, 0.0,    B_UNIT_DEF_NONE},
	{"cubic thou", "cubic thous",       "cu mil", NULL,  "Cubic Thous", UN_SC_MIL * UN_SC_MIL * UN_SC_MIL, 0.0,  B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialVolCollecton = {buImperialVolDef, 4, 0, sizeof(buImperialVolDef) / sizeof(bUnitDef)};

/* Mass */
static struct bUnitDef buMetricMassDef[] = {
	{"ton", "tonnes",           "ton", "t",  "1000 Kilograms", UN_SC_MTON, 0.0,  B_UNIT_DEF_NONE},
	{"quintal", "quintals",     "ql",  "q",  "100 Kilograms", UN_SC_QL, 0.0,     B_UNIT_DEF_NONE},
	{"kilogram", "kilograms",   "kg",  NULL, "Kilograms", UN_SC_KG, 0.0,         B_UNIT_DEF_NONE}, /* base unit */
	{"hectogram", "hectograms", "hg",  NULL, "Hectograms", UN_SC_HG, 0.0,        B_UNIT_DEF_NONE},
	{"dekagram", "dekagrams",   "dag", NULL, "10 Grams", UN_SC_DAG, 0.0,         B_UNIT_DEF_SUPPRESS},
	{"gram", "grams",           "g",   NULL, "Grams", UN_SC_G, 0.0,              B_UNIT_DEF_NONE},
	{NULL, NULL, NULL,  NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricMassCollecton = {buMetricMassDef, 2, 0, sizeof(buMetricMassDef) / sizeof(bUnitDef)};

static struct bUnitDef buImperialMassDef[] = {
	{"ton", "tonnes",   "ton", "t", "Tonnes", UN_SC_ITON, 0.0,      B_UNIT_DEF_NONE},
	{"centum weight", "centum weights", "cwt", NULL, "Centum weights", UN_SC_CWT, 0.0, B_UNIT_DEF_NONE},
	{"stone", "stones", "st", NULL,     "Stones", UN_SC_ST, 0.0,    B_UNIT_DEF_NONE},
	{"pound", "pounds", "lb", NULL,     "Pounds", UN_SC_LB, 0.0,    B_UNIT_DEF_NONE}, /* base unit */
	{"ounce", "ounces", "oz", NULL,     "Ounces", UN_SC_OZ, 0.0,    B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialMassCollecton = {buImperialMassDef, 3, 0, sizeof(buImperialMassDef) / sizeof(bUnitDef)};

/* Even if user scales the system to a point where km^3 is used, velocity and
 * acceleration aren't scaled: that's why we have so few units for them */

/* Velocity */
static struct bUnitDef buMetricVelDef[] = {
	{"meter per second", "meters per second",       "m/s",  NULL,   "Meters per second", UN_SC_M, 0.0,            B_UNIT_DEF_NONE}, /* base unit */
	{"kilometer per hour", "kilometers per hour",   "km/h", NULL,   "Kilometers per hour", UN_SC_KM / 3600.0f, 0.0, B_UNIT_DEF_SUPPRESS},
	{NULL, NULL, NULL,  NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricVelCollecton = {buMetricVelDef, 0, 0, sizeof(buMetricVelDef) / sizeof(bUnitDef)};

static struct bUnitDef buImperialVelDef[] = {
	{"foot per second", "feet per second",  "ft/s", "fps",  "Feet per second", UN_SC_FT, 0.0,       B_UNIT_DEF_NONE}, /* base unit */
	{"mile per hour", "miles per hour",     "mph", NULL,    "Miles per hour", UN_SC_MI / 3600.0f, 0.0, B_UNIT_DEF_SUPPRESS},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialVelCollecton = {buImperialVelDef, 0, 0, sizeof(buImperialVelDef) / sizeof(bUnitDef)};

/* Acceleration */
static struct bUnitDef buMetricAclDef[] = {
	{"meter per second squared", "meters per second squared", "m/s²", "m/s2", "Meters per second squared", UN_SC_M, 0.0, B_UNIT_DEF_NONE}, /* base unit */
	{NULL, NULL, NULL,  NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricAclCollecton = {buMetricAclDef, 0, 0, sizeof(buMetricAclDef) / sizeof(bUnitDef)};

static struct bUnitDef buImperialAclDef[] = {
	{"foot per second squared", "feet per second squared", "ft/s²", "ft/s2", "Feet per second squared", UN_SC_FT, 0.0, B_UNIT_DEF_NONE}, /* base unit */
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialAclCollecton = {buImperialAclDef, 0, 0, sizeof(buImperialAclDef) / sizeof(bUnitDef)};

/* Time */
static struct bUnitDef buNaturalTimeDef[] = {
	/* weeks? - probably not needed for blender */
	{"day", "days",                 "d", NULL,  "Days",         90000.0, 0.0,   B_UNIT_DEF_NONE},
	{"hour", "hours",               "hr", "h",  "Hours",        3600.0, 0.0,    B_UNIT_DEF_NONE},
	{"minute", "minutes",           "min", "m", "Minutes",      60.0, 0.0,      B_UNIT_DEF_NONE},
	{"second", "seconds",           "sec", "s", "Seconds",      1.0, 0.0,       B_UNIT_DEF_NONE}, /* base unit */
	{"millisecond", "milliseconds", "ms", NULL, "Milliseconds", 0.001, 0.0,     B_UNIT_DEF_NONE},
	{"microsecond", "microseconds", "µs", "us", "Microseconds", 0.000001, 0.0,  B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buNaturalTimeCollecton = {buNaturalTimeDef, 3, 0, sizeof(buNaturalTimeDef) / sizeof(bUnitDef)};


static struct bUnitDef buNaturalRotDef[] = {
	{"degree", "degrees",			"°", NULL, "Degrees",		M_PI/180.0, 0.0,	B_UNIT_DEF_NONE},
//	{"radian", "radians",			"r", NULL, "Radians",		1.0, 0.0,			B_UNIT_DEF_NONE},
//	{"turn", "turns",				"t", NULL, "Turns",			1.0/(M_PI*2.0), 0.0,B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buNaturalRotCollection = {buNaturalRotDef, 0, 0, sizeof(buNaturalRotDef) / sizeof(bUnitDef)};

#define UNIT_SYSTEM_TOT (((sizeof(bUnitSystems) / 9) / sizeof(void *)) - 1)
static struct bUnitCollection *bUnitSystems[][9] = {
	{NULL, NULL, NULL, NULL, NULL, &buNaturalRotCollection, &buNaturalTimeCollecton, NULL, NULL},
	{NULL, &buMetricLenCollecton, &buMetricAreaCollecton, &buMetricVolCollecton, &buMetricMassCollecton, &buNaturalRotCollection, &buNaturalTimeCollecton, &buMetricVelCollecton, &buMetricAclCollecton}, /* metric */
	{NULL, &buImperialLenCollecton, &buImperialAreaCollecton, &buImperialVolCollecton, &buImperialMassCollecton, &buNaturalRotCollection, &buNaturalTimeCollecton, &buImperialVelCollecton, &buImperialAclCollecton}, /* imperial */
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};



/* internal, has some option not exposed */
static bUnitCollection *unit_get_system(int system, int type)
{
	assert((system > -1) && (system < UNIT_SYSTEM_TOT) && (type > -1) && (type < B_UNIT_TYPE_TOT));
	return bUnitSystems[system][type]; /* select system to use, metric/imperial/other? */
}

static bUnitDef *unit_default(bUnitCollection *usys)
{
	return &usys->units[usys->base_unit];
}

static bUnitDef *unit_best_fit(double value, bUnitCollection *usys, bUnitDef *unit_start, int suppress)
{
	bUnitDef *unit;
	double value_abs = value > 0.0 ? value : -value;

	for (unit = unit_start ? unit_start : usys->units; unit->name; unit++) {

		if (suppress && (unit->flag & B_UNIT_DEF_SUPPRESS))
			continue;

		/* scale down scalar so 1cm doesnt convert to 10mm because of float error */
		if (value_abs >= unit->scalar * (1.0 - EPS))
			return unit;
	}

	return unit_default(usys);
}

/* convert into 2 units and 2 values for "2ft, 3inch" syntax */
static void unit_dual_convert(double value, bUnitCollection *usys, bUnitDef **unit_a, bUnitDef **unit_b,
                              double *value_a, double *value_b)
{
	bUnitDef *unit = unit_best_fit(value, usys, NULL, 1);

	*value_a = (value < 0.0 ? ceil : floor)(value / unit->scalar) * unit->scalar;
	*value_b = value - (*value_a);

	*unit_a = unit;
	*unit_b = unit_best_fit(*value_b, usys, *unit_a, 1);
}

static int unit_as_string(char *str, int len_max, double value, int prec, bUnitCollection *usys,
                          /* non exposed options */
                          bUnitDef *unit, char pad)
{
	double value_conv;
	int len, i;

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

	/* Convert to a string */
	{
		len = BLI_snprintf(str, len_max, "%.*f", prec, value_conv);

		if (len >= len_max)
			len = len_max;
	}

	/* Add unit prefix and strip zeros */

	/* replace trailing zero's with spaces
	 * so the number is less complicated but allignment in a button wont
	 * jump about while dragging */
	i = len - 1;

	while (i > 0 && str[i] == '0') { /* 4.300 -> 4.3 */
		str[i--] = pad;
	}

	if (i > 0 && str[i] == '.') { /* 10. -> 10 */
		str[i--] = pad;
	}

	/* Now add the suffix */
	if (i < len_max) {
		int j = 0;
		i++;
		while (unit->name_short[j] && (i < len_max)) {
			str[i++] = unit->name_short[j++];
		}

		if (pad) {
			/* this loop only runs if so many zeros were removed that
			 * the unit name only used padded chars,
			 * In that case add padding for the name. */

			while (i <= len + j && (i < len_max)) {
				str[i++] = pad;
			}
		}
	}

	/* terminate no matter whats done with padding above */
	if (i >= len_max)
		i = len_max - 1;

	str[i] = '\0';
	return i;
}

/* Used for drawing number buttons, try keep fast */
void bUnit_AsString(char *str, int len_max, double value, int prec, int system, int type, int split, int pad)
{
	bUnitCollection *usys = unit_get_system(system, type);

	if (usys == NULL || usys->units[0].name == NULL)
		usys = &buDummyCollecton;

	/* split output makes sense only for length, mass and time */
	if (split && (type == B_UNIT_LENGTH || type == B_UNIT_MASS || type == B_UNIT_TIME)) {
		bUnitDef *unit_a, *unit_b;
		double value_a, value_b;

		unit_dual_convert(value, usys, &unit_a, &unit_b, &value_a, &value_b);

		/* check the 2 is a smaller unit */
		if (unit_b > unit_a) {
			int i = unit_as_string(str, len_max, value_a, prec, usys, unit_a, '\0');

			/* is there enough space for at least 1 char of the next unit? */
			if (i + 2 < len_max) {
				str[i++] = ' ';

				/* use low precision since this is a smaller unit */
				unit_as_string(str + i, len_max - i, value_b, prec ? 1 : 0, usys, unit_b, '\0');
			}
			return;
		}
	}

	unit_as_string(str, len_max, value, prec, usys, NULL, pad ? ' ' : '\0');
}

BLI_INLINE int isalpha_or_utf8(const int ch)
{
	return (ch >= 128 || isalpha(ch));
}

static const char *unit_find_str(const char *str, const char *substr)
{
	const char *str_found;

	if (substr && substr[0] != '\0') {
		str_found = strstr(str, substr);
		if (str_found) {
			/* previous char cannot be a letter */
			if (str_found == str ||
			    /* weak unicode support!, so "µm" won't match up be replaced by "m"
			     * since non ascii utf8 values will NEVER return TRUE */
			    isalpha_or_utf8(*BLI_str_prev_char_utf8(str_found)) == 0)
			{
				/* next char cannot be alphanum */
				int len_name = strlen(substr);

				if (!isalpha_or_utf8(*(str_found + len_name))) {
					return str_found;
				}
			}
		}

	}
	return NULL;

}

/* Note that numbers are added within brackets
 * ") " - is used to detect numbers we added so we can detect if commas need to be added
 *
 * "1m1cm+2mm"				- Original value
 * "1*1#1*0.01#+2*0.001#"	- Replace numbers
 * "1*1,1*0.01 +2*0.001 "	- Add comma's if ( - + * / % ^ < > ) not found in between
 *
 */

/* not too strict, (- = * /) are most common  */
static int ch_is_op(char op)
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
			return 1;
		default:
			return 0;
	}
}

static int unit_scale_str(char *str, int len_max, char *str_tmp, double scale_pref, bUnitDef *unit,
                          const char *replace_str)
{
	char *str_found;

	if ((len_max > 0) && (str_found = (char *)unit_find_str(str, replace_str))) {
		/* XXX - investigate, does not respect len_max properly  */

		int len, len_num, len_name, len_move, found_ofs;

		found_ofs = (int)(str_found - str);

		len = strlen(str);

		len_name = strlen(replace_str);
		len_move = (len - (found_ofs + len_name)) + 1; /* 1+ to copy the string terminator */
		len_num = BLI_snprintf(str_tmp, TEMP_STR_SIZE, "*%g"SEP_STR, unit->scalar / scale_pref); /* # removed later */

		if (len_num > len_max)
			len_num = len_max;

		if (found_ofs + len_num + len_move > len_max) {
			/* can't move the whole string, move just as much as will fit */
			len_move -= (found_ofs + len_num + len_move) - len_max;
		}

		if (len_move > 0) {
			/* resize the last part of the string */
			memmove(str_found + len_num, str_found + len_name, len_move); /* may grow or shrink the string */
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

static int unit_replace(char *str, int len_max, char *str_tmp, double scale_pref, bUnitDef *unit)
{
	int ofs = 0;
	ofs += unit_scale_str(str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name_short);
	ofs += unit_scale_str(str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name_plural);
	ofs += unit_scale_str(str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name_alt);
	ofs += unit_scale_str(str + ofs, len_max - ofs, str_tmp, scale_pref, unit, unit->name);
	return ofs;
}

static int unit_find(const char *str, bUnitDef *unit)
{
	if (unit_find_str(str, unit->name_short))   return 1;
	if (unit_find_str(str, unit->name_plural))  return 1;
	if (unit_find_str(str, unit->name_alt))     return 1;
	if (unit_find_str(str, unit->name))         return 1;

	return 0;
}

/* make a copy of the string that replaces the units with numbers
 * this is used before parsing
 * This is only used when evaluating user input and can afford to be a bit slower
 *
 * This is to be used before python evaluation so..
 * 10.1km -> 10.1*1000.0
 * ...will be resolved by python.
 *
 * values will be split by a comma's
 * 5'2" -> 5'0.0254, 2*0.3048
 *
 * str_prev is optional, when valid it is used to get a base unit when none is set.
 *
 * return true of a change was made.
 */
int bUnit_ReplaceString(char *str, int len_max, const char *str_prev, double scale_pref, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);

	bUnitDef *unit;
	char str_tmp[TEMP_STR_SIZE];
	int change = 0;

	if (usys == NULL || usys->units[0].name == NULL) {
		return 0;
	}

	{ /* make lowercase */
		int i;
		char *ch = str;

		for (i = 0; (i >= len_max || *ch == '\0'); i++, ch++)
			if ((*ch >= 'A') && (*ch <= 'Z'))
				*ch += ('a' - 'A');
	}

	for (unit = usys->units; unit->name; unit++) {
		/* in case there are multiple instances */
		while (unit_replace(str, len_max, str_tmp, scale_pref, unit))
			change = 1;
	}
	unit = NULL;

	{
		/* try other unit systems now, so we can evaluate imperial when metric is set for eg. */
		bUnitCollection *usys_iter;
		int system_iter;

		for (system_iter = 0; system_iter < UNIT_SYSTEM_TOT; system_iter++) {
			if (system_iter != system) {
				usys_iter = unit_get_system(system_iter, type);
				if (usys_iter) {
					for (unit = usys_iter->units; unit->name; unit++) {
						int ofs = 0;
						/* in case there are multiple instances */
						while ((ofs = unit_replace(str + ofs, len_max - ofs, str_tmp, scale_pref, unit)))
							change = 1;
					}
				}
			}
		}
	}
	unit = NULL;

	if (change == 0) {
		/* no units given so infer a unit from the previous string or default */
		if (str_prev) {
			/* see which units the original value had */
			for (unit = usys->units; unit->name; unit++) {
				if (unit_find(str_prev, unit))
					break;
			}
		}

		if (unit == NULL || unit->name == NULL)
			unit = unit_default(usys);

		/* add the unit prefix and re-run, use brackets in case there was an expression given */
		if (BLI_snprintf(str_tmp, sizeof(str_tmp), "(%s)%s", str, unit->name) < sizeof(str_tmp)) {
			strncpy(str, str_tmp, len_max);
			return bUnit_ReplaceString(str, len_max, NULL, scale_pref, system, type);
		}
		else {
			/* BLI_snprintf would not fit into str_tmp, cant do much in this case
			 * check for this because otherwise bUnit_ReplaceString could call its self forever */
			return 0;
		}

	}

	/* replace # with commas when there is no operator between it and the next number
	 *
	 * "1*1# 3*100# * 3"  ->  "1 *1, 3 *100  * 3"
	 *
	 * */
	{
		char *str_found = str;
		char *ch = str;

		while ((str_found = strchr(str_found, SEP_CHR))) {

			int op_found = 0;
			/* any operators after this?*/
			for (ch = str_found + 1; *ch != '\0'; ch++) {

				if (*ch == ' ' || *ch == '\t') {
					/* do nothing */
				}
				else if (ch_is_op(*ch) || *ch == ',') { /* found an op, no need to insert a ',' */
					op_found = 1;
					break;
				}
				else { /* found a non-op character */
					op_found = 0;
					break;
				}
			}

			*str_found++ = op_found ? ' ' : ',';
		}
	}

	return change;
}

/* 45µm --> 45um */
void bUnit_ToUnitAltName(char *str, int len_max, const char *orig_str, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);

	bUnitDef *unit;
	bUnitDef *unit_def = unit_default(usys);

	/* find and substitute all units */
	for (unit = usys->units; unit->name; unit++) {
		if (len_max > 0 && (unit->name_alt || unit == unit_def)) {
			const char *found = unit_find_str(orig_str, unit->name_short);
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
				if (unit->name_alt)
					len_name = BLI_snprintf(str, len_max, "%s", unit->name_alt);
				else
					len_name = 0;

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
	bUnitCollection *usys = unit_get_system(system, type);
	bUnitDef *unit;

	if (usys == NULL)
		return -1;

	unit = unit_best_fit(value, usys, NULL, 1);
	if (unit == NULL)
		return -1;

	return unit->scalar;
}

double bUnit_BaseScalar(int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);
	return unit_default(usys)->scalar;
}

/* external access */
int bUnit_IsValid(int system, int type)
{
	return !(system < 0 || system > UNIT_SYSTEM_TOT || type < 0 || type > B_UNIT_TYPE_TOT);
}

void bUnit_GetSystem(void **usys_pt, int *len, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);
	*usys_pt = usys;

	if (usys == NULL) {
		*len = 0;
		return;
	}

	*len = usys->length;
}

int bUnit_GetBaseUnit(void *usys_pt)
{
	return ((bUnitCollection *)usys_pt)->base_unit;
}

const char *bUnit_GetName(void *usys_pt, int index)
{
	return ((bUnitCollection *)usys_pt)->units[index].name;
}
const char *bUnit_GetNameDisplay(void *usys_pt, int index)
{
	return ((bUnitCollection *)usys_pt)->units[index].name_display;
}

double bUnit_GetScaler(void *usys_pt, int index)
{
	return ((bUnitCollection *)usys_pt)->units[index].scalar;
}
