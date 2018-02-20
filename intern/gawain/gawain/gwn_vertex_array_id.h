
// Gawain buffer IDs
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2018 Mike Erwin, Clément Foucault
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

// Manage GL vertex array IDs in a thread-safe way
// Use these instead of glGenBuffers & its friends
// - alloc must be called from a thread that is bound
//   to the context that will be used for drawing with
//   this vao.
// - free can be called from any thread

#ifdef __cplusplus
extern "C" {
#endif

#include "gwn_common.h"
#include "gwn_context.h"

GLuint GWN_vao_default(void);
GLuint GWN_vao_alloc(void);
void GWN_vao_free(GLuint vao_id, Gwn_Context*);

#ifdef __cplusplus
}
#endif
