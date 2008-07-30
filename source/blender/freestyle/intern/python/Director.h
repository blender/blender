#ifndef  FREESTYLE_PYTHON_DIRECTOR
# define FREESTYLE_PYTHON_DIRECTOR

#include "../view_map/Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

//   SWIG directors
// ----------------------------
//   ViewEdgeInternal::ViewEdgeIterator;
//   ChainingIterator;
//   ChainSilhouetteIterator;
//   ChainPredicateIterator;
//   UnaryPredicate0D;
//   UnaryPredicate1D;
//   BinaryPredicate1D;
//   StrokeShader;

bool director_BPy_UnaryPredicate1D___call__( PyObject *py_up1D, Interface1D& if1D);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif // FREESTYLE_PYTHON_DIRECTOR
