/**
 * BKE_shrinkwrap.h
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SHRINKWRAP_H
#define BKE_SHRINKWRAP_H

/* bitset stuff */
//TODO: should move this to other generic lib files?
typedef char* BitSet;
#define bitset_memsize(size)		(sizeof(char)*((size+7)>>3))

#define bitset_new(size,name)		((BitSet)MEM_callocN( bitset_memsize(size) , name))
#define bitset_free(set)			(MEM_freeN((void*)set))

#define bitset_get(set,index)	((set)[(index)>>3] & (1 << ((index)&0x7)))
#define bitset_set(set,index)	((set)[(index)>>3] |= (1 << ((index)&0x7)))
#define bitset_unset(set,index)	((set)[(index)>>3] &= ~(1 << ((index)&0x7)))



struct Object;
struct DerivedMesh;
struct ShrinkwrapModifierData;



typedef struct ShrinkwrapCalcData
{
	ShrinkwrapModifierData *smd;	//shrinkwrap modifier data

	struct Object *ob;				//object we are applying shrinkwrap to
	struct DerivedMesh *original;	//mesh before shrinkwrap (TODO clean this variable.. we don't really need it)
	struct DerivedMesh *final;		//initially a copy of original mesh.. mesh thats going to be shrinkwrapped

	struct DerivedMesh *target;		//mesh we are shrinking to
	
	//matrixs for local<->target space transform
	float local2target[4][4];		
	float target2local[4][4];

	float keptDist;					//Distance to kept from target (units are in local space)
	//float *weights;				//weights of vertexs
	BitSet moved;					//BitSet indicating if vertex has moved

} ShrinkwrapCalcData;

void shrinkwrap_calc_nearest_vertex(ShrinkwrapCalcData *data);
void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *data);
void shrinkwrap_calc_nearest_surface_point(ShrinkwrapCalcData *data);

struct DerivedMesh *shrinkwrapModifier_do(struct ShrinkwrapModifierData *smd, struct Object *ob, struct DerivedMesh *dm, int useRenderParams, int isFinalCalc);

#endif


