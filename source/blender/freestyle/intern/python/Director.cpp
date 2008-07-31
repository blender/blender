#include "Director.h"

#include "BPy_Convert.h"

#include "BPy_BinaryPredicate0D.h"
#include "BPy_BinaryPredicate1D.h"
#include "BPy_UnaryFunction0D.h"
#include "BPy_UnaryFunction1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "BPy_StrokeShader.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Interface1D/BPy_Stroke.h"
#include "Interface1D/BPy_ViewEdge.h"

//   BinaryPredicate0D: __call__
bool Director_BPy_BinaryPredicate0D___call__( PyObject *obj, Interface0D& i1, Interface0D& i2) {
	PyObject *result = PyObject_CallMethod( obj, "__call__", "OO", BPy_Interface0D_from_Interface0D(i1), BPy_Interface0D_from_Interface0D(i2) );
	
	return bool_from_PyBool(result);
}


//   BinaryPredicate1D: __call__
bool Director_BPy_BinaryPredicate1D___call__( PyObject *obj, Interface1D& i1, Interface1D& i2) {
	PyObject *result = PyObject_CallMethod( obj, "__call__", "OO", BPy_Interface1D_from_Interface1D(i1), BPy_Interface1D_from_Interface1D(i2) );
	
	return bool_from_PyBool(result);
}


//   UnaryPredicate0D: __call__
bool Director_BPy_UnaryPredicate0D___call__( PyObject *obj, Interface0DIterator& if0D_it) {
	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", BPy_Interface0DIterator_from_Interface0DIterator(if0D_it) );

	return bool_from_PyBool(result);
}


//   UnaryPredicate1D: __call__
bool Director_BPy_UnaryPredicate1D___call__( PyObject *obj, Interface1D& if1D) {
	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", BPy_Interface1D_from_Interface1D(if1D) );

	return bool_from_PyBool(result);
}


//   StrokeShader: shade
void Director_BPy_StrokeShader_shade( PyObject *obj, Stroke& s) {
	PyObject_CallMethod( obj, "shade", "O", BPy_Stroke_from_Stroke(s) );
}

//   ChainingIterator: init, traverse
void Director_BPy_ChainingIterator_init( PyObject *obj ) {
	PyObject_CallMethod( obj, "init", "", 0 );
}

ViewEdge * Director_BPy_ChainingIterator_traverse( PyObject *obj, AdjacencyIterator& a_it ) {
	PyObject *result = PyObject_CallMethod( obj, "traverse", "O", BPy_AdjacencyIterator_from_AdjacencyIterator(a_it) );

	return ((BPy_ViewEdge *) result)->ve;
}


// BPy_UnaryFunction{0D,1D}: __call__
// BPy_UnaryFunction0DDouble
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


