#ifndef DNA_FREESTYLE_TYPES_H
#define DNA_FREESTYLE_TYPES_H

#include "DNA_listBase.h"

#define FREESTYLE_SUGGESTIVE_CONTOURS_FLAG  1
#define FREESTYLE_RIDGES_AND_VALLEYS_FLAG   2
#define FREESTYLE_MATERIAL_BOUNDARIES_FLAG  4

typedef struct FreestyleModuleConfig {
	struct FreestyleModuleConfig *next, *prev;
	
	char module_path[256];
	short is_displayed;
	short pad[3];
	
} FreestyleModuleConfig;

typedef struct FreestyleConfig {
	ListBase modules;
	
	int flags;
	float sphere_radius;
	float dkr_epsilon;
	int pad;
	
} FreestyleConfig;



#endif

