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

// BPy_UnaryFunction0DDouble
double Director_BPy_UnaryFunction0DDouble___call__( PyObject *obj, Interface0DIterator& if0D_it);
// BPy_UnaryFunction0DEdgeNature
// BPy_UnaryFunction0DFloat
// BPy_UnaryFunction0DId
// BPy_UnaryFunction0DMaterial
// BPy_UnaryFunction0DUnsigned
// BPy_UnaryFunction0DVec2f
// BPy_UnaryFunction0DVec3f
// BPy_UnaryFunction0DVectorViewShape
// BPy_UnaryFunction0DViewShape

// BPy_UnaryFunction1DDouble
// BPy_UnaryFunction1DEdgeNature
// BPy_UnaryFunction1DFloat
// BPy_UnaryFunction1DUnsigned
// BPy_UnaryFunction1DVec2f
// BPy_UnaryFunction1DVec3f
// BPy_UnaryFunction1DVectorViewShape
// BPy_UnaryFunction1DVoid
void Director_BPy_UnaryFunction1DVoid___call__( PyObject *obj, Interface1D& if1D);




#endif // FREESTYLE_PYTHON_DIRECTOR
