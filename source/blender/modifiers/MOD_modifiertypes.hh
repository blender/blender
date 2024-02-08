/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

#include "BKE_modifier.hh"

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
extern ModifierTypeInfo modifierType_Fluid;
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
extern ModifierTypeInfo modifierType_Skin;
extern ModifierTypeInfo modifierType_LaplacianSmooth;
extern ModifierTypeInfo modifierType_Triangulate;
extern ModifierTypeInfo modifierType_UVWarp;
extern ModifierTypeInfo modifierType_MeshCache;
extern ModifierTypeInfo modifierType_LaplacianDeform;
extern ModifierTypeInfo modifierType_Wireframe;
extern ModifierTypeInfo modifierType_Weld;
extern ModifierTypeInfo modifierType_DataTransfer;
extern ModifierTypeInfo modifierType_NormalEdit;
extern ModifierTypeInfo modifierType_CorrectiveSmooth;
extern ModifierTypeInfo modifierType_MeshSequenceCache;
extern ModifierTypeInfo modifierType_SurfaceDeform;
extern ModifierTypeInfo modifierType_WeightedNormal;
extern ModifierTypeInfo modifierType_Nodes;
extern ModifierTypeInfo modifierType_MeshToVolume;
extern ModifierTypeInfo modifierType_VolumeDisplace;
extern ModifierTypeInfo modifierType_VolumeToMesh;
extern ModifierTypeInfo modifierType_GreasePencilOpacity;
extern ModifierTypeInfo modifierType_GreasePencilSubdiv;
extern ModifierTypeInfo modifierType_GreasePencilColor;
extern ModifierTypeInfo modifierType_GreasePencilTint;
extern ModifierTypeInfo modifierType_GreasePencilSmooth;
extern ModifierTypeInfo modifierType_GreasePencilOffset;
extern ModifierTypeInfo modifierType_GreasePencilNoise;
extern ModifierTypeInfo modifierType_GreasePencilMirror;
extern ModifierTypeInfo modifierType_GreasePencilThickness;
extern ModifierTypeInfo modifierType_GreasePencilLattice;

/* MOD_util.cc */

/**
 * Only called by `BKE_modifier.hh/modifier.cc`
 */
void modifier_type_init(ModifierTypeInfo *types[]);
