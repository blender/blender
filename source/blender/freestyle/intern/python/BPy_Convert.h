/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include <typeinfo>

#include "../geometry/Geom.h"

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

#include "generic/python_utildefines.h"
#include "mathutils/mathutils.h"

//==============================
// C++ => Python
//==============================

PyObject *PyBool_from_bool(bool b);
PyObject *Vector_from_Vec2f(Freestyle::Geometry::Vec2f &v);
PyObject *Vector_from_Vec3f(Freestyle::Geometry::Vec3f &v);
PyObject *Vector_from_Vec3r(Freestyle::Geometry::Vec3r &v);

PyObject *Any_BPy_Interface0D_from_Interface0D(Freestyle::Interface0D &if0D);
PyObject *Any_BPy_Interface1D_from_Interface1D(Freestyle::Interface1D &if1D);
PyObject *Any_BPy_FEdge_from_FEdge(Freestyle::FEdge &fe);
PyObject *Any_BPy_ViewVertex_from_ViewVertex(Freestyle::ViewVertex &vv);

PyObject *BPy_BBox_from_BBox(const Freestyle::BBox<Freestyle::Geometry::Vec3r> &bb);
PyObject *BPy_CurvePoint_from_CurvePoint(Freestyle::CurvePoint &cp);
PyObject *BPy_directedViewEdge_from_directedViewEdge(Freestyle::ViewVertex::directedViewEdge &dve);
PyObject *BPy_FEdge_from_FEdge(Freestyle::FEdge &fe);
PyObject *BPy_FEdgeSharp_from_FEdgeSharp(Freestyle::FEdgeSharp &fes);
PyObject *BPy_FEdgeSmooth_from_FEdgeSmooth(Freestyle::FEdgeSmooth &fes);
PyObject *BPy_Id_from_Id(Freestyle::Id &id);
PyObject *BPy_Interface0D_from_Interface0D(Freestyle::Interface0D &if0D);
PyObject *BPy_Interface1D_from_Interface1D(Freestyle::Interface1D &if1D);
PyObject *BPy_IntegrationType_from_IntegrationType(Freestyle::IntegrationType i);
PyObject *BPy_FrsMaterial_from_FrsMaterial(const Freestyle::FrsMaterial &m);
PyObject *BPy_Nature_from_Nature(ushort n);
PyObject *BPy_MediumType_from_MediumType(Freestyle::Stroke::MediumType n);
PyObject *BPy_SShape_from_SShape(Freestyle::SShape &ss);
PyObject *BPy_Stroke_from_Stroke(Freestyle::Stroke &s);
PyObject *BPy_StrokeAttribute_from_StrokeAttribute(Freestyle::StrokeAttribute &sa);
PyObject *BPy_StrokeVertex_from_StrokeVertex(Freestyle::StrokeVertex &sv);
PyObject *BPy_SVertex_from_SVertex(Freestyle::SVertex &sv);
PyObject *BPy_ViewVertex_from_ViewVertex(Freestyle::ViewVertex &vv);
PyObject *BPy_NonTVertex_from_NonTVertex(Freestyle::NonTVertex &ntv);
PyObject *BPy_TVertex_from_TVertex(Freestyle::TVertex &tv);
PyObject *BPy_ViewEdge_from_ViewEdge(Freestyle::ViewEdge &ve);
PyObject *BPy_Chain_from_Chain(Freestyle::Chain &c);
PyObject *BPy_ViewShape_from_ViewShape(Freestyle::ViewShape &vs);

PyObject *BPy_AdjacencyIterator_from_AdjacencyIterator(Freestyle::AdjacencyIterator &a_it);
PyObject *BPy_Interface0DIterator_from_Interface0DIterator(Freestyle::Interface0DIterator &if0D_it,
                                                           bool reversed);
PyObject *BPy_CurvePointIterator_from_CurvePointIterator(
    Freestyle::CurveInternal::CurvePointIterator &cp_it);
PyObject *BPy_StrokeVertexIterator_from_StrokeVertexIterator(
    Freestyle::StrokeInternal::StrokeVertexIterator &sv_it, bool reversed);
PyObject *BPy_SVertexIterator_from_SVertexIterator(
    Freestyle::ViewEdgeInternal::SVertexIterator &sv_it);
PyObject *BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator(
    Freestyle::ViewVertexInternal::orientedViewEdgeIterator &ove_it, bool reversed);
PyObject *BPy_ViewEdgeIterator_from_ViewEdgeIterator(
    Freestyle::ViewEdgeInternal::ViewEdgeIterator &ve_it);
PyObject *BPy_ChainingIterator_from_ChainingIterator(Freestyle::ChainingIterator &c_it);
PyObject *BPy_ChainPredicateIterator_from_ChainPredicateIterator(
    Freestyle::ChainPredicateIterator &cp_it);
PyObject *BPy_ChainSilhouetteIterator_from_ChainSilhouetteIterator(
    Freestyle::ChainSilhouetteIterator &cs_it);

//==============================
// Python => C++
//==============================

bool bool_from_PyBool(PyObject *b);
Freestyle::IntegrationType IntegrationType_from_BPy_IntegrationType(PyObject *obj);
Freestyle::Stroke::MediumType MediumType_from_BPy_MediumType(PyObject *obj);
Freestyle::Nature::EdgeNature EdgeNature_from_BPy_Nature(PyObject *obj);
bool Vec2f_ptr_from_PyObject(PyObject *obj, Freestyle::Geometry::Vec2f &vec);
bool Vec3f_ptr_from_PyObject(PyObject *obj, Freestyle::Geometry::Vec3f &vec);
bool Vec3r_ptr_from_PyObject(PyObject *obj, Freestyle::Geometry::Vec3r &vec);
bool Vec2f_ptr_from_Vector(PyObject *obj, Freestyle::Geometry::Vec2f &vec);
bool Vec3f_ptr_from_Vector(PyObject *obj, Freestyle::Geometry::Vec3f &vec);
bool Vec3r_ptr_from_Vector(PyObject *obj, Freestyle::Geometry::Vec3r &vec);
bool Vec3f_ptr_from_Color(PyObject *obj, Freestyle::Geometry::Vec3f &vec);
bool Vec3r_ptr_from_Color(PyObject *obj, Freestyle::Geometry::Vec3r &vec);
bool Vec2f_ptr_from_PyList(PyObject *obj, Freestyle::Geometry::Vec2f &vec);
bool Vec3f_ptr_from_PyList(PyObject *obj, Freestyle::Geometry::Vec3f &vec);
bool Vec3r_ptr_from_PyList(PyObject *obj, Freestyle::Geometry::Vec3r &vec);
bool Vec2f_ptr_from_PyTuple(PyObject *obj, Freestyle::Geometry::Vec2f &vec);
bool Vec3f_ptr_from_PyTuple(PyObject *obj, Freestyle::Geometry::Vec3f &vec);
bool Vec3r_ptr_from_PyTuple(PyObject *obj, Freestyle::Geometry::Vec3r &vec);

bool float_array_from_PyObject(PyObject *obj, float *v, int n);

int convert_v4(PyObject *obj, void *v);
int convert_v3(PyObject *obj, void *v);
int convert_v2(PyObject *obj, void *v);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
