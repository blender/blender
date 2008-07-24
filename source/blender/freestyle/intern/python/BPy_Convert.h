#ifndef FREESTYLE_PYTHON_CONVERT_H
#define FREESTYLE_PYTHON_CONVERT_H

#include "../geometry/Geom.h"
using namespace Geometry;

#include "BPy_Id.h"
#include "BPy_IntegrationType.h"
#include "BPy_Interface0D.h"
#include "Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface1D/BPy_FEdge.h"
#include "BPy_Nature.h"
#include "BPy_MediumType.h"
#include "BPy_StrokeAttribute.h"

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

PyObject * BPy_FEdge_from_FEdge( FEdge& fe );
PyObject * BPy_Id_from_Id( Id& id );
PyObject * BPy_Interface0D_from_Interface0D( Interface0D& if0D );
PyObject * BPy_Nature_from_Nature( unsigned short n );
PyObject * BPy_MediumType_from_MediumType( int n );
PyObject * BPy_StrokeAttribute_from_StrokeAttribute( StrokeAttribute& sa );
PyObject * BPy_StrokeVertex_from_StrokeVertex( StrokeVertex& sv );
PyObject * BPy_SVertex_from_SVertex( SVertex& sv );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_CONVERT_H */
