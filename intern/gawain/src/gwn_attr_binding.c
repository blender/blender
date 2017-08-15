
// Gawain vertex attribute binding
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_attr_binding.h"
#include "gwn_attr_binding_private.h"
#include <stddef.h>

#if GWN_VERT_ATTR_MAX_LEN != 16
  #error "attrib binding code assumes GWN_VERT_ATTR_MAX_LEN = 16"
#endif

void AttribBinding_clear(Gwn_AttrBinding* binding)
	{
	binding->loc_bits = 0;
	binding->enabled_bits = 0;
	}

unsigned read_attrib_location(const Gwn_AttrBinding* binding, unsigned a_idx)
	{
#if TRUST_NO_ONE
	assert(a_idx < GWN_VERT_ATTR_MAX_LEN);
	assert(binding->enabled_bits & (1 << a_idx));
#endif

	return (binding->loc_bits >> (4 * a_idx)) & 0xF;
	}

static void write_attrib_location(Gwn_AttrBinding* binding, unsigned a_idx, unsigned location)
	{
#if TRUST_NO_ONE
	assert(a_idx < GWN_VERT_ATTR_MAX_LEN);
	assert(location < GWN_VERT_ATTR_MAX_LEN);
#endif

	const unsigned shift = 4 * a_idx;
	const uint64_t mask = ((uint64_t)0xF) << shift;
	// overwrite this attrib's previous location
	binding->loc_bits = (binding->loc_bits & ~mask) | (location << shift);
	// mark this attrib as enabled
	binding->enabled_bits |= 1 << a_idx;
	}

void get_attrib_locations(const Gwn_VertFormat* format, Gwn_AttrBinding* binding, const Gwn_ShaderInterface* shaderface)
	{
	AttribBinding_clear(binding);

	for (unsigned a_idx = 0; a_idx < format->attrib_ct; ++a_idx)
		{
		const Gwn_VertAttr* a = format->attribs + a_idx;
		for (unsigned n_idx = 0; n_idx < a->name_ct; ++n_idx)
			{
			const Gwn_ShaderInput* input = GWN_shaderinterface_attr(shaderface, a->name[n_idx]);

#if TRUST_NO_ONE
			assert(input != NULL);
			// TODO: make this a recoverable runtime error? indicates mismatch between vertex format and program
#endif

			write_attrib_location(binding, a_idx, input->location);
			}
		}
	}
