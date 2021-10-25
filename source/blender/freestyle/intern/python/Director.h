/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/Director.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_DIRECTOR_H__
#define __FREESTYLE_PYTHON_DIRECTOR_H__

namespace Freestyle {
class UnaryPredicate0D;
class UnaryPredicate1D;
class BinaryPredicate0D;
class BinaryPredicate1D;
class ChainingIterator;
class AdjacencyIterator;
class Interface0D;
class Interface1D;
class Interface0DIterator;
class Stroke;
class StrokeShader;
}

using namespace Freestyle;

// BinaryPredicate0D: __call__
int Director_BPy_BinaryPredicate0D___call__(BinaryPredicate0D *bp0D, Interface0D& i1, Interface0D& i2);

// BinaryPredicate1D: __call__
int Director_BPy_BinaryPredicate1D___call__(BinaryPredicate1D *bp1D, Interface1D& i1, Interface1D& i2);

// UnaryFunction{0D,1D}: __call__
int Director_BPy_UnaryFunction0D___call__(void *uf0D, void *py_uf0D, Interface0DIterator& if0D_it);
int Director_BPy_UnaryFunction1D___call__(void *uf1D, void *py_uf1D, Interface1D& if1D);

// UnaryPredicate0D: __call__
int Director_BPy_UnaryPredicate0D___call__(UnaryPredicate0D *up0D, Interface0DIterator& if0D_it);
	
// UnaryPredicate1D: __call__
int Director_BPy_UnaryPredicate1D___call__(UnaryPredicate1D *up1D, Interface1D& if1D);

// StrokeShader: shade
int Director_BPy_StrokeShader_shade(StrokeShader *ss, Stroke& s);

// ChainingIterator: init, traverse
int Director_BPy_ChainingIterator_init(ChainingIterator *c_it);
int Director_BPy_ChainingIterator_traverse(ChainingIterator *c_it, AdjacencyIterator& a_it);

#endif // __FREESTYLE_PYTHON_DIRECTOR_H__
