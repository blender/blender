#ifndef DNA_FREESTYLE_TYPES_H
#define DNA_FREESTYLE_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

/* FreestyleConfig::flags */
#define FREESTYLE_SUGGESTIVE_CONTOURS_FLAG  1
#define FREESTYLE_RIDGES_AND_VALLEYS_FLAG   2
#define FREESTYLE_MATERIAL_BOUNDARIES_FLAG  4

/* FreestyleConfig::mode */
#define FREESTYLE_CONTROL_SCRIPT_MODE  1
#define FREESTYLE_CONTROL_EDITOR_MODE  2

/* FreestyleLineSet::flags */
#define FREESTYLE_LINESET_CURRENT  1
#define FREESTYLE_LINESET_ENABLED  2
#define FREESTYLE_LINESET_SEL_NOT  4
#define FREESTYLE_LINESET_SEL_OR   8

/* FreestyleLineSet::selection */
#define FREESTYLE_SEL_SILHOUETTE          1
#define FREESTYLE_SEL_BORDER              2
#define FREESTYLE_SEL_CREASE              4
#define FREESTYLE_SEL_RIDGE               8
#define FREESTYLE_SEL_VALLEY              16
#define FREESTYLE_SEL_SUGGESTIVE_CONTOUR  32
#define FREESTYLE_SEL_MATERIAL_BOUNDARY   64
#define FREESTYLE_SEL_CONTOUR             128
#define FREESTYLE_SEL_EXTERNAL_CONTOUR    256
#define FREESTYLE_SEL_VISIBILITY          512

/* FreestyleLineSet::qi */
#define FREESTYLE_QI_VISIBLE  1
#define FREESTYLE_QI_HIDDEN   2
#define FREESTYLE_QI_RANGE    3

typedef struct FreestyleLineStyle {
	ID id;

} FreestyleLineStyle;

typedef struct FreestyleLineSet {
	struct FreestyleLineSet *next, *prev;

	char name[32]; /* line set name */
	int flags;
	int selection; /* feature edge selection */

	float crease_angle;
	float sphere_radius;
	float dkr_epsilon;
	short qi; /* quantitative invisibility */
	short pad;
	int qi_start, qi_end;

	FreestyleLineStyle *linestyle; /* line style */

	ListBase objects; /* target objects on which stylized lines are drawn */

} FreestyleLineSet;

typedef struct FreestyleModuleConfig {
	struct FreestyleModuleConfig *next, *prev;
	
	char module_path[256];
	short is_displayed;
	short pad[3];
	
} FreestyleModuleConfig;

typedef struct FreestyleConfig {
	ListBase modules;
	
	int mode; /* scripting, editor */
	int flags; /* suggestive contours, ridges/valleys, material boundaries */
	float sphere_radius;
	float dkr_epsilon;
	float crease_angle;
	int pad;
	
	ListBase linesets;

} FreestyleConfig;

#endif

