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

/** \file source/blender/freestyle/intern/python/BPy_Convert.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_CONVERT_H__
#define __FREESTYLE_PYTHON_CONVERT_H__

extern "C" {
#include <Python.h>
}

#include <typeinfo>

#include "../geometry/Geom.h"

using namespace Freestyle;
using namespace Freestyle::Geometry;

// BBox
#include "../geometry/BBox.h"

// FEdge, FEdgeSharp, FEdgeSmooth, SShape, SVertex, FEdgeInternal::SVertexIterator
#include "../view_map/Silhouette.h" 

// Id
#include "../system/Id.h"

// Interface0D, Interface0DIteratorNested, Interface0DIterator
#include "../view_map/Interface0D.h"

// Interface1D
#include "../view_map/Interface1D.h"

// FrsMaterial
#include "../scene_graph/FrsMaterial.h"

// Nature::VertexNature, Nature::EdgeNature
#include "../winged_edge/Nature.h"

// Stroke, StrokeAttribute, StrokeVertex
#include "../stroke/Stroke.h"

// NonTVertex, TVertex, ViewEdge, ViewMap, ViewShape, ViewVertex
#include "../view_map/ViewMap.h"

// CurvePoint, Curve
#include "../stroke/Curve.h"

// Chain
#include "../stroke/Chain.h"

//====== ITERATORS

// AdjacencyIterator, ChainingIterator, ChainSilhouetteIterator, ChainPredicateIterator
#include "../stroke/ChainingIterators.h"

// ViewVertexInternal::orientedViewEdgeIterator
// ViewEdgeInternal::SVertexIterator
// ViewEdgeInternal::ViewEdgeIterator
#include "../view_map/ViewMapIterators.h"

// StrokeInternal::StrokeVertexIterator
#include "../stroke/StrokeIterators.h"

// CurveInternal::CurvePointIterator
#include "../stroke/CurveIterators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include "mathutils/mathutils.h"
#include "generic/python_utildefines.h"

//==============================
// C++ => Python
//==============================

PyObject * PyBool_from_bool(bool b);
PyObject * Vector_from_Vec2f(Vec2f& v);
PyObject * Vector_from_Vec3f(Vec3f& v);
PyObject * Vector_from_Vec3r(Vec3r& v);

PyObject * Any_BPy_Interface0D_from_Interface0D(Interface0D& if0D);
PyObject * Any_BPy_Interface1D_from_Interface1D(Interface1D& if1D);
PyObject * Any_BPy_FEdge_from_FEdge(FEdge& fe);
PyObject * Any_BPy_ViewVertex_from_ViewVertex(ViewVertex& vv);

PyObject * BPy_BBox_from_BBox(const BBox< Vec3r > &bb);
PyObject * BPy_CurvePoint_from_CurvePoint(CurvePoint& cp);
PyObject * BPy_directedViewEdge_from_directedViewEdge(ViewVertex::directedViewEdge& dve);
PyObject * BPy_FEdge_from_FEdge(FEdge& fe);
PyObject * BPy_FEdgeSharp_from_FEdgeSharp(FEdgeSharp& fes);
PyObject * BPy_FEdgeSmooth_from_FEdgeSmooth(FEdgeSmooth& fes);
PyObject * BPy_Id_from_Id(Id& id);
PyObject * BPy_Interface0D_from_Interface0D(Interface0D& if0D);
PyObject * BPy_Interface1D_from_Interface1D(Interface1D& if1D);
PyObject * BPy_IntegrationType_from_IntegrationType(IntegrationType i);
PyObject * BPy_FrsMaterial_from_FrsMaterial(const FrsMaterial& m);
PyObject * BPy_Nature_from_Nature(unsigned short n);
PyObject * BPy_MediumType_from_MediumType(Stroke::MediumType n);
PyObject * BPy_SShape_from_SShape(SShape& ss);
PyObject * BPy_Stroke_from_Stroke(Stroke& s);
PyObject * BPy_StrokeAttribute_from_StrokeAttribute(StrokeAttribute& sa);
PyObject * BPy_StrokeVertex_from_StrokeVertex(StrokeVertex& sv);
PyObject * BPy_SVertex_from_SVertex(SVertex& sv);
PyObject * BPy_ViewVertex_from_ViewVertex(ViewVertex& vv);
PyObject * BPy_NonTVertex_from_NonTVertex(NonTVertex& ntv);
PyObject * BPy_TVertex_from_TVertex(TVertex& tv);
PyObject * BPy_ViewEdge_from_ViewEdge(ViewEdge& ve);
PyObject * BPy_Chain_from_Chain(Chain& c);
PyObject * BPy_ViewShape_from_ViewShape(ViewShape& vs);

PyObject * BPy_AdjacencyIterator_from_AdjacencyIterator(AdjacencyIterator& a_it);
PyObject * BPy_Interface0DIterator_from_Interface0DIterator(Interface0DIterator& if0D_it, bool reversed);
PyObject * BPy_CurvePointIterator_from_CurvePointIterator(CurveInternal::CurvePointIterator& cp_it);
PyObject * BPy_StrokeVertexIterator_from_StrokeVertexIterator(StrokeInternal::StrokeVertexIterator& sv_it,
                                                              bool reversed);
PyObject * BPy_SVertexIterator_from_SVertexIterator(ViewEdgeInternal::SVertexIterator& sv_it);
PyObject * BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator(ViewVertexInternal::orientedViewEdgeIterator& ove_it,
                                                                      bool reversed);
PyObject * BPy_ViewEdgeIterator_from_ViewEdgeIterator(ViewEdgeInternal::ViewEdgeIterator& ve_it);
PyObject * BPy_ChainingIterator_from_ChainingIterator(ChainingIterator& c_it);
PyObject * BPy_ChainPredicateIterator_from_ChainPredicateIterator(ChainPredicateIterator& cp_it);
PyObject * BPy_ChainSilhouetteIterator_from_ChainSilhouetteIterator(ChainSilhouetteIterator& cs_it);

//==============================
// Python => C++
//==============================

bool bool_from_PyBool(PyObject *b);
IntegrationType IntegrationType_from_BPy_IntegrationType(PyObject *obj);
Stroke::MediumType MediumType_from_BPy_MediumType(PyObject *obj);
Nature::EdgeNature EdgeNature_from_BPy_Nature(PyObject *obj);
bool Vec2f_ptr_from_PyObject(PyObject *obj, Vec2f &vec);
bool Vec3f_ptr_from_PyObject(PyObject *obj, Vec3f &vec);
bool Vec3r_ptr_from_PyObject(PyObject *obj, Vec3r &vec);
bool Vec2f_ptr_from_Vector(PyObject *obj, Vec2f &vec);
bool Vec3f_ptr_from_Vector(PyObject *obj, Vec3f &vec);
bool Vec3r_ptr_from_Vector(PyObject *obj, Vec3r &vec);
bool Vec3f_ptr_from_Color(PyObject *obj, Vec3f &vec);
bool Vec3r_ptr_from_Color(PyObject *obj, Vec3r &vec);
bool Vec2f_ptr_from_PyList(PyObject *obj, Vec2f &vec);
bool Vec3f_ptr_from_PyList(PyObject *obj, Vec3f &vec);
bool Vec3r_ptr_from_PyList(PyObject *obj, Vec3r &vec);
bool Vec2f_ptr_from_PyTuple(PyObject *obj, Vec2f &vec);
bool Vec3f_ptr_from_PyTuple(PyObject *obj, Vec3f &vec);
bool Vec3r_ptr_from_PyTuple(PyObject *obj, Vec3r &vec);

bool float_array_from_PyObject(PyObject *obj, float *v, int n);

int convert_v4(PyObject *obj, void *v);
int convert_v3(PyObject *obj, void *v);
int convert_v2(PyObject *obj, void *v);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* __FREESTYLE_PYTHON_CONVERT_H__ */
