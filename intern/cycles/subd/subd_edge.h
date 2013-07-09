/*
 * Original code in the public domain -- castanyo@yahoo.es
 * 
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SUBD_EDGE_H__
#define __SUBD_EDGE_H__

#include "subd_mesh.h"

CCL_NAMESPACE_BEGIN

/* Subd Half Edge */

class SubdEdge
{
public:
	int id;
	
	SubdEdge *next;
	SubdEdge *prev;
	SubdEdge *pair;
	SubdVert *vert;
	SubdFace *face;

	SubdEdge(int id_)
	{
		id = id_;
		next = NULL;
		prev = NULL;
		pair = NULL;
		vert = NULL;
		face = NULL;
	}
	
	/* vertex queries */
	SubdVert *from() { return vert; }
	SubdVert *to() { return next->vert; }

	/* face queries */
	bool is_boundary() { return !(face && pair->face); }
};

CCL_NAMESPACE_END

#endif /* __SUBD_EDGE_H__ */

