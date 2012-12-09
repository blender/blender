/* DNA_freestyle_types.h
 *
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_FREESTYLE_TYPES_H
#define DNA_FREESTYLE_TYPES_H

#include "DNA_listBase.h"

struct FreestyleLineStyle;

/* FreestyleConfig::flags */
#define FREESTYLE_SUGGESTIVE_CONTOURS_FLAG  1
#define FREESTYLE_RIDGES_AND_VALLEYS_FLAG   2
#define FREESTYLE_MATERIAL_BOUNDARIES_FLAG  4
#define FREESTYLE_FACE_SMOOTHNESS_FLAG      8
#define FREESTYLE_ADVANCED_OPTIONS_FLAG     16
#define FREESTYLE_CULLING                   32

/* FreestyleConfig::mode */
#define FREESTYLE_CONTROL_SCRIPT_MODE  1
#define FREESTYLE_CONTROL_EDITOR_MODE  2

/* FreestyleLineSet::flags */
#define FREESTYLE_LINESET_CURRENT  1l
#define FREESTYLE_LINESET_ENABLED  2
#define FREESTYLE_LINESET_FE_NOT   4
#define FREESTYLE_LINESET_FE_AND   8
#define FREESTYLE_LINESET_GR_NOT   16
#define FREESTYLE_LINESET_FM_NOT   32
#define FREESTYLE_LINESET_FM_BOTH  64

/* FreestyleLineSet::selection */
#define FREESTYLE_SEL_VISIBILITY    1
#define FREESTYLE_SEL_EDGE_TYPES    2
#define FREESTYLE_SEL_GROUP         4
#define FREESTYLE_SEL_IMAGE_BORDER  8
#define FREESTYLE_SEL_FACE_MARK     16

/* FreestyleLineSet::edge_types, exclude_edge_types */
#define FREESTYLE_FE_SILHOUETTE          1
#define FREESTYLE_FE_BORDER              2
#define FREESTYLE_FE_CREASE              4
#define FREESTYLE_FE_RIDGE_VALLEY        8
/* Note: FREESTYLE_FE_VALLEY = 16 is no longer used */
#define FREESTYLE_FE_SUGGESTIVE_CONTOUR  32
#define FREESTYLE_FE_MATERIAL_BOUNDARY   64
#define FREESTYLE_FE_CONTOUR             128
#define FREESTYLE_FE_EXTERNAL_CONTOUR    512
#define FREESTYLE_FE_EDGE_MARK           1024

/* FreestyleLineSet::qi */
#define FREESTYLE_QI_VISIBLE  1
#define FREESTYLE_QI_HIDDEN   2
#define FREESTYLE_QI_RANGE    3

/* FreestyleConfig::raycasting_algorithm */
// Defines should be replaced with ViewMapBuilder::visibility_algo
#define FREESTYLE_ALGO_REGULAR					1
#define FREESTYLE_ALGO_FAST					2
#define FREESTYLE_ALGO_VERYFAST					3
#define FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL		4
#define FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL			5
#define FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE		6
#define FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE			7

typedef struct FreestyleLineSet {
	struct FreestyleLineSet *next, *prev;

	char name[32]; /* line set name */
	int flags;

	int selection; /* selection criteria */
	short qi; /* quantitative invisibility */
	short pad1;
	int qi_start, qi_end;
	int edge_types, exclude_edge_types; /* feature edge types */
	int pad2;
	struct Group *group; /* group of target objects */

	struct FreestyleLineStyle *linestyle;

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
	int raycasting_algorithm; /* XXX deprecated */
	int flags; /* suggestive contours, ridges/valleys, material boundaries */
	float sphere_radius;
	float dkr_epsilon;
	float crease_angle; /* in radians! */

	ListBase linesets;

} FreestyleConfig;

#endif
