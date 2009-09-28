#ifndef DNA_FREESTYLE_TYPES_H
#define DNA_FREESTYLE_TYPES_H

#include "DNA_listBase.h"

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

