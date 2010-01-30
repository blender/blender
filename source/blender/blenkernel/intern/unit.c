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

#include "BLI_math.h"
#include "BLI_winstuff.h"

#define TEMP_STR_SIZE 256

#define SEP_CHR		'#'
#define SEP_STR		"#"

#define EUL 0.000001


/* define a single unit */
typedef struct bUnitDef {
	char *name;
	char *name_plural;	/* abused a bit for the display name */
	char *name_short;	/* this is used for display*/
	char *name_alt;		/* can be NULL */
	
	char *name_display;		/* can be NULL */

	double scalar;
	double bias;		/* not used yet, needed for converting temperature */
	int flag;
} bUnitDef;

#define B_UNIT_DEF_NONE 0
#define B_UNIT_DEF_SUPPRESS 1 /* Use for units that are not used enough to be translated into for common use */

/* define a single unit */
typedef struct bUnitCollection {
	struct bUnitDef *units;
	int base_unit;				/* use for 0.0, or none given */
	int flag;					/* options for this system */
	int length;					/* to quickly find the last item */
} bUnitCollection;

/* Dummy */
static struct bUnitDef buDummyDef[] = {
	{"",	NULL, "",	NULL, NULL, 1.0, 0.0},
	{NULL,	NULL, NULL,	NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buDummyCollecton = {buDummyDef, 0, 0, sizeof(buDummyDef)};


/* Lengths */
static struct bUnitDef buMetricLenDef[] = {
	{"kilometer", "kilometers",		"km", NULL,	"Kilometers", 1000.0, 0.0,		B_UNIT_DEF_NONE},
	{"hectometer", "hectometers",	"hm", NULL,	"100 Meters", 100.0, 0.0,			B_UNIT_DEF_SUPPRESS},
	{"dekameter", "dekameters",		"dkm",NULL,	"10 Meters", 10.0, 0.0,			B_UNIT_DEF_SUPPRESS},
	{"meter", "meters",				"m",  NULL,	"Meters", 1.0, 0.0, 			B_UNIT_DEF_NONE}, /* base unit */
	{"decimetre", "decimetres",		"dm", NULL,	"10 Centimeters", 0.1, 0.0,			B_UNIT_DEF_SUPPRESS},
	{"centimeter", "centimeters",	"cm", NULL,	"Centimeters", 0.01, 0.0,			B_UNIT_DEF_NONE},
	{"millimeter", "millimeters",	"mm", NULL,	"Millimeters", 0.001, 0.0,			B_UNIT_DEF_NONE},
	{"micrometer", "micrometers",	"um", "µm",	"Micrometers", 0.000001, 0.0,		B_UNIT_DEF_NONE}, // micron too?

	/* These get displayed because of float precision problems in the transform header,
	 * could work around, but for now probably people wont use these */
	/*
	{"nanometer", "Nanometers",		"nm", NULL,	0.000000001, 0.0,	B_UNIT_DEF_NONE},
	{"picometer", "Picometers",		"pm", NULL,	0.000000000001, 0.0,B_UNIT_DEF_NONE},
	*/
	{NULL, NULL, NULL,	NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buMetricLenCollecton = {buMetricLenDef, 3, 0, sizeof(buMetricLenDef)/sizeof(bUnitDef)};

static struct bUnitDef buImperialLenDef[] = {
	{"mile", "miles",		"mi", "m", "Miles",		1609.344, 0.0,	B_UNIT_DEF_NONE},
	{"furlong", "furlongs",	"fur", NULL, "Furlongs",201.168, 0.0,	B_UNIT_DEF_SUPPRESS},
	{"chain", "chains",		"ch", NULL, "Chains",	0.9144*22.0, 0.0,	B_UNIT_DEF_SUPPRESS},
	{"yard", "yards",		"yd", NULL, "Yards",	0.9144, 0.0,	B_UNIT_DEF_NONE},
	{"foot", "feet",		"'", "ft", "Feet",		0.3048, 0.0,	B_UNIT_DEF_NONE},
	{"inch", "inches",		"\"", "in", "Inches",	0.0254, 0.0,	B_UNIT_DEF_NONE}, /* base unit */
	{"thou", "thous",		"mil", NULL, "Thous",	0.0000254, 0.0,	B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buImperialLenCollecton = {buImperialLenDef, 4, 0, sizeof(buImperialLenDef)/sizeof(bUnitDef)};


/* Time */
static struct bUnitDef buNaturalTimeDef[] = {
	/* weeks? - probably not needed for blender */
	{"day", "days",					"d", NULL,	"Days",			90000.0, 0.0,	B_UNIT_DEF_NONE},
	{"hour", "hours",				"hr", "h",	"Hours",		3600.0, 0.0,	B_UNIT_DEF_NONE},
	{"minute", "minutes",			"min", "m",	"Minutes",		60.0, 0.0,		B_UNIT_DEF_NONE},
	{"second", "seconds",			"sec", "s",	"Seconds",		1.0, 0.0,		B_UNIT_DEF_NONE}, /* base unit */
	{"millisecond", "milliseconds",	"ms", NULL,	"Milliseconds",	0.001, 0.0	,	B_UNIT_DEF_NONE},
	{"microsecond", "microseconds",	"us", NULL,	"Microseconds",	0.000001, 0.0,	B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buNaturalTimeCollecton = {buNaturalTimeDef, 3, 0, sizeof(buNaturalTimeDef)/sizeof(bUnitDef)};


static struct bUnitDef buNaturalRotDef[] = {
	{"degree", "degrees",			"°", NULL, "Degrees",		M_PI/180.f, 0.0,	B_UNIT_DEF_NONE},
	{NULL, NULL, NULL, NULL, NULL, 0.0, 0.0}
};
static struct bUnitCollection buNaturalRotCollection = {buNaturalRotDef, 0, 0, sizeof(buNaturalRotDef)/sizeof(bUnitDef)};

#define UNIT_SYSTEM_MAX 3
static struct bUnitCollection *bUnitSystems[][8] = {
	{0,0,0,0,0,&buNaturalRotCollection,&buNaturalTimeCollecton,0},
	{0,&buMetricLenCollecton, 0,0,0, &buNaturalRotCollection, &buNaturalTimeCollecton,0}, /* metric */
	{0,&buImperialLenCollecton, 0,0,0,&buNaturalRotCollection, &buNaturalTimeCollecton,0}, /* imperial */
	{0,0,0,0,0,0,0,0}
};

/* internal, has some option not exposed */
static bUnitCollection *unit_get_system(int system, int type)
{
	return bUnitSystems[system][type]; /* select system to use, metric/imperial/other? */
}

static bUnitDef *unit_default(bUnitCollection *usys)
{
	return &usys->units[usys->base_unit];
}

static bUnitDef *unit_best_fit(double value, bUnitCollection *usys, bUnitDef *unit_start, int suppress)
{
	bUnitDef *unit;
	double value_abs= value>0.0?value:-value;

	for(unit= unit_start ? unit_start:usys->units; unit->name; unit++) {

		if(suppress && (unit->flag & B_UNIT_DEF_SUPPRESS))
			continue;

		if (value_abs >= unit->scalar*(1.0-EUL)) /* scale down scalar so 1cm doesnt convert to 10mm because of float error */
			return unit;
	}

	return unit_default(usys);
}



/* convert into 2 units and 2 values for "2ft, 3inch" syntax */
static void unit_dual_convert(double value, bUnitCollection *usys,
		bUnitDef **unit_a, bUnitDef **unit_b, double *value_a, double *value_b)
{
	bUnitDef *unit= unit_best_fit(value, usys, NULL, 1);

	*value_a= floor(value/unit->scalar) * unit->scalar;
	*value_b= value - (*value_a);

	*unit_a=	unit;
	*unit_b=	unit_best_fit(*value_b, usys, *unit_a, 1);
}

static int unit_as_string(char *str, int len_max, double value, int prec, bUnitCollection *usys,
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
		unit= unit_default(usys);
	}
	else {
		unit= unit_best_fit(value, usys, NULL, 1);
	}

	value_conv= value/unit->scalar;

	/* Convert to a string */
	{
		char conv_str[6] = {'%', '.', '0'+prec, 'l', 'f', '\0'}; /* "%.2lf" when prec is 2, must be under 10 */
		len= snprintf(str, len_max, conv_str, (float)value_conv);

		if(len >= len_max)
			len= len_max;
	}
	
	/* Add unit prefix and strip zeros */

	/* replace trailing zero's with spaces
	 * so the number is less complicated but allignment in a button wont
	 * jump about while dragging */
	i= len-1;

	while(i>0 && str[i]=='0') { /* 4.300 -> 4.3 */
		str[i--]= pad;
	}

	if(i>0 && str[i]=='.') { /* 10. -> 10 */
		str[i--]= pad;
	}
	
	/* Now add the suffix */
	if(i<len_max) {
		int j=0;
		i++;
		while(unit->name_short[j] && (i < len_max)) {
			str[i++]= unit->name_short[j++];
		}

		if(pad) {
			/* this loop only runs if so many zeros were removed that
			 * the unit name only used padded chars,
			 * In that case add padding for the name. */

			while(i<=len+j && (i < len_max)) {
				str[i++]= pad;
			}
		}
	}

	/* terminate no matter whats done with padding above */
	if(i >= len_max)
		i= len_max-1;

	str[i] = '\0';
	return i;
}


/* Used for drawing number buttons, try keep fast */
void bUnit_AsString(char *str, int len_max, double value, int prec, int system, int type, int split, int pad)
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
			i= unit_as_string(str, len_max, value_a, prec, usys,  unit_a, '\0');

			/* is there enough space for at least 1 char of the next unit? */
			if(i+2 < len_max) {
				str[i++]= ' ';

				/* use low precision since this is a smaller unit */
				unit_as_string(str+i, len_max-i, value_b, prec?1:0, usys,  unit_b, '\0');
			}
			return;
		}
	}

	unit_as_string(str, len_max, value, prec, usys,    NULL, pad?' ':'\0');
}


static char *unit_find_str(char *str, char *substr)
{
	char *str_found;

	if(substr && substr[0] != '\0') {
		str_found= strstr(str, substr);
		if(str_found) {
			/* previous char cannot be a letter */
			if (str_found == str || isalpha(*(str_found-1))==0) {
				/* next char cannot be alphanum */
				int len_name = strlen(substr);

				if (!isalpha(*(str_found+len_name))) {
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
	switch(op) {
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

static int unit_scale_str(char *str, int len_max, char *str_tmp, double scale_pref, bUnitDef *unit, char *replace_str)
{
	char *str_found;

	if((len_max>0) && (str_found= unit_find_str(str, replace_str))) { /* XXX - investigate, does not respect len_max properly  */
		int len, len_num, len_name, len_move, found_ofs;

		found_ofs = (int)(str_found-str);

		len= strlen(str);

		len_name = strlen(replace_str);
		len_move= (len - (found_ofs+len_name)) + 1; /* 1+ to copy the string terminator */
		len_num= snprintf(str_tmp, TEMP_STR_SIZE, "*%lg"SEP_STR, unit->scalar/scale_pref); /* # removed later */

		if(len_num > len_max)
			len_num= len_max;

		if(found_ofs+len_num+len_move > len_max) {
			/* can't move the whole string, move just as much as will fit */
			len_move -= (found_ofs+len_num+len_move) - len_max;
		}

		if(len_move>0) {
			/* resize the last part of the string */
			memmove(str_found+len_num, str_found+len_name, len_move); /* may grow or shrink the string */
		}

		if(found_ofs+len_num > len_max) {
			/* not even the number will fit into the string, only copy part of it */
			len_num -= (found_ofs+len_num) - len_max;
		}

		if(len_num > 0) {
			/* its possible none of the number could be copied in */
			memcpy(str_found, str_tmp, len_num); /* without the string terminator */
		}

		/* since the null terminator wont be moved if the stringlen_max
		 * was not long enough to fit everything in it */
		str[len_max-1]= '\0';
		return found_ofs + len_num;
	}
	return 0;
}

static int unit_replace(char *str, int len_max, char *str_tmp, double scale_pref, bUnitDef *unit)
{	
	int ofs= 0;
	ofs += unit_scale_str(str+ofs, len_max-ofs, str_tmp, scale_pref, unit, unit->name_short);
	ofs += unit_scale_str(str+ofs, len_max-ofs, str_tmp, scale_pref, unit, unit->name_plural);
	ofs += unit_scale_str(str+ofs, len_max-ofs, str_tmp, scale_pref, unit, unit->name_alt);
	ofs += unit_scale_str(str+ofs, len_max-ofs, str_tmp, scale_pref, unit, unit->name);
	return ofs;
}

static int unit_find(char *str, bUnitDef *unit)
{
	if (unit_find_str(str, unit->name_short))	return 1;
	if (unit_find_str(str, unit->name_plural))	return 1;
	if (unit_find_str(str, unit->name_alt))		return 1;
	if (unit_find_str(str, unit->name))			return 1;

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
int bUnit_ReplaceString(char *str, int len_max, char *str_prev, double scale_pref, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);

	bUnitDef *unit;
	char str_tmp[TEMP_STR_SIZE];
	int change= 0;

	if(usys==NULL || usys->units[0].name==NULL) {
		return 0;
	}
	

	{	/* make lowercase */
		int i;
		char *ch= str;

		for(i=0; (i>=len_max || *ch=='\0'); i++, ch++)
			if((*ch>='A') && (*ch<='Z'))
				*ch += ('a'-'A');
	}


	for(unit= usys->units; unit->name; unit++) {

		if(unit->flag & B_UNIT_DEF_SUPPRESS)
			continue;

		/* incase there are multiple instances */
		while(unit_replace(str, len_max, str_tmp, scale_pref, unit))
			change= 1;
	}
	unit= NULL;

	{
		/* try other unit systems now, so we can evaluate imperial when metric is set for eg. */
		bUnitCollection *usys_iter;
		int system_iter;

		for(system_iter= 0; system_iter<UNIT_SYSTEM_MAX; system_iter++) {
			if (system_iter != system) {
				usys_iter= unit_get_system(system_iter, type);
				if (usys_iter) {
					for(unit= usys_iter->units; unit->name; unit++) {

						if((unit->flag & B_UNIT_DEF_SUPPRESS) == 0) {
							int ofs = 0;
							/* incase there are multiple instances */
							while((ofs=unit_replace(str+ofs, len_max-ofs, str_tmp, scale_pref, unit)))
								change= 1;
						}
					}
				}
			}
		}
	}
	unit= NULL;
	
	if(change==0) {
		/* no units given so infer a unit from the previous string or default */
		if(str_prev) {
			/* see which units the original value had */
			for(unit= usys->units; unit->name; unit++) {

				if(unit->flag & B_UNIT_DEF_SUPPRESS)
					continue;

				if (unit_find(str_prev, unit))
					break;
			}
		}

		if(unit==NULL || unit->name == NULL)
			unit= unit_default(usys);


		/* add the unit prefix and re-run, use brackets incase there was an expression given */
		if(snprintf(str_tmp, sizeof(str_tmp), "(%s)%s", str, unit->name) < sizeof(str_tmp)) {
			strncpy(str, str_tmp, len_max);
			return bUnit_ReplaceString(str, len_max, NULL, scale_pref, system, type);
		}
		else {
			/* snprintf would not fit into str_tmp, cant do much in this case
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
		char *str_found= str;
		char *ch= str;

		while((str_found= strchr(str_found, SEP_CHR))) {

			int op_found= 0;
			/* any operators after this?*/
			for(ch= str_found+1; *ch!='\0'; ch++) {

				if(*ch==' ' || *ch=='\t') {
					/* do nothing */
				}
				else if (ch_is_op(*ch) || *ch==',') { /* found an op, no need to insert a ,*/
					op_found= 1;
					break;
				}
				else { /* found a non-op character */
					op_found= 0;
					break;
				}
			}

			*str_found++ = op_found ? ' ':',';
		}
	}

	return change;
}


double bUnit_ClosestScalar(double value, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);
	bUnitDef *unit;

	if(usys==NULL)
		return -1;

	unit= unit_best_fit(value, usys, NULL, 1);
	if(unit==NULL)
		return -1;

	return unit->scalar;
}

double bUnit_BaseScalar(int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);
	return unit_default(usys)->scalar;
}

/* external access */
void bUnit_GetSystem(void **usys_pt, int *len, int system, int type)
{
	bUnitCollection *usys = unit_get_system(system, type);
	*usys_pt= usys;

	if(usys==NULL) {
		*len= 0;
		return;
	}

	*len= usys->length;
}

char *bUnit_GetName(void *usys_pt, int index)
{
	return ((bUnitCollection *)usys_pt)->units[index].name;
}
char *bUnit_GetNameDisplay(void *usys_pt, int index)
{
	return ((bUnitCollection *)usys_pt)->units[index].name_display;
}

double bUnit_GetScaler(void *usys_pt, int index)
{
	return ((bUnitCollection *)usys_pt)->units[index].scalar;
}
