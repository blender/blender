
// Gawain vertex attribute binding
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "attrib_binding.h"

void clear_AttribBinding(AttribBinding* binding)
	{
	binding->loc_bits = 0;
	binding->enabled_bits = 0;
	}

unsigned read_attrib_location(const AttribBinding* binding, unsigned a_idx)
	{
#if TRUST_NO_ONE
	assert(MAX_VERTEX_ATTRIBS == 16);
	assert(a_idx < MAX_VERTEX_ATTRIBS);
	assert(binding->enabled_bits & (1 << a_idx));
#endif

	return (binding->loc_bits >> (4 * a_idx)) & 0xF;
	}

void write_attrib_location(AttribBinding* binding, unsigned a_idx, unsigned location)
	{
#if TRUST_NO_ONE
	assert(MAX_VERTEX_ATTRIBS == 16);
	assert(a_idx < MAX_VERTEX_ATTRIBS);
	assert(location < MAX_VERTEX_ATTRIBS);
#endif

	const unsigned shift = 4 * a_idx;
	const uint64_t mask = ((uint64_t)0xF) << shift;
	// overwrite this attrib's previous location
	binding->loc_bits = (binding->loc_bits & ~mask) | (location << shift);
	// mark this attrib as enabled
	binding->enabled_bits |= 1 << a_idx;
	}

void get_attrib_locations(const VertexFormat* format, AttribBinding* binding, GLuint program)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	clear_AttribBinding(binding);

	for (unsigned a_idx = 0; a_idx < format->attrib_ct; ++a_idx)
		{
		const Attrib* a = format->attribs + a_idx;
		GLint loc = glGetAttribLocation(program, a->name);

#if TRUST_NO_ONE
		assert(loc != -1);
#endif

		write_attrib_location(binding, a_idx, loc);
		}
	}
