/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <OpenImageIO/fmath.h>

#include <OSL/genclosure.h>

#include "osl_closures.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;

/// Debug closure
///
/// This is going to be used for mask AOV's and similar
/// purposes. A tag (string) is always associated with
/// this closure, that "selects: the channel where the
/// weight should be sent.

class DebugClosure : public ClosurePrimitive {
public:
	ustring m_tag;

	DebugClosure () : ClosurePrimitive(Debug) {}

	bool mergeable(const ClosurePrimitive *other) const {
		const DebugClosure *comp = (const DebugClosure *)other;
		return m_tag == comp->m_tag &&
		       ClosurePrimitive::mergeable(other);
	}

	size_t memsize() const { return sizeof(*this); }

	const char *name() const { return "debug"; }

	void print_on(std::ostream &out) const {
		out << name() << " (\"" << m_tag.c_str() << "\")";
	}

};

ClosureParam closure_debug_params[] = {
	CLOSURE_STRING_PARAM(DebugClosure, m_tag),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(DebugClosure)
};

CLOSURE_PREPARE(closure_debug_prepare, DebugClosure)

CCL_NAMESPACE_END

