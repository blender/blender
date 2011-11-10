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

#ifndef __SUBD_FACE_H__
#define __SUBD_FACE_H__

#include "subd_edge.h"
#include "subd_mesh.h"

CCL_NAMESPACE_BEGIN

/* Subd Face */

class SubdFace
{
public:
	int id;
	SubdEdge *edge;

	SubdFace(int id_)
	{
		id = id_;
		edge = NULL;
	}

	bool contains(SubdEdge *e)
	{
		for(EdgeIterator it(edges()); !it.isDone(); it.advance())
			if(it.current() == e)
				return true;

		return false;
	}

	int num_edges()
	{
		int num = 0;

		for(EdgeIterator it(edges()); !it.isDone(); it.advance())
			num++;

		return num;
	}

	bool is_boundary()
	{
		for(EdgeIterator it(edges()); !it.isDone(); it.advance()) {
			SubdEdge *edge = it.current();

			if(edge->pair->face == NULL)
				return true;
		}

		return false;
	}
	
	/* iterate over edges in clockwise order */
	class EdgeIterator
	{
	public:
		EdgeIterator(SubdEdge *e) : end(NULL), cur(e) { }

		virtual void advance()
		{
			if (end == NULL) end = cur;
			cur = cur->next;
		}

		virtual bool isDone() { return end == cur; }
		virtual SubdEdge *current() { return cur; }

	private:
		SubdEdge *end;
		SubdEdge *cur;
	};

	EdgeIterator edges() { return EdgeIterator(edge); }
	EdgeIterator edges(SubdEdge *edge) {  return EdgeIterator(edge);  }
};

CCL_NAMESPACE_END

#endif /* __SUBD_FACE_H__ */

