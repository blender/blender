#include "BPy_Convert.h"

#include "BPy_BBox.h"
#include "BPy_Material.h"
#include "BPy_Id.h"
#include "BPy_IntegrationType.h"
#include "BPy_Interface0D.h"
#include "Interface0D/BPy_CurvePoint.cpp"
#include "Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "BPy_Interface1D.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/BPy_Stroke.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "BPy_Nature.h"
#include "BPy_MediumType.h"
#include "BPy_SShape.h"
#include "BPy_StrokeAttribute.h"
#include "BPy_ViewShape.h"

#include "Iterator/BPy_AdjacencyIterator.h"
#include "Iterator/BPy_ChainPredicateIterator.h"
#include "Iterator/BPy_ChainSilhouetteIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Iterator/BPy_CurvePointIterator.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "Iterator/BPy_SVertexIterator.h"
#include "Iterator/BPy_StrokeVertexIterator.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_orientedViewEdgeIterator.h"

#include "../stroke/StrokeRep.h"

#ifdef __cplusplus
extern "C" {
#endif


///////////////////////////////////////////////////////////////////////////////////////////


PyObject * PyBool_from_bool( bool b ){
	return PyBool_FromLong( b ? 1 : 0);
}

bool bool_from_PyBool( PyObject *b ) {
	return b == Py_True;
}

PyObject * Vector_from_Vec2f( Vec2f& vec ) {
	float vec_data[2]; // because vec->_coord is protected

	vec_data[0] = vec.x();		vec_data[1] = vec.y();
	return newVectorObject( vec_data, 2, Py_NEW);
}

PyObject * Vector_from_Vec3f( Vec3f& vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject * Vector_from_Vec3r( Vec3r& vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject * BPy_Id_from_Id( Id& id ) {
	PyObject *py_id = Id_Type.tp_new( &Id_Type, 0, 0 );
	((BPy_Id *) py_id)->id = new Id( id.getFirst(), id.getSecond() );

	return py_id;
}

PyObject * BPy_Interface0D_from_Interface0D( Interface0D& if0D ) {
	PyObject *py_if0D =  Interface0D_Type.tp_new( &Interface0D_Type, 0, 0 );
	((BPy_Interface0D *) py_if0D)->if0D = &if0D;

	return py_if0D;
}

PyObject * BPy_Interface1D_from_Interface1D( Interface1D& if1D ) {
	PyObject *py_if1D =  Interface1D_Type.tp_new( &Interface1D_Type, 0, 0 );
	((BPy_Interface1D *) py_if1D)->if1D = &if1D;

	return py_if1D;
}


PyObject * BPy_SVertex_from_SVertex( SVertex& sv ) {
	PyObject *py_sv = SVertex_Type.tp_new( &SVertex_Type, 0, 0 );
	((BPy_SVertex *) py_sv)->sv = new SVertex( sv );
	((BPy_SVertex *) py_sv)->py_if0D.if0D = ((BPy_SVertex *) py_sv)->sv;

	return py_sv;
}

PyObject * BPy_FEdge_from_FEdge( FEdge& fe ) {
	PyObject *py_fe = FEdge_Type.tp_new( &FEdge_Type, 0, 0 );
	((BPy_FEdge *) py_fe)->fe = new FEdge( fe );
	((BPy_FEdge *) py_fe)->py_if1D.if1D = ((BPy_FEdge *) py_fe)->fe;

	return py_fe;
}

PyObject * BPy_Nature_from_Nature( unsigned short n ) {
	PyObject *py_n =  Nature_Type.tp_new( &Nature_Type, 0, 0 );

	PyObject *args = PyTuple_New(1);
	PyTuple_SetItem( args, 0, PyInt_FromLong(n) );
	Nature_Type.tp_init( py_n, args, 0 );
	Py_DECREF(args);

	return py_n;
}

PyObject * BPy_Stroke_from_Stroke( Stroke& s ) {
	PyObject *py_s = Stroke_Type.tp_new( &Stroke_Type, 0, 0 );
	((BPy_Stroke *) py_s)->s = new Stroke( s );
	((BPy_Stroke *) py_s)->py_if1D.if1D = ((BPy_Stroke *) py_s)->s;

	return py_s;
}

PyObject * BPy_StrokeAttribute_from_StrokeAttribute( StrokeAttribute& sa ) {
	PyObject *py_sa = StrokeAttribute_Type.tp_new( &StrokeAttribute_Type, 0, 0 );
	((BPy_StrokeAttribute *) py_sa)->sa = new StrokeAttribute( sa );

	return py_sa;	
}

PyObject * BPy_MediumType_from_MediumType( int n ) {
	PyObject *py_mt =  MediumType_Type.tp_new( &MediumType_Type, 0, 0 );

	PyObject *args = PyTuple_New(1);
	PyTuple_SetItem( args, 0, PyInt_FromLong(n) );
	MediumType_Type.tp_init( py_mt, args, 0 );
	Py_DECREF(args);

	return py_mt;
}

PyObject * BPy_StrokeVertex_from_StrokeVertex( StrokeVertex& sv ) {
	PyObject *py_sv = StrokeVertex_Type.tp_new( &StrokeVertex_Type, 0, 0 );
	((BPy_StrokeVertex *) py_sv)->sv = new StrokeVertex( sv );
	((BPy_StrokeVertex *) py_sv)->py_cp.cp = ((BPy_StrokeVertex *) py_sv)->sv;
	((BPy_StrokeVertex *) py_sv)->py_cp.py_if0D.if0D = ((BPy_StrokeVertex *) py_sv)->sv;

	return py_sv;
}

PyObject * BPy_ViewVertex_from_ViewVertex_ptr( ViewVertex *vv ) {
	PyObject *py_vv = ViewVertex_Type.tp_new( &ViewVertex_Type, 0, 0 );
	((BPy_ViewVertex *) py_vv)->vv = vv;
	((BPy_ViewVertex *) py_vv)->py_if0D.if0D = ((BPy_ViewVertex *) py_vv)->vv;

	return py_vv;
}

PyObject * BPy_BBox_from_BBox( BBox< Vec3r > &bb ) {
	PyObject *py_bb = BBox_Type.tp_new( &BBox_Type, 0, 0 );
	((BPy_BBox *) py_bb)->bb = new BBox< Vec3r >( bb );

	return py_bb;
}

PyObject * BPy_ViewEdge_from_ViewEdge( ViewEdge& ve ) {
	PyObject *py_ve = ViewEdge_Type.tp_new( &ViewEdge_Type, 0, 0 );
	((BPy_ViewEdge *) py_ve)->ve = new ViewEdge( ve );
	((BPy_ViewEdge *) py_ve)->py_if1D.if1D = ((BPy_ViewEdge *) py_ve)->ve;

	return py_ve;
}

PyObject * BPy_SShape_from_SShape( SShape& ss ) {
	PyObject *py_ss = SShape_Type.tp_new( &SShape_Type, 0, 0 );
	((BPy_SShape *) py_ss)->ss = new SShape( ss );

	return py_ss;	
}

PyObject * BPy_ViewShape_from_ViewShape( ViewShape& vs ) {
	PyObject *py_vs = ViewShape_Type.tp_new( &ViewShape_Type, 0, 0 );
	((BPy_ViewShape *) py_vs)->vs = new ViewShape( vs );

	return py_vs;
}

PyObject * BPy_Material_from_Material( Material& m ){
	PyObject *py_m = Material_Type.tp_new( &Material_Type, 0, 0 );
	((BPy_Material*) py_m)->m = new Material( m );

	return py_m;
}

PyObject * BPy_IntegrationType_from_IntegrationType( int i ) {
	PyObject *py_it = IntegrationType_Type.tp_new( &IntegrationType_Type, 0, 0 );

	PyObject *args = PyTuple_New(1);
	PyTuple_SetItem( args, 0, PyInt_FromLong(i) );
	IntegrationType_Type.tp_init( py_it, args, 0 );
	Py_DECREF(args);

	return py_it;
}

PyObject * BPy_CurvePoint_from_CurvePoint( CurvePoint& cp ) {
	PyObject *py_cp = CurvePoint_Type.tp_new( &CurvePoint_Type, 0, 0 );
	((BPy_CurvePoint*) py_cp)->cp = new CurvePoint( cp );

	return py_cp;
}

PyObject * BPy_directedViewEdge_from_directedViewEdge( ViewVertex::directedViewEdge& dve ) {
	PyObject *py_dve = PyList_New(2);
	
	PyList_SetItem(	py_dve, 0, BPy_ViewEdge_from_ViewEdge(*(dve.first)) );
	PyList_SetItem(	py_dve, 1, PyBool_from_bool(dve.second) );
	
	return py_dve;
}


//==============================
// Constants
//==============================

IntegrationType IntegrationType_from_BPy_IntegrationType( PyObject* obj ) {
	return static_cast<IntegrationType>( PyInt_AsLong(obj) );
}

Stroke::MediumType MediumType_from_BPy_MediumType( PyObject* obj ) {
	return static_cast<Stroke::MediumType>( PyInt_AsLong(obj) );
}

//==============================
// Iterators
//==============================

PyObject * BPy_AdjacencyIterator_from_AdjacencyIterator( AdjacencyIterator& a_it ) {
	PyObject *py_a_it = AdjacencyIterator_Type.tp_new( &AdjacencyIterator_Type, 0, 0 );
	((BPy_AdjacencyIterator *) py_a_it)->a_it = new AdjacencyIterator( a_it );
	((BPy_AdjacencyIterator *) py_a_it)->py_it.it = ((BPy_AdjacencyIterator *) py_a_it)->a_it;

	return py_a_it;
}

PyObject * BPy_Interface0DIterator_from_Interface0DIterator( Interface0DIterator& if0D_it ) {
	PyObject *py_if0D_it = Interface0DIterator_Type.tp_new( &Interface0DIterator_Type, 0, 0 );
	((BPy_Interface0DIterator *) py_if0D_it)->if0D_it = new Interface0DIterator( if0D_it );
	((BPy_Interface0DIterator *) py_if0D_it)->py_it.it = ((BPy_Interface0DIterator *) py_if0D_it)->if0D_it;

	return py_if0D_it;
}

PyObject * BPy_CurvePointIterator_from_CurvePointIterator( CurveInternal::CurvePointIterator& cp_it ) {
	PyObject *py_cp_it = CurvePointIterator_Type.tp_new( &CurvePointIterator_Type, 0, 0 );
	((BPy_CurvePointIterator *) py_cp_it)->cp_it = new CurveInternal::CurvePointIterator( cp_it );
	((BPy_CurvePointIterator *) py_cp_it)->py_it.it = ((BPy_CurvePointIterator *) py_cp_it)->cp_it;

	return py_cp_it;
}

PyObject * BPy_StrokeVertexIterator_from_StrokeVertexIterator( StrokeInternal::StrokeVertexIterator& sv_it) {
	PyObject *py_sv_it = StrokeVertexIterator_Type.tp_new( &StrokeVertexIterator_Type, 0, 0 );
	((BPy_StrokeVertexIterator *) py_sv_it)->sv_it = new StrokeInternal::StrokeVertexIterator( sv_it );
	((BPy_StrokeVertexIterator *) py_sv_it)->py_it.it = ((BPy_StrokeVertexIterator *) py_sv_it)->sv_it;

	return py_sv_it;
}

PyObject * BPy_SVertexIterator_from_SVertexIterator( ViewEdgeInternal::SVertexIterator& sv_it ) {
	PyObject *py_sv_it = SVertexIterator_Type.tp_new( &SVertexIterator_Type, 0, 0 );
	((BPy_SVertexIterator *) py_sv_it)->sv_it = new ViewEdgeInternal::SVertexIterator( sv_it );
	((BPy_SVertexIterator *) py_sv_it)->py_it.it = ((BPy_SVertexIterator *) py_sv_it)->sv_it;
	
	return py_sv_it;
}


PyObject * BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ViewVertexInternal::orientedViewEdgeIterator& ove_it ) {
	PyObject *py_ove_it = orientedViewEdgeIterator_Type.tp_new( &orientedViewEdgeIterator_Type, 0, 0 );
	((BPy_orientedViewEdgeIterator *) py_ove_it)->ove_it = new ViewVertexInternal::orientedViewEdgeIterator( ove_it );
	((BPy_orientedViewEdgeIterator *) py_ove_it)->py_it.it = ((BPy_orientedViewEdgeIterator *) py_ove_it)->ove_it;
	
	return py_ove_it;
}

PyObject * BPy_ViewEdgeIterator_from_ViewEdgeIterator( ViewEdgeInternal::ViewEdgeIterator& ve_it )  {
	PyObject *py_ve_it = ViewEdgeIterator_Type.tp_new( &ViewEdgeIterator_Type, 0, 0 );
	((BPy_ViewEdgeIterator *) py_ve_it)->ve_it = new ViewEdgeInternal::ViewEdgeIterator( ve_it );
	((BPy_ViewEdgeIterator *) py_ve_it)->py_it.it =	((BPy_ViewEdgeIterator *) py_ve_it)->ve_it;
	
	return py_ve_it;
}

PyObject * BPy_ChainingIterator_from_ChainingIterator( ChainingIterator& c_it ) {
	PyObject *py_c_it = ChainingIterator_Type.tp_new( &ChainingIterator_Type, 0, 0 );
	((BPy_ChainingIterator *) py_c_it)->c_it = new ChainingIterator( c_it );
	((BPy_ChainingIterator *) py_c_it)->py_ve_it.py_it.it = ((BPy_ChainingIterator *) py_c_it)->c_it;
	
	return py_c_it;
}

PyObject * BPy_ChainPredicateIterator_from_ChainPredicateIterator( ChainPredicateIterator& cp_it ) {
	PyObject *py_cp_it = ChainPredicateIterator_Type.tp_new( &ChainPredicateIterator_Type, 0, 0 );
	((BPy_ChainPredicateIterator *) py_cp_it)->cp_it = new ChainPredicateIterator( cp_it );
	((BPy_ChainPredicateIterator *) py_cp_it)->py_c_it.py_ve_it.py_it.it = ((BPy_ChainPredicateIterator *) py_cp_it)->cp_it;

	return py_cp_it;
}

PyObject * BPy_ChainSilhouetteIterator_from_ChainSilhouetteIterator( ChainSilhouetteIterator& cs_it ) {
	PyObject *py_cs_it = ChainSilhouetteIterator_Type.tp_new( &ChainSilhouetteIterator_Type, 0, 0 );
	((BPy_ChainSilhouetteIterator *) py_cs_it)->cs_it = new ChainSilhouetteIterator( cs_it );
	((BPy_ChainSilhouetteIterator *) py_cs_it)->py_c_it.py_ve_it.py_it.it = ((BPy_ChainSilhouetteIterator *) py_cs_it)->cs_it;

	return py_cs_it;
}





///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif