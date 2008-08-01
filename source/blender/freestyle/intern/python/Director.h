#ifndef  FREESTYLE_PYTHON_DIRECTOR
# define FREESTYLE_PYTHON_DIRECTOR

class Interface0D;
class Interface1D;
class Interface0DIterator;
class Stroke;
class AdjacencyIterator;
class ViewEdge;

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

#ifdef __cplusplus
}
#endif

//   BinaryPredicate0D: __call__
bool Director_BPy_BinaryPredicate0D___call__( PyObject *obj, Interface0D& i1, Interface0D& i2);

//   BinaryPredicate1D: __call__
bool Director_BPy_BinaryPredicate1D___call__( PyObject *obj, Interface1D& i1, Interface1D& i2);

//   UnaryPredicate0D: __call__
bool Director_BPy_UnaryPredicate0D___call__( PyObject *obj, Interface0DIterator& if0D_it);
	
//   UnaryPredicate1D: __call__
bool Director_BPy_UnaryPredicate1D___call__( PyObject *obj, Interface1D& if1D);

//   StrokeShader: shade
void Director_BPy_StrokeShader_shade( PyObject *obj, Stroke& s);

//   ChainingIterator: init, traverse
void Director_BPy_ChainingIterator_init( PyObject *obj );
ViewEdge * Director_BPy_ChainingIterator_traverse( PyObject *obj, AdjacencyIterator& a_it );

// BPy_UnaryFunction{0D,1D}: __call__
void Director_BPy_UnaryFunction0D___call__( void *uf0D, PyObject *obj, Interface0DIterator& if0D_it);
void Director_BPy_UnaryFunction1D___call__( void *uf1D, PyObject *obj, Interface1D& if1D);

// BPy_Iterator: increment, decrement, isBegin, isEnd
void Director_BPy_Iterator_increment( PyObject *obj );
void Director_BPy_Iterator_decrement( PyObject *obj );
bool Director_BPy_Iterator_isBegin( PyObject *obj );
bool Director_BPy_Iterator_isEnd( PyObject *obj );

// BPy_Interface1D: verticesBegin, verticesEnd, pointsBegin, pointsEnd
Interface0DIterator Director_BPy_Interface1D_verticesBegin( PyObject *obj );
Interface0DIterator Director_BPy_Interface1D_verticesEnd( PyObject *obj );
Interface0DIterator Director_BPy_Interface1D_pointsBegin( PyObject *obj );
Interface0DIterator Director_BPy_Interface1D_pointsEnd( PyObject *obj );



#endif // FREESTYLE_PYTHON_DIRECTOR
