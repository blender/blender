
// Gawain vertex attribute binding
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "gwn_vertex_format.h"
#include "gwn_shader_interface.h"

void AttribBinding_clear(Gwn_AttrBinding*);

void get_attrib_locations(const Gwn_VertFormat*, Gwn_AttrBinding*, const Gwn_ShaderInterface*);
unsigned read_attrib_location(const Gwn_AttrBinding*, unsigned a_idx);
