/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

#include "BKE_gpencil_modifier_legacy.h"

/* ****************** Type structures for all modifiers ****************** */

extern GpencilModifierTypeInfo modifierType_Gpencil_None;
extern GpencilModifierTypeInfo modifierType_Gpencil_Noise;
extern GpencilModifierTypeInfo modifierType_Gpencil_Subdiv;
extern GpencilModifierTypeInfo modifierType_Gpencil_Simplify;
extern GpencilModifierTypeInfo modifierType_Gpencil_Thick;
extern GpencilModifierTypeInfo modifierType_Gpencil_Tint;
extern GpencilModifierTypeInfo modifierType_Gpencil_Color;
extern GpencilModifierTypeInfo modifierType_Gpencil_Array;
extern GpencilModifierTypeInfo modifierType_Gpencil_Build;
extern GpencilModifierTypeInfo modifierType_Gpencil_Opacity;
extern GpencilModifierTypeInfo modifierType_Gpencil_Outline;
extern GpencilModifierTypeInfo modifierType_Gpencil_Lattice;
extern GpencilModifierTypeInfo modifierType_Gpencil_Length;
extern GpencilModifierTypeInfo modifierType_Gpencil_Mirror;
extern GpencilModifierTypeInfo modifierType_Gpencil_Smooth;
extern GpencilModifierTypeInfo modifierType_Gpencil_Hook;
extern GpencilModifierTypeInfo modifierType_Gpencil_Offset;
extern GpencilModifierTypeInfo modifierType_Gpencil_Armature;
extern GpencilModifierTypeInfo modifierType_Gpencil_Time;
extern GpencilModifierTypeInfo modifierType_Gpencil_Multiply;
extern GpencilModifierTypeInfo modifierType_Gpencil_Texture;
extern GpencilModifierTypeInfo modifierType_Gpencil_WeightProximity;
extern GpencilModifierTypeInfo modifierType_Gpencil_WeightAngle;
extern GpencilModifierTypeInfo modifierType_Gpencil_Lineart;
extern GpencilModifierTypeInfo modifierType_Gpencil_Dash;
extern GpencilModifierTypeInfo modifierType_Gpencil_Shrinkwrap;
extern GpencilModifierTypeInfo modifierType_Gpencil_Envelope;

/* MOD_gpencil_legacy_util.c */

void gpencil_modifier_type_init(GpencilModifierTypeInfo *types[]);
