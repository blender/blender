
// Gawain context
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2018 Mike Erwin, Clément Foucault
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "gwn_batch.h"
#include "gwn_context.h"
#include "gwn_shader_interface.h"

void gwn_batch_remove_interface_ref(Gwn_Batch*, const Gwn_ShaderInterface*);
void gwn_batch_vao_cache_clear(Gwn_Batch*);

void gwn_context_add_batch(Gwn_Context*, Gwn_Batch*);
void gwn_context_remove_batch(Gwn_Context*, Gwn_Batch*);

#ifdef __cplusplus
}
#endif
