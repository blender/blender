
// Gawain vertex format (private interface for use inside Gawain)
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016-2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

void VertexFormat_pack(Gwn_VertFormat*);
unsigned padding(unsigned offset, unsigned alignment);
unsigned vertex_buffer_size(const Gwn_VertFormat*, unsigned vertex_ct);
