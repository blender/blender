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

typedef struct FreestyleLineStyle {
	ID id;

} FreestyleLineStyle;

typedef struct FreestyleLineSet {
	struct FreestyleLineSet *next, *prev;

	char name[32]; /* line set name */
	int flags;
	int pad;

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

