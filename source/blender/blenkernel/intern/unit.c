/**
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

/* define a single unit */
typedef struct bUnitDef {
	char *name;
	char *name_plural;	/* can be NULL */
	char *name_short;	/* this is used for display*/
	char *name_alt;		/* can be NULL */
	
	double mul;
	double bias;		/* not used yet, needed for converting temperature */
} bUnitDef;

/* define a single unit */
typedef struct bUnitCollection {
	struct bUnitDef *units;
	int base_unit;				/* use for 0.0, or none given */
	int flag;					/* options for this system */
} bUnitCollection;

/* Dummy */
static struct bUnitDef buDummyDef[] = {
	{"", NULL, "", NULL,	1.0, 0.0},
	{NULL, NULL, NULL,	NULL, 0.0, 0.0}
};
static struct bUnitCollection buDummyCollecton = {buDummyDef, 0, 0};


/* Lengths */
static struct bUnitDef buMetricLenDef[] = {
	{"kilometer", "kilometers",		"km", NULL,	1000.0, 0.0},
	{"meter", "meters",				"m",  NULL,	1.0, 0.0}, /* base unit */
	{"centimeter", "centimeters",	"cm", NULL,	0.01, 0.0},
	{"millimeter", "millimeters",	"mm", NULL,	0.001, 0.0},
	{"micrometer", "micrometers",	"um", "µm",	0.000001, 0.0}, // micron too?
	{"nanometer", "nanometers",		"nm", NULL,	0.000000001, 0.0},
	{"picometer", "picometers",		"pm", NULL,	0.000000000001, 0.0},
	{NULL, NULL, NULL,	NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricLenCollecton = {buMetricLenDef, 1, 0};

static struct bUnitDef buImperialLenDef[] = {
	{"mile", "miles",				"mi", "m",	1609.344, 0.0},
	{"yard", "yards",				"yd", NULL,	0.9144, 0.0},
	{"foot", "feet",				"'", "ft",	0.3048, 0.0},
	{"inch", "inches",				"\"", "in",	0.0254, 0.0}, /* base unit */
	{"thou", "thous",				"mil", NULL,0.0000254, 0.0},
	{NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialLenCollecton = {buImperialLenDef, 2, 0};


/* Time */
static struct bUnitDef buNaturalTimeDef[] = {
	/* weeks? - probably not needed for blender */
	{"day", "days",					"d", NULL,	90000.0, 0.0},
	{"hour", "hours",				"hr", "h",	3600.0, 0.0},
	{"minute", "minutes",			"min", "m",	60.0, 0.0},
	{"second", "seconds",			"sec", "s",	1.0, 0.0}, /* base unit */
	{"millisecond", "milliseconds",	"ms", NULL,	0.001, 0.0},
	{"microsecond", "microseconds",	"us", NULL,	0.000001, 0.0},
	{NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buNaturalTimeCollecton = {buNaturalTimeDef, 3, 0};

static struct bUnitCollection *bUnitSystems[][8] = {
	{0,0,0,0,0,0,0,0},
	{0,&buMetricLenCollecton, 0,0,0,0, &buNaturalTimeCollecton,0}, /* metric */
	{0,&buImperialLenCollecton, 0,0,0,0, &buNaturalTimeCollecton,0}, /* imperial */
	{0,0,0,0,0,0,0,0}
};

/* internal, has some option not exposed */
static bUnitCollection *unit_get_system(int system, int type)
{
	return bUnitSystems[system][type]; /* select system to use, metric/imperial/other? */
}

static bUnitDef *unit_best_fit(double value, bUnitCollection *usys, bUnitDef *unit_start)
{
	bUnitDef *unit;
	double value_abs= value>0.0?value:-value;

	for(unit= unit_start ? unit_start:usys->units; unit->name; unit++)
		if (value_abs >= unit->mul)
			return unit;

	return &usys->units[usys->base_unit];
}

/* convert into 2 units and 2 values for "2ft, 3inch" syntax */
static void unit_dual_convert(double value, bUnitCollection *usys,
		bUnitDef **unit_a, bUnitDef **unit_b, double *value_a, double *value_b)
{
	bUnitDef *unit= unit_best_fit(value, usys, NULL);

	*value_a= floor(value/unit->mul) * unit->mul;
	*value_b= value - (*value_a);

	*unit_a=	unit;
	*unit_b=	unit_best_fit(*value_b, usys, *unit_a);
}

static int unit_as_string(char *str, double value, int prec, bUnitCollection *usys,
		/* non exposed options */
		bUnitDef *unit, char pad)
{
	double value_conv;
	int len, i;
	
	if(unit) {
		/* use unit without finding the best one */
	}
	else if(value == 0.0) {
		/* use the default units since there is no way to convert */
		unit= &usys->units[usys->base_unit];
	}
	else {
		unit= unit_best_fit(value, usys, NULL);
	}

	value_conv= value/unit->mul;

	/* Convert to a string */
	{
		char conv_str[5] = {'%', '.', '0'+prec, 'f', '\0'}; /* "%.2f" when prec is 2, must be under 10 */
		len= sprintf(str, conv_str, (float)value_conv);
	}
	
	
	/* Add unit prefix and strip zeros */
	{
		/* replace trailing zero's with spaces 
		 * so the number is less complicated but allignment in a button wont
		 * jump about while dragging */
		int j;
		i= len-1;

	
		while(i>0 && str[i]=='0') { /* 4.300 -> 4.3 */
			str[i--]= pad;
		}
		
		if(i>0 && str[i]=='.') { /* 10. -> 10 */
			str[i--]= pad;
		}
		
		/* Now add the suffix */
		i++;
		j=0;
		while(unit->name_short[j]) {
			str[i++]= unit->name_short[j++];
		}

		if(pad) {
			/* this loop only runs if so many zeros were removed that
			 * the unit name only used padded chars,
			 * In that case add padding for the name. */

			while(i<=len+j) {
				str[i++]= pad;
			}
		}
		
		/* terminate no matter whats done with padding above */
		str[i] = '\0';
	}

	return i;
}


/* Used for drawing number buttons, try keep fast */
void bUnit_AsString(char *str, double value, int prec, int system, int type, int split, int pad)
{
	bUnitCollection *usys = unit_get_system(system, type);

	if(usys==NULL || usys->units[0].name==NULL)
		usys= &buDummyCollecton;

	if(split) {
		int i;
		bUnitDef *unit_a, *unit_b;
		double value_a, value_b;

		unit_dual_convert(value, usys,		&unit_a, &unit_b, &value_a, &value_b);

		/* check the 2 is a smaller unit */
		if(unit_b > unit_a) {
			i= unit_as_string(str, value_a, prec, usys,  unit_a, '\0');
			str[i++]= ',';
			str[i++]= ' ';

			/* use low precision since this is a smaller unit */
			unit_as_string(str+i, value_b, prec?1:0, usys,  unit_b, '\0');
			return;
		}
	}

	unit_as_string(str, value, prec, usys,    NULL, pad?' ':'\0');
}


static int unit_scale_str(char *str, char *str_tmp, double scale_pref, bUnitDef *unit, char *replace_str)
{
	char *str_found;
	int change= 0;

	if(replace_str==NULL || replace_str[0] == '\0')
		return 0;

	if((str_found= strstr(str, replace_str))) {
		/* previous char cannot be a letter */
		if (str_found == str || isalpha(*(str_found-1))==0) {
			int len_name = strlen(replace_str);

			/* next char cannot be alphanum */
			if (!isalpha(*(str_found+len_name))) {
				int len= strlen(str);
				int len_num= sprintf(str_tmp, "*%g", scale_pref*unit->mul);
				memmove(str_found+len_num, str_found+len_name, (len+1)-(int)((str_found+len_name)-str)); /* may grow or shrink the string, 1+ to copy the string terminator */
				memcpy(str_found, str_tmp, len_num); /* without the string terminator */
				change= 1;
			}
		}
	}
	return change;
}

static int unit_replace(char *str, char *str_tmp, double scale_pref, bUnitDef *unit)
{	
	//unit_replace_delimit(str, str_tmp);
	int change= 0;
	change |= unit_scale_str(str, str_tmp, scale_pref, unit, unit->name_short);
	change |= unit_scale_str(str, str_tmp, scale_pref, unit, unit->name_plural);
	change |= unit_scale_str(str, str_tmp, scale_pref, unit, unit->name_alt);
	change |= unit_scale_str(str, str_tmp, scale_pref, unit, unit->name);
	return change;
}

/* make a copy of the string that replaces the units with numbers
 * this is used before parsing
 * This is only used when evaluating user input and can afford to be a bit slower
 * 
 * This is to be used before python evaluation so..
 * 10.1km -> 10.1*1000.0
 * ...will be resolved by python.
 * 
 * return true of a change was made.
 */
int bUnit_ReplaceString(char *str, char *str_orig, double scale_pref, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);
	bUnitDef *unit;
	char str_tmp[256];
	int change= 0;
	
	strcpy(str, str_orig);

	if(usys==NULL || usys->units[0].name==NULL) {
		return 0;
	}

	scale_pref= 1.0/scale_pref;
	
	for(unit= usys->units; unit->name; unit++) {
		/* incase there are multiple instances */
		while(unit_replace(str, str_tmp, scale_pref, unit))
			change= 1;
	}
	// printf("replace %s\n", str);
	return change;
}
