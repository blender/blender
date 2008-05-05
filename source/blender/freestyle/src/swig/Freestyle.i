//
//  FileName         : Freestyle.i
//  Author           : Emmanuel Turquin
//  Purpose          : SWIG file used to generate Python binding
//  Date Of Creation : 18/07/2003
//
///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////



%module(directors="1") Freestyle
%{
 #include "../system/Cast.h"
 #include "../stroke/Module.h"
 #include "../system/Precision.h"
 #include "../system/Id.h"
 #include "../geometry/VecMat.h"
 #include "../geometry/Geom.h"
 #include "../geometry/Noise.h"
 #include "../scene_graph/Material.h"
 #include "../winged_edge/Nature.h"
 #include "../view_map/Interface0D.h"
 #include "../view_map/Interface1D.h"
 #include "../view_map/Functions0D.h"
 #include "../view_map/Functions1D.h"
 #include "../view_map/Silhouette.h"
 #include "../view_map/ViewMap.h"
 #include "../view_map/ViewMapIterators.h"
 #include "../stroke/AdvancedFunctions0D.h"
 #include "../stroke/AdvancedFunctions1D.h"
 #include "../stroke/ChainingIterators.h"
 #include "../stroke/ContextFunctions.h"
 #include "../stroke/Predicates0D.h"
 #include "../stroke/Predicates1D.h"
 #include "../stroke/AdvancedPredicates1D.h"
 #include "../stroke/StrokeShader.h"
// #include "../stroke/Curve.h"
 #include "../stroke/CurveIterators.h"       
 #include "../stroke/Stroke.h"
 #include "../stroke/StrokeIterators.h"
 #include "../stroke/BasicStrokeShaders.h"
 #include "../stroke/AdvancedStrokeShaders.h"
 #include "../stroke/Operators.h"
 #include "../stroke/Canvas.h"
%}

%include "stl.i"
%template(vectorInt)   std::vector<int>;

#ifdef SWIG
# define LIB_GEOMETRY_EXPORT
# define LIB_VIEW_MAP_EXPORT
# define LIB_STROKE_EXPORT
# define LIB_GEOMETRY_EXPORT
#endif // SWIG

// Generic exception handler
%exception {
    try {
        $action
    }
//    catch (Swig::DirectorTypeMismatch&) {
//      cout << "Warning: return type mismatch" << endl;
//    }
    catch (Swig::DirectorException&) {
      cout << "Warning: director exception catched" << endl;
    }
}

// Precision
%include "../system/Precision.h"

// Id
%include "../system/Id.h"

// Vec
%include "../geometry/VecMat.h"

%template(Vec_2u)   VecMat::Vec<unsigned,2>;
%template(Vec_2i)   VecMat::Vec<int,2>;
%template(Vec_2d)   VecMat::Vec<double,2>;
%template(Vec_2f)   VecMat::Vec<float,2>;

%template(Vec2u)	VecMat::Vec2<unsigned>;
%template(Vec2i)	VecMat::Vec2<int>;
%template(Vec2f)	VecMat::Vec2<float>;
%template(Vec2d)	VecMat::Vec2<double>;

%template(Vec_3u)   VecMat::Vec<unsigned,3>;
%template(Vec_3i)   VecMat::Vec<int,3>;
%template(Vec_3d)   VecMat::Vec<double,3>;
%template(Vec_3f)   VecMat::Vec<float,3>;

%template(Vec3u)	VecMat::Vec3<unsigned>;
%template(Vec3i)	VecMat::Vec3<int>;
%template(Vec3f)	VecMat::Vec3<float>;
%template(Vec3d)	VecMat::Vec3<double>;

//%template(HVec3u) VecMat::HVec3<unsigned>;
//%template(HVec3i) VecMat::HVec3<int>;
//%template(HVec3f) VecMat::HVec3<float>;
//%template(HVec3d) VecMat::HVec3<double>;
//%template(HVec3r) VecMat::HVec3<real>;

//%template(Matrix22u) VecMat::SquareMatrix<unsigned, 2>;
//%template(Matrix22i) VecMat::SquareMatrix<int, 2>;
//%template(Matrix22f) VecMat::SquareMatrix<float, 2>;
//%template(Matrix22d) VecMat::SquareMatrix<double, 2>;

//%template(Matrix22r) VecMat::SquareMatrix<real, 2>;
//%template(Matrix33u) VecMat::SquareMatrix<unsigned, 3>;
//%template(Matrix33i) VecMat::SquareMatrix<int, 3>;
//%template(Matrix33f) VecMat::SquareMatrix<float, 3>;
//%template(Matrix33d) VecMat::SquareMatrix<double, 3>;
//%template(Matrix33r) VecMat::SquareMatrix<real, 3>;

//%template(Matrix44u) VecMat::SquareMatrix<unsigned, 4>;
//%template(Matrix44i) VecMat::SquareMatrix<int, 4>;
//%template(Matrix44f) VecMat::SquareMatrix<float, 4>;
//%template(Matrix44d) VecMat::SquareMatrix<double, 4>;
//%template(Matrix44r) VecMat::SquareMatrix<real, 4>;

%include "../geometry/Geom.h"

// Noise
%include "../geometry/Noise.h"

// Material
%include "../scene_graph/Material.h"

// ViewMap Components
%include "../winged_edge/Nature.h"

%rename(getObject) Interface0DIterator::operator*;
%rename(getObject) Interface0DIteratorNested::operator*;
%include "../view_map/Interface0D.h"

%include "../view_map/Interface1D.h"
%template(integrateUnsigned)	integrate<unsigned>;
%template(integrateFloat)	integrate<float>;
%template(integrateDouble)	integrate<double>;
//%template(integrateReal)	integrate<real>;

%rename(getObject) FEdgeInternal::SVertexIterator::operator*;
%rename(FEdgeSVertexIterator) FEdgeInternal::SVertexIterator;
%include "../view_map/Silhouette.h"

%template(ViewShapesContainer) std::vector<ViewShape*>;
%template(ViewEdgesContainer) std::vector<ViewEdge*>;
%template(FEdgesContainer) std::vector<FEdge*>;
%template(ViewVerticesContainer) std::vector<ViewVertex*>;
%template(SVerticesContainer) std::vector<SVertex*>;

%ignore NonTVertex::edges_begin;
%ignore NonTVertex::edges_last;
%ignore NonTVertex::edges_iterator;
%ignore TVertex::edges_begin;
%ignore TVertex::edges_last;
%ignore TVertex::edges_iterator;
%ignore ViewEdge::ViewEdge_iterator;
%ignore ViewEdge::fedge_iterator_begin;
%ignore ViewEdge::fedge_iterator_last;
%ignore ViewEdge::fedge_iterator_end;
%ignore ViewEdge::vertices_begin;
%ignore ViewEdge::vertices_last;
%ignore ViewEdge::vertices_end;
%rename(directedViewEdge) ViewVertex::directedViewEdge; 
//%template(directedViewEdge) std::pair<ViewEdge*,bool>;
%include "../view_map/ViewMap.h"


%feature("director") ViewEdgeInternal::ViewEdgeIterator;
%rename(getObject) ViewVertexInternal::orientedViewEdgeIterator::operator*;
%rename(ViewVertexOrientedViewEdgeIterator) ViewVertexInternal::orientedViewEdgeIterator;
%rename(getObject) ViewEdgeInternal::SVertexIterator::operator*;
%rename(ViewEdgeSVertexIterator) ViewEdgeInternal::SVertexIterator;
%rename(getObject) ViewEdgeInternal::ViewEdgeIterator::operator*;
%rename(ViewEdgeViewEdgeIterator) ViewEdgeInternal::ViewEdgeIterator;
%include "../view_map/ViewMapIterators.h"

%include "../view_map/Interface0D.h"

// SWIG directives in "../view_map/Functions0D.h"
%ignore Functions0D::getFEdges;
%ignore Functions0D::getViewEdges;
%ignore Functions0D::getShapeF0D;
%ignore Functions0D::getOccludersF0D;
%ignore Functions0D::getOccludeeF0D;
%include "../view_map/Functions0D.h"

// SWIG directives in "../view_map/Functions1D.h"
%ignore Functions1D::getOccludeeF1D;
%ignore Functions1D::getOccludersF1D;
%ignore Functions1D::getShapeF1D;
%include "../view_map/Functions1D.h"

// Module parameters
%include "../stroke/Module.h"

%include "../stroke/AdvancedFunctions0D.h"
%include "../stroke/AdvancedFunctions1D.h"

%include "../stroke/ContextFunctions.h"

%rename(getObject) AdjacencyIterator::operator*;
%rename(getObject) ViewEdgeInternal::ViewEdgeIterator::operator*;
%feature("director") ChainingIterator;
%feature("director") ChainSilhouetteIterator;
%feature("director") ChainPredicateIterator;
%include "../stroke/ChainingIterators.h"

%feature("director") UnaryPredicate0D;
%include "../stroke/Predicates0D.h"

%feature("director") UnaryPredicate1D;
%feature("director") BinaryPredicate1D;
%include "../stroke/Predicates1D.h"
%include "../stroke/AdvancedPredicates1D.h"

%rename(getObject) CurveInternal::CurvePointIterator::operator*;
%rename(CurvePointIterator) CurveInternal::CurvePointIterator;
%include "../stroke/CurveIterators.h"

%ignore Curve::points_begin;
%ignore Curve::points_end;
%ignore Curve::vertices_begin;
%ignore Curve::vertices_end;
%include "../stroke/Curve.h"

%ignore Stroke::vertices_begin;
%ignore Stroke::vertices_end;
%include "../stroke/StrokeIterators.h"
%include "../stroke/Stroke.h"

%rename(getObject) StrokeInternal::StrokeVertexIterator::operator*;
%rename(StrokeVertexIterator) StrokeInternal::StrokeVertexIterator;
%include "../stroke/StrokeIterators.h"

%feature("director") StrokeShader;
%template(ShadersContainer) std::vector<StrokeShader*>;
%include "../stroke/StrokeShader.h"

%include "../stroke/BasicStrokeShaders.h"
%include "../stroke/AdvancedStrokeShaders.h"

%ignore Operators::getStrokesSet;
%ignore Operators::reset;
%include "../stroke/Operators.h"

// Canvas.h
%include "../stroke/Canvas.h"

// Cast functions
%include "../system/Cast.h"
%template(castToSVertex)	Cast::cast<Interface0D, SVertex>;
%template(castToViewVertex)	Cast::cast<Interface0D, ViewVertex>;
%template(castToTVertex)	Cast::cast<Interface0D, TVertex>;
%template(castToCurvePoint)	Cast::cast<Interface0D, CurvePoint>;
%template(castToStrokeVertex)	Cast::cast<Interface0D, StrokeVertex>;
%template(castToNonTVertex)	Cast::cast<Interface0D, NonTVertex>;
%template(castToFEdge)		Cast::cast<Interface1D, FEdge>;
%template(castToViewEdge)	Cast::cast<Interface1D, ViewEdge>;
%template(castToStroke)		Cast::cast<Interface1D, Stroke>;
%template(castToChain)		Cast::cast<Interface1D, Chain>;
