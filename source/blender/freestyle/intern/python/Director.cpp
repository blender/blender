#include "Director.h"

#include "BPy_Convert.h"

#include "BPy_BinaryPredicate0D.h"
#include "BPy_BinaryPredicate1D.h"
#include "BPy_FrsMaterial.h"
#include "BPy_Id.h"
#include "BPy_UnaryFunction0D.h"
#include "BPy_UnaryFunction1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "BPy_StrokeShader.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "Interface1D/BPy_Stroke.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "BPy_ViewShape.h"

#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DEdgeNature.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DFloat.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DId.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DMaterial.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DUnsigned.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec2f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec3f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVectorViewShape.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DViewShape.h"

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
void Director_BPy_UnaryFunction0D___call__( void *uf0D, PyObject *obj, Interface0DIterator& if0D_it) {

	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", BPy_Interface0DIterator_from_Interface0DIterator(if0D_it) );
	
	if( BPy_UnaryFunction0DDouble_Check(obj) ) {	
		((UnaryFunction0D<double> *) uf0D)->result = PyFloat_AsDouble(result);

	} else if ( BPy_UnaryFunction0DEdgeNature_Check(obj) ) {
		((UnaryFunction0D<Nature::EdgeNature> *) uf0D)->result = EdgeNature_from_BPy_Nature(result);
	
	} else if ( BPy_UnaryFunction0DFloat_Check(obj) ) {

		((UnaryFunction0D<float> *) uf0D)->result = PyFloat_AsDouble(result);
	
	} else if ( BPy_UnaryFunction0DId_Check(obj) ) {
		((UnaryFunction0D<Id> *) uf0D)->result = *( ((BPy_Id *) result)->id );
	
	} else if ( BPy_UnaryFunction0DMaterial_Check(obj) ) {
		((UnaryFunction0D<Material> *) uf0D)->result = *( ((BPy_FrsMaterial *) result)->m );
	
	} else if ( BPy_UnaryFunction0DUnsigned_Check(obj) ) {
		((UnaryFunction0D<unsigned> *) uf0D)->result = PyInt_AsLong(result);
	
	} else if ( BPy_UnaryFunction0DVec2f_Check(obj) ) {
		Vec2f *v = Vec2f_ptr_from_Vector( result );
		((UnaryFunction0D<Vec2f> *) uf0D)->result = *v;
		delete v; 
	
	} else if ( BPy_UnaryFunction0DVec3f_Check(obj) ) {
		Vec3f *v = Vec3f_ptr_from_Vector( result );
		((UnaryFunction0D<Vec3f> *) uf0D)->result = *v;
		delete v;
	
	} else if ( BPy_UnaryFunction0DVectorViewShape_Check(obj) ) {
		vector<ViewShape*> vec;
		for( int i = 0; i < PyList_Size(result); i++) {
			ViewShape *b = ( (BPy_ViewShape *) PyList_GetItem(result, i) )->vs;
			vec.push_back( b );
		}
			
		((UnaryFunction0D< vector<ViewShape*> > *) uf0D)->result = vec;
	
	} else if ( BPy_UnaryFunction0DViewShape_Check(obj) ) {
		((UnaryFunction0D<ViewShape*> *) uf0D)->result = ((BPy_ViewShape *) result)->vs;
	
	}	

}

void Director_BPy_UnaryFunction1D___call__( void *uf1D, PyObject *obj, Interface1D& if1D) {

// BPy_UnaryFunction1DDouble
// BPy_UnaryFunction1DEdgeNature
// BPy_UnaryFunction1DFloat
// BPy_UnaryFunction1DUnsigned
// BPy_UnaryFunction1DVec2f
// BPy_UnaryFunction1DVec3f
// BPy_UnaryFunction1DVectorViewShape
// BPy_UnaryFunction1DVoid


}


// BPy_Iterator: increment, decrement, isBegin, isEnd
void Director_BPy_Iterator_increment( PyObject *obj ) {
	PyObject_CallMethod( obj, "increment", "", 0 );
}

void Director_BPy_Iterator_decrement( PyObject *obj ) {
	PyObject_CallMethod( obj, "decrement", "", 0 );
}

bool Director_BPy_Iterator_isBegin( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "isBegin", "", 0 );

	return bool_from_PyBool(result);
}

bool Director_BPy_Iterator_isEnd( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "isEnd", "", 0 );

	return bool_from_PyBool(result);
}

// BPy_Interface1D: verticesBegin, verticesEnd, pointsBegin, pointsEnd
Interface0DIterator Director_BPy_Interface1D_verticesBegin( PyObject *obj ){
	PyObject *result = PyObject_CallMethod( obj, "verticesBegin", "", 0 );

	return *( ((BPy_Interface0DIterator *) result)->if0D_it );
}

Interface0DIterator Director_BPy_Interface1D_verticesEnd( PyObject *obj ){
	PyObject *result =  PyObject_CallMethod( obj, "verticesEnd", "", 0 );

	return *( ((BPy_Interface0DIterator *) result)->if0D_it );
}

Interface0DIterator Director_BPy_Interface1D_pointsBegin( PyObject *obj ){
	PyObject *result =  PyObject_CallMethod( obj, "pointsBegin", "", 0 );

	return *( ((BPy_Interface0DIterator *) result)->if0D_it );
}

Interface0DIterator Director_BPy_Interface1D_pointsEnd( PyObject *obj ){
	PyObject *result =  PyObject_CallMethod( obj, "pointsEnd", "", 0 );

	return *( ((BPy_Interface0DIterator *) result)->if0D_it );
}

