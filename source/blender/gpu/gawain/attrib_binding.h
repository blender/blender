
// Gawain vertex attribute binding
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "vertex_format.h"

typedef struct {
	uint64_t loc_bits; // store 4 bits for each of the 16 attribs
	uint16_t enabled_bits; // 1 bit for each attrib
} AttribBinding;

void clear_AttribBinding(AttribBinding*);

unsigned read_attrib_location(const AttribBinding*, unsigned a_idx);
void write_attrib_location(AttribBinding*, unsigned a_idx, unsigned location);

void get_attrib_locations(const VertexFormat*, AttribBinding*, GLuint program);
void bind_attrib_locations(const VertexFormat*, GLuint program);
