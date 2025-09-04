/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLT_translation.hh"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

namespace blender::seq {

StripModifierTypeInfo seqModifierType_None = {
    /*idname*/ "None",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "None"),
    /*struct_name*/ "StripModifierData",
    /*struct_size*/ sizeof(StripModifierData),
    /*init_data*/ nullptr,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ nullptr,
    /*panel_register*/ nullptr,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
