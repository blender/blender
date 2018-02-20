
// Gawain buffer IDs
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

// Manage GL buffer IDs in a thread-safe way
// Use these instead of glGenBuffers & its friends
// - alloc must be called from main thread
// - free can be called from any thread

#ifdef __cplusplus
extern "C" {
#endif

#include "gwn_common.h"

GLuint GWN_buf_id_alloc(void);
void GWN_buf_id_free(GLuint buffer_id);

#ifdef __cplusplus
}
#endif
