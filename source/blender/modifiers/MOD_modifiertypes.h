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
 * Contributor(s): Ben Batt
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file MOD_modifiertypes.h
 *  \ingroup modifiers
 */

#ifndef __MOD_MODIFIERTYPES_H__
#define __MOD_MODIFIERTYPES_H__

#include "BKE_modifier.h"

/* ****************** Type structures for all modifiers ****************** */

extern ModifierTypeInfo modifierType_None;
extern ModifierTypeInfo modifierType_Subsurf;
extern ModifierTypeInfo modifierType_Lattice;
extern ModifierTypeInfo modifierType_Curve;
extern ModifierTypeInfo modifierType_Build;
extern ModifierTypeInfo modifierType_Mirror;
extern ModifierTypeInfo modifierType_Decimate;
extern ModifierTypeInfo modifierType_Wave;
extern ModifierTypeInfo modifierType_Armature;
extern ModifierTypeInfo modifierType_Hook;
extern ModifierTypeInfo modifierType_Softbody;
extern ModifierTypeInfo modifierType_Boolean;
extern ModifierTypeInfo modifierType_Array;
extern ModifierTypeInfo modifierType_EdgeSplit;
extern ModifierTypeInfo modifierType_Displace;
extern ModifierTypeInfo modifierType_UVProject;
extern ModifierTypeInfo modifierType_Smooth;
extern ModifierTypeInfo modifierType_Cast;
extern ModifierTypeInfo modifierType_MeshDeform;
extern ModifierTypeInfo modifierType_ParticleSystem;
extern ModifierTypeInfo modifierType_ParticleInstance;
extern ModifierTypeInfo modifierType_Explode;
extern ModifierTypeInfo modifierType_Cloth;
extern ModifierTypeInfo modifierType_Collision;
extern ModifierTypeInfo modifierType_Bevel;
extern ModifierTypeInfo modifierType_Shrinkwrap;
extern ModifierTypeInfo modifierType_Fluidsim;
extern ModifierTypeInfo modifierType_Mask;
extern ModifierTypeInfo modifierType_SimpleDeform;
extern ModifierTypeInfo modifierType_Multires;
extern ModifierTypeInfo modifierType_Surface;
extern ModifierTypeInfo modifierType_Smoke;
extern ModifierTypeInfo modifierType_ShapeKey;
extern ModifierTypeInfo modifierType_Solidify;
extern ModifierTypeInfo modifierType_Screw;
extern ModifierTypeInfo modifierType_Ocean;
extern ModifierTypeInfo modifierType_Warp;
extern ModifierTypeInfo modifierType_NavMesh;
extern ModifierTypeInfo modifierType_WeightVGEdit;
extern ModifierTypeInfo modifierType_WeightVGMix;
extern ModifierTypeInfo modifierType_WeightVGProximity;
extern ModifierTypeInfo modifierType_DynamicPaint;
extern ModifierTypeInfo modifierType_Remesh;
extern ModifierTypeInfo modifierType_NgonInterp;

/* MOD_util.c */
void modifier_type_init(ModifierTypeInfo *types[]);

#endif //__MOD_MODIFIERTYPES_H__
