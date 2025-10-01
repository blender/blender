/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#pragma once

#include "SEQ_modifier.hh"

namespace blender::seq {

/* ****************** Type structures for all modifiers ****************** */

extern StripModifierTypeInfo seqModifierType_None;
extern StripModifierTypeInfo seqModifierType_BrightContrast;
extern StripModifierTypeInfo seqModifierType_ColorBalance;
extern StripModifierTypeInfo seqModifierType_Compositor;
extern StripModifierTypeInfo seqModifierType_Curves;
extern StripModifierTypeInfo seqModifierType_HueCorrect;
extern StripModifierTypeInfo seqModifierType_Mask;
extern StripModifierTypeInfo seqModifierType_SoundEqualizer;
extern StripModifierTypeInfo seqModifierType_Tonemap;
extern StripModifierTypeInfo seqModifierType_WhiteBalance;

}  // namespace blender::seq
