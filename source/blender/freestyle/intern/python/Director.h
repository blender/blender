#ifndef  FREESTYLE_PYTHON_DIRECTOR
# define FREESTYLE_PYTHON_DIRECTOR

#include "../geometry/Geom.h"
#include "../winged_edge/Nature.h"

class AdjacencyIterator;
class FEdge;
class Id;
class Interface0D;
class Interface1D;
class Interface0DIterator;
class NonTVertex;
class Stroke;
class SVertex;
class TVertex;
class ViewEdge;
class ViewVertex;


#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

#ifdef __cplusplus
}
#endif

//   BinaryPredicate0D: __call__
int Director_BPy_BinaryPredicate0D___call__( PyObject *obj, Interface0D& i1, Interface0D& i2);

//   BinaryPredicate1D: __call__
int Director_BPy_BinaryPredicate1D___call__( PyObject *obj, Interface1D& i1, Interface1D& i2);

//	Interface0D: getX, getY, getZ, getPoint3D, getProjectedX, getProjectedY, getProjectedZ, getPoint2D, getFEdge, getId, getNature, castToSVertex, castToViewVertex, castToNonTVertex, castToTVertex
double Director_BPy_Interface0D_getX( PyObject *obj );
double Director_BPy_Interface0D_getY( PyObject *obj );
double Director_BPy_Interface0D_getZ( PyObject *obj );
Geometry::Vec3f Director_BPy_Interface0D_getPoint3D( PyObject *obj );
double Director_BPy_Interface0D_getProjectedX( PyObject *obj );
double Director_BPy_Interface0D_getProjectedY( PyObject *obj );
double Director_BPy_Interface0D_getProjectedZ( PyObject *obj );
Geometry::Vec2f Director_BPy_Interface0D_getPoint2D( PyObject *obj );
FEdge * Director_BPy_Interface0D_getFEdge( PyObject *obj );
Id Director_BPy_Interface0D_getId( PyObject *obj );
Nature::EdgeNature Director_BPy_Interface0D_getNature( PyObject *obj );
SVertex * Director_BPy_Interface0D_castToSVertex( PyObject *obj );
ViewVertex * Director_BPy_Interface0D_castToViewVertex( PyObject *obj );
NonTVertex * Director_BPy_Interface0D_castToNonTVertex( PyObject *obj );
TVertex * Director_BPy_Interface0D_castToTVertex( PyObject *obj );

//	Interface1D: verticesBegin, verticesEnd, pointsBegin, pointsEnd, getLength2D, getId, getNature
Interface0DIterator Director_BPy_Interface1D_verticesBegin( PyObject *obj );
Interface0DIterator Director_BPy_Interface1D_verticesEnd( PyObject *obj );
Interface0DIterator Director_BPy_Interface1D_pointsBegin( PyObject *obj );
Interface0DIterator Director_BPy_Interface1D_pointsEnd( PyObject *obj );
double Director_BPy_Interface1D_getLength2D( PyObject *obj );
Id Director_BPy_Interface1D_getId( PyObject *obj );
Nature::EdgeNature Director_BPy_Interface1D_getNature( PyObject *obj );

//	UnaryFunction{0D,1D}: __call__
int Director_BPy_UnaryFunction0D___call__( void *uf0D, PyObject *obj, Interface0DIterator& if0D_it);
int Director_BPy_UnaryFunction1D___call__( void *uf1D, PyObject *obj, Interface1D& if1D);

//   UnaryPredicate0D: __call__
int Director_BPy_UnaryPredicate0D___call__( PyObject *obj, Interface0DIterator& if0D_it);
	
//   UnaryPredicate1D: __call__
int Director_BPy_UnaryPredicate1D___call__( PyObject *obj, Interface1D& if1D);

//   StrokeShader: shade
int Director_BPy_StrokeShader_shade( PyObject *obj, Stroke& s);

//	Iterator: increment, decrement, isBegin, isEnd
void Director_BPy_Iterator_increment( PyObject *obj );
void Director_BPy_Iterator_decrement( PyObject *obj );
bool Director_BPy_Iterator_isBegin( PyObject *obj );
bool Director_BPy_Iterator_isEnd( PyObject *obj );

//   ChainingIterator: init, traverse
int Director_BPy_ChainingIterator_init( PyObject *obj );
int Director_BPy_ChainingIterator_traverse( PyObject *obj, AdjacencyIterator& a_it, ViewEdge **ve );





#endif // FREESTYLE_PYTHON_DIRECTOR
