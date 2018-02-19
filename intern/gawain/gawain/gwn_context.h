
// Gawain context
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2018 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

// This interface allow Gawain to manage VAOs for mutiple context and threads.

#ifdef __cplusplus
extern "C" {
#endif

#include "gwn_common.h"
#include "gwn_batch.h"
#include "gwn_shader_interface.h"

typedef struct Gwn_Context Gwn_Context;

Gwn_Context* GWN_context_create(void);
void GWN_context_discard(Gwn_Context*);

void GWN_context_active_set(Gwn_Context*);
Gwn_Context* GWN_context_active_get(void);

#ifdef __cplusplus
}
#endif
