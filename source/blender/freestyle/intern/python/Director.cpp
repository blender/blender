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
#include "Interface0D/BPy_SVertex.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface0D/ViewVertex/BPy_NonTVertex.h"
#include "Interface0D/ViewVertex/BPy_TVertex.h"
#include "Interface1D/BPy_FEdge.h"
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

#include "UnaryFunction1D/BPy_UnaryFunction1DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DEdgeNature.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DFloat.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DUnsigned.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec2f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec3f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVectorViewShape.h"


//   BinaryPredicate0D: __call__
bool Director_BPy_BinaryPredicate0D___call__( PyObject *obj, Interface0D& i1, Interface0D& i2) {
	PyObject *arg1 = BPy_Interface0D_from_Interface0D(i1);
	PyObject *arg2 = BPy_Interface0D_from_Interface0D(i2);
	PyObject *result = PyObject_CallMethod( obj, "__call__", "OO", arg1, arg2 );
	Py_DECREF(arg1);
	Py_DECREF(arg2);
	if (!result) {
		cerr << "Warning: BinaryPredicate0D::__call__() failed." << endl;
		PyErr_Clear();
		return false;
	}
	bool ret = bool_from_PyBool(result);
	Py_DECREF(result);
	return ret;
}


//   BinaryPredicate1D: __call__
bool Director_BPy_BinaryPredicate1D___call__( PyObject *obj, Interface1D& i1, Interface1D& i2) {
	PyObject *arg1 = BPy_Interface1D_from_Interface1D(i1);
	PyObject *arg2 = BPy_Interface1D_from_Interface1D(i2);
	PyObject *result = PyObject_CallMethod( obj, "__call__", "OO", arg1, arg2 );
	Py_DECREF(arg1);
	Py_DECREF(arg2);
	if (!result) {
		cerr << "Warning: BinaryPredicate1D::__call__() failed." << endl;
		PyErr_Clear();
		return false;
	}
	bool ret = bool_from_PyBool(result);
	Py_DECREF(result);
	return ret;
}


//   UnaryPredicate0D: __call__
bool Director_BPy_UnaryPredicate0D___call__( PyObject *obj, Interface0DIterator& if0D_it) {
	PyObject *arg = BPy_Interface0DIterator_from_Interface0DIterator(if0D_it);
	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", arg );
	Py_DECREF(arg);
	if (!result) {
		cerr << "Warning: UnaryPredicate0D::__call__() failed." << endl;
		PyErr_Clear();
		return false;
	}
	bool ret = bool_from_PyBool(result);
	Py_DECREF(result);
	return ret;
}


//   UnaryPredicate1D: __call__
bool Director_BPy_UnaryPredicate1D___call__( PyObject *obj, Interface1D& if1D) {
	PyObject *arg = BPy_Interface1D_from_Interface1D(if1D);
	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", arg );
	Py_DECREF(arg);
	if (!result) {
		cerr << "Warning: UnaryPredicate1D::__call__() failed." << endl;
		PyErr_Clear();
		return false;
	}
	bool ret = bool_from_PyBool(result);
	Py_DECREF(result);
	return ret;
}


//   StrokeShader: shade
void Director_BPy_StrokeShader_shade( PyObject *obj, Stroke& s) {
	PyObject *arg = BPy_Stroke_from_Stroke_ptr(&s);
	PyObject *result = PyObject_CallMethod( obj, "shade", "O", arg );
	Py_DECREF(arg);
	if (!result) {
		cerr << "Warning: StrokeShader::shade() failed" << endl;
		PyErr_Clear();
		return;
	}
	Py_DECREF(result);
}

//   ChainingIterator: init, traverse
void Director_BPy_ChainingIterator_init( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "init", "", 0 );
	if (!result) {
		cerr << "Warning: ChainingIterator::init() failed." << endl;
		PyErr_Clear();
		return;
	}
	Py_DECREF(result);
}

ViewEdge * Director_BPy_ChainingIterator_traverse( PyObject *obj, AdjacencyIterator& a_it ) {
	PyObject *arg = BPy_AdjacencyIterator_from_AdjacencyIterator(a_it);
	PyObject *result = PyObject_CallMethod( obj, "traverse", "O", arg );
	Py_DECREF(arg);
	if (!result) {
		cerr << "Warning: ChainingIterator::traverse() failed." << endl;
		PyErr_Clear();
		return NULL;
	}
	ViewEdge *ret = ((BPy_ViewEdge *) result)->ve;
	Py_DECREF(result);
	return ret;
}


// BPy_UnaryFunction{0D,1D}: __call__
void Director_BPy_UnaryFunction0D___call__( void *uf0D, PyObject *obj, Interface0DIterator& if0D_it) {

	PyObject *arg = BPy_Interface0DIterator_from_Interface0DIterator(if0D_it);
	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", arg );
	Py_DECREF(arg);
	if (!result) {
		cerr << "Warning: UnaryFunction0D::__call__() failed." << endl;
		PyErr_Clear();
		return;
	}
	
	if( BPy_UnaryFunction0DDouble_Check(obj) ) {	
		((UnaryFunction0D<double> *) uf0D)->result = PyFloat_AsDouble(result);

	} else if ( BPy_UnaryFunction0DEdgeNature_Check(obj) ) {
		((UnaryFunction0D<Nature::EdgeNature> *) uf0D)->result = EdgeNature_from_BPy_Nature(result);
	
	} else if ( BPy_UnaryFunction0DFloat_Check(obj) ) {
		((UnaryFunction0D<float> *) uf0D)->result = PyFloat_AsDouble(result);
	
	} else if ( BPy_UnaryFunction0DId_Check(obj) ) {
		((UnaryFunction0D<Id> *) uf0D)->result = *( ((BPy_Id *) result)->id );
	
	} else if ( BPy_UnaryFunction0DMaterial_Check(obj) ) {
		((UnaryFunction0D<FrsMaterial> *) uf0D)->result = *( ((BPy_FrsMaterial *) result)->m );
	
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

	Py_DECREF(result);
}

void Director_BPy_UnaryFunction1D___call__( void *uf1D, PyObject *obj, Interface1D& if1D) {

	PyObject *arg = BPy_Interface1D_from_Interface1D(if1D);
	PyObject *result = PyObject_CallMethod( obj, "__call__", "O", arg );
	Py_DECREF(arg);
	if (!result) {
		cerr << "Warning: UnaryFunction1D::__call__() failed." << endl;
		PyErr_Clear();
		return;
	}
	
	if( BPy_UnaryFunction1DDouble_Check(obj) ) {	
		((UnaryFunction1D<double> *) uf1D)->result = PyFloat_AsDouble(result);

	} else if ( BPy_UnaryFunction1DEdgeNature_Check(obj) ) {
		((UnaryFunction1D<Nature::EdgeNature> *) uf1D)->result = EdgeNature_from_BPy_Nature(result);
	
	} else if ( BPy_UnaryFunction1DFloat_Check(obj) ) {
		((UnaryFunction1D<float> *) uf1D)->result = PyFloat_AsDouble(result);
	
	} else if ( BPy_UnaryFunction1DUnsigned_Check(obj) ) {
		((UnaryFunction1D<unsigned> *) uf1D)->result = PyInt_AsLong(result);
	
	} else if ( BPy_UnaryFunction1DVec2f_Check(obj) ) {
		Vec2f *v = Vec2f_ptr_from_Vector( result );
		((UnaryFunction1D<Vec2f> *) uf1D)->result = *v;
		delete v; 
	
	} else if ( BPy_UnaryFunction1DVec3f_Check(obj) ) {
		Vec3f *v = Vec3f_ptr_from_Vector( result );
		((UnaryFunction1D<Vec3f> *) uf1D)->result = *v;
		delete v;
	
	} else if ( BPy_UnaryFunction1DVectorViewShape_Check(obj) ) {
		vector<ViewShape*> vec;
		for( int i = 1; i < PyList_Size(result); i++) {
			ViewShape *b = ( (BPy_ViewShape *) PyList_GetItem(result, i) )->vs;
			vec.push_back( b );
		}
			
		((UnaryFunction1D< vector<ViewShape*> > *) uf1D)->result = vec;
	
	} 

	Py_DECREF(result);
}


//	Iterator: increment, decrement, isBegin, isEnd
void Director_BPy_Iterator_increment( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "increment", "", 0 );
	Py_DECREF(result);
}

void Director_BPy_Iterator_decrement( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "decrement", "", 0 );
	Py_DECREF(result);
}

bool Director_BPy_Iterator_isBegin( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "isBegin", "", 0 );
	bool ret = bool_from_PyBool(result);
	Py_DECREF(result);
	return ret;
}

bool Director_BPy_Iterator_isEnd( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "isEnd", "", 0 );
	bool ret = bool_from_PyBool(result);
	Py_DECREF(result);
	return ret;
}

//	Interface0D: getX, getY, getZ, getPoint3D, getProjectedX, getProjectedY, getProjectedZ, getPoint2D, getFEdge, getId, getNature, castToSVertex, castToViewVertex, castToNonTVertex, castToTVertex
double Director_BPy_Interface0D_getX( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getX", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

double Director_BPy_Interface0D_getY( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getY", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

double Director_BPy_Interface0D_getZ( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getZ", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

Geometry::Vec3f Director_BPy_Interface0D_getPoint3D( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getPoint3D", "", 0 );
	
	Geometry::Vec3f *v_ref = Vec3f_ptr_from_Vector( result );
	Geometry::Vec3f v(*v_ref);
	Py_DECREF(result);
	delete v_ref;

	return v;
}

double Director_BPy_Interface0D_getProjectedX( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getProjectedX", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

double Director_BPy_Interface0D_getProjectedY( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getProjectedY", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

double Director_BPy_Interface0D_getProjectedZ( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getProjectedZ", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

Geometry::Vec2f Director_BPy_Interface0D_getPoint2D( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getPoint2D", "", 0 );

	Geometry::Vec2f *v_ref = Vec2f_ptr_from_Vector( result );
	Geometry::Vec2f v(*v_ref);
	Py_DECREF(result);
	delete v_ref;

	return v;
}

FEdge * Director_BPy_Interface0D_getFEdge( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getFEdge", "", 0 );
	FEdge *ret = ((BPy_FEdge *) result)->fe;
	Py_DECREF(result);
	return ret;
}

Id Director_BPy_Interface0D_getId( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getId", "", 0 );
	Id ret = *( ((BPy_Id *) result)->id );
	Py_DECREF(result);
	return ret;
}

Nature::EdgeNature Director_BPy_Interface0D_getNature( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getNature", "", 0 );
	Nature::EdgeNature ret = EdgeNature_from_BPy_Nature(result);
	Py_DECREF(result);
	return ret;
}

SVertex * Director_BPy_Interface0D_castToSVertex( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "castToSVertex", "", 0 );
	SVertex *ret = ((BPy_SVertex *) result)->sv;
	Py_DECREF(result);
	return ret;
}

ViewVertex * Director_BPy_Interface0D_castToViewVertex( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "castToViewVertex", "", 0 );
	ViewVertex *ret = ((BPy_ViewVertex *) result)->vv;
	Py_DECREF(result);
	return ret;
}

NonTVertex * Director_BPy_Interface0D_castToNonTVertex( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "castToNonTVertex", "", 0 );
	NonTVertex *ret = ((BPy_NonTVertex *) result)->ntv;
	Py_DECREF(result);
	return ret;
}

TVertex * Director_BPy_Interface0D_castToTVertex( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "castToTVertex", "", 0 );
	TVertex *ret = ((BPy_TVertex *) result)->tv;
	Py_DECREF(result);
	return ret;
}

//	Interface1D: verticesBegin, verticesEnd, pointsBegin, pointsEnd
Interface0DIterator Director_BPy_Interface1D_verticesBegin( PyObject *obj ){
	PyObject *result = PyObject_CallMethod( obj, "verticesBegin", "", 0 );
	Interface0DIterator ret = *( ((BPy_Interface0DIterator *) result)->if0D_it );
	Py_DECREF(result);
	return ret;
}

Interface0DIterator Director_BPy_Interface1D_verticesEnd( PyObject *obj ){
	PyObject *result =  PyObject_CallMethod( obj, "verticesEnd", "", 0 );
	Interface0DIterator ret = *( ((BPy_Interface0DIterator *) result)->if0D_it );
	Py_DECREF(result);
	return ret;
}

Interface0DIterator Director_BPy_Interface1D_pointsBegin( PyObject *obj ){
	PyObject *result =  PyObject_CallMethod( obj, "pointsBegin", "", 0 );
	Interface0DIterator ret = *( ((BPy_Interface0DIterator *) result)->if0D_it );
	Py_DECREF(result);
	return ret;
}

Interface0DIterator Director_BPy_Interface1D_pointsEnd( PyObject *obj ){
	PyObject *result =  PyObject_CallMethod( obj, "pointsEnd", "", 0 );
	Interface0DIterator ret = *( ((BPy_Interface0DIterator *) result)->if0D_it );
	Py_DECREF(result);
	return ret;
}

double Director_BPy_Interface1D_getLength2D( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getLength2D", "", 0 );
	double ret = PyFloat_AsDouble(result);
	Py_DECREF(result);
	return ret;
}

Id Director_BPy_Interface1D_getId( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getId", "", 0 );
	Id ret = *( ((BPy_Id *) result)->id );
	Py_DECREF(result);
	return ret;
}

Nature::EdgeNature Director_BPy_Interface1D_getNature( PyObject *obj ) {
	PyObject *result = PyObject_CallMethod( obj, "getNature", "", 0 );
	Nature::EdgeNature ret = EdgeNature_from_BPy_Nature(result);
	Py_DECREF(result);
	return ret;
}
