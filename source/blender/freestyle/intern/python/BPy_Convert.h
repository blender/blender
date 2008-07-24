#ifndef FREESTYLE_PYTHON_CONVERT_H
#define FREESTYLE_PYTHON_CONVERT_H


#include "../geometry/Geom.h"
using namespace Geometry;

// BBox
#include "../geometry/BBox.h"

// FEdge, FEdgeSharp, FEdgeSmooth, SShape, SVertex, FEdgeInternal::SVertexIterator
#include "../view_map/Silhouette.h" 

// Id
#include "../system/Id.h"

// Interface0D, Interface0DIteratorNested, Interface0DIterator
#include "../view_map/Interface0D.h"

// Stroke, StrokeAttribute, StrokeVertex
#include "../stroke/Stroke.h"

// NonTVertex, TVertex, ViewEdge, ViewMap, ViewShape, ViewVertex
#include "../view_map/ViewMap.h"

// ViewVertexInternal::orientedViewEdgeIterator
// ViewEdgeInternal::SVertexIterator
// ViewEdgeInternal::ViewEdgeIterator
#include "../view_map/ViewMapIterators.h"
//#####################    IMPORTANT   #####################
//  Do not use the following namespaces within this file :
//   - ViewVertexInternal 
//   - ViewEdgeInternal
//##########################################################


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>
#include "api2_2x/vector.h"
#include "api2_2x/gen_utils.h"

PyObject * PyBool_from_bool( bool b );
bool bool_from_PyBool( PyObject *b );

PyObject * Vector_from_Vec2f( Vec2f& v );
PyObject * Vector_from_Vec3f( Vec3f& v );
PyObject * Vector_from_Vec3r( Vec3r& v );

PyObject * BPy_BBox_from_BBox( BBox< Vec3r > &bb );
PyObject * BPy_FEdge_from_FEdge( FEdge& fe );
PyObject * BPy_Id_from_Id( Id& id );
PyObject * BPy_Interface0D_from_Interface0D( Interface0D& if0D );
PyObject * BPy_Nature_from_Nature( unsigned short n );
PyObject * BPy_MediumType_from_MediumType( int n );
PyObject * BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ViewVertexInternal::orientedViewEdgeIterator& ove_it );
PyObject * BPy_SShape_from_SShape( SShape& ss );
PyObject * BPy_StrokeAttribute_from_StrokeAttribute( StrokeAttribute& sa );
PyObject * BPy_StrokeVertex_from_StrokeVertex( StrokeVertex& sv );
PyObject * BPy_SVertex_from_SVertex( SVertex& sv );
PyObject * BPy_ViewVertex_from_ViewVertex_ptr( ViewVertex *vv );
PyObject * BPy_ViewEdge_from_ViewEdge( ViewEdge& ve );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_CONVERT_H */
