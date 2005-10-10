/**
 *
 * $Id:  
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2004-2005 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_OBJECT_FLUIDSIM_H
#define DNA_OBJECT_FLUIDSIM_H


#ifdef __cplusplus
extern "C" {
#endif
	
struct Mesh;
	
typedef struct FluidsimSettings {
	/* domain,fluid or obstacle */
	short type;
	/* display advanced options in fluid sim tab (on=1,off=0)*/
	short show_advancedoptions;

	/* domain object settings */
	/* resolutions */
	short resolutionxyz;
	short previewresxyz;
	/* size of the domain in real units (meters along largest resolution x,y,z extent) */
	float realsize;
	/* show original meshes, preview or final sim */
	short guiDisplayMode;
	short renderDisplayMode;

	/* fluid properties */
	float viscosityValue;
	short viscosityMode;
	short viscosityExponent;
	/* gravity strength */
	float gravx,gravy,gravz;
	/* anim start end time */
	float animStart, animEnd;
	/* g star param (LBM compressibility) */
	float gstar;
	/* activate refinement? */
	int maxRefine;
	/* store output path, and file prefix for baked fluid surface */
	/* strlens; 80= FILE_MAXFILE, 160= FILE_MAXDIR */
	char surfdataDir[160], surfdataPrefix[80];
	
	/* fluid object type settings */
	/* gravity strength */
	float iniVelx,iniVely,iniVelz;

	/* store pointer to original mesh (for replacing the current one) */
	struct Mesh *orgMesh;
} FluidsimSettings;

/* ob->fluidsimSettings defines */
#define OB_FLUIDSIM_ENABLE			1
#define OB_FLUIDSIM_DOMAIN			2
#define OB_FLUIDSIM_FLUID				4
#define OB_FLUIDSIM_OBSTACLE		8
#define OB_FLUIDSIM_INFLOW      16
#define OB_FLUIDSIM_OUTFLOW     32

#ifdef __cplusplus
}
#endif

#endif


