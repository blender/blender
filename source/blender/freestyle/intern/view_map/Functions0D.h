/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_FUNCTIONS_0D_H__
#define __FREESTYLE_FUNCTIONS_0D_H__

/** \file blender/freestyle/intern/view_map/Functions0D.h
 *  \ingroup freestyle
 *  \brief Functions taking 0D input
 *  \author Stephane Grabli
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include <set>
#include <vector>

#include "Interface0D.h"

#include "../geometry/Geom.h"

#include "../python/Director.h"

#include "../scene_graph/FrsMaterial.h"

#include "../system/Exception.h"
#include "../system/Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class FEdge;
class ViewEdge;
class SShape;

using namespace Geometry;

//
// UnaryFunction0D (base class for functions in 0D)
//
///////////////////////////////////////////////////////////

/*! Base class for Unary Functions (functors) working on Interface0DIterator.
 *  A unary function will be used by calling its operator() on an Interface0DIterator.
 *  \attention In the scripting language, there exists several prototypes depending on the returned value type.
 *  For example, you would inherit from a UnaryFunction0DDouble if you wish to define a function that returns a double.
 *  The different existing prototypes are:
 *    - UnaryFunction0DVoid
 *    - UnaryFunction0DUnsigned
 *    - UnaryFunction0DReal
 *    - UnaryFunction0DFloat
 *    - UnaryFunction0DDouble
 *    - UnaryFunction0DVec2f
 *    - UnaryFunction0DVec3f
 */
template <class T>
class /*LIB_VIEW_MAP_EXPORT*/ UnaryFunction0D
{
public:
	T result;
	PyObject *py_uf0D;

	/*! The type of the value returned by the functor. */
	typedef T ReturnedValueType;

	/*! Default constructor. */
	UnaryFunction0D()
	{
		py_uf0D = NULL;
	}

	/*! Destructor; */
	virtual ~UnaryFunction0D() {}

	/*! Returns the string "UnaryFunction0D" */
	virtual string getName() const
	{
		return "UnaryFunction0D";
	}

	/*! The operator ().
	 *  \param iter
	 *    An Interface0DIterator pointing onto the point at which we wish to evaluate the function.
	 *  \return the result of the function of type T.
	 */
	virtual int operator()(Interface0DIterator& iter)
	{
		return Director_BPy_UnaryFunction0D___call__(this, py_uf0D, iter);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:UnaryFunction0D")
#endif
};

#ifdef SWIG
%feature("director")  UnaryFunction0D<void>;
%feature("director")  UnaryFunction0D<unsigned>;
%feature("director")  UnaryFunction0D<float>;
%feature("director")  UnaryFunction0D<double>;
%feature("director")  UnaryFunction0D<Vec2f>;
%feature("director")  UnaryFunction0D<Vec3f>;
%feature("director")  UnaryFunction0D<Id>;

%template(UnaryFunction0DVoid)             UnaryFunction0D<void>;
%template(UnaryFunction0DUnsigned)         UnaryFunction0D<unsigned>;
%template(UnaryFunction0DFloat)            UnaryFunction0D<float>;
%template(UnaryFunction0DDouble)           UnaryFunction0D<double>;
%template(UnaryFunction0DVec2f)            UnaryFunction0D<Vec2f>;
%template(UnaryFunction0DVec3f)            UnaryFunction0D<Vec3f>;
%template(UnaryFunction0DId)               UnaryFunction0D<Id>;
%template(UnaryFunction0DViewShape)        UnaryFunction0D<ViewShape*>;
%template(UnaryFunction0DVectorViewShape)  UnaryFunction0D<std::vector<ViewShape*> >;
#endif // SWIG

//
// Functions definitions
//
///////////////////////////////////////////////////////////
class ViewShape;

namespace Functions0D {

// GetXF0D
/*! Returns the X 3D coordinate of an Interface0D. */
class LIB_VIEW_MAP_EXPORT GetXF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "GetXF0D" */
	string getName() const
	{
		return "GetXF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter->getX();
		return 0;
	}
};

// GetYF0D
/*! Returns the Y 3D coordinate of an Interface0D. */
class LIB_VIEW_MAP_EXPORT GetYF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "GetYF0D" */
	string getName() const
	{
		return "GetYF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter->getY();
		return 0;
	}
};

// GetZF0D
/*! Returns the Z 3D coordinate of an Interface0D. */
class LIB_VIEW_MAP_EXPORT GetZF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "GetZF0D" */
	string getName() const
	{
		return "GetZF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter->getZ();
		return 0;
	}
};

// GetProjectedXF0D
/*! Returns the X 3D projected coordinate of an Interface0D. */
class LIB_VIEW_MAP_EXPORT GetProjectedXF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "GetProjectedXF0D" */
	string getName() const
	{
		return "GetProjectedXF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter->getProjectedX();
		return 0;
	}
};

// GetProjectedYF0D
/*! Returns the Y projected 3D coordinate of an Interface0D. */
class LIB_VIEW_MAP_EXPORT GetProjectedYF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "GetProjectedYF0D" */
	string getName() const
	{
		return "GetProjectedYF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter->getProjectedY();
		return 0;
	}
};

// GetProjectedZF0D
/*! Returns the Z projected 3D coordinate of an Interface0D. */
class LIB_VIEW_MAP_EXPORT GetProjectedZF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "GetProjectedZF0D" */
	string getName() const
	{
		return "GetProjectedZF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter->getProjectedZ();
		return 0;
	}
};

// GetCurvilinearAbscissaF0D
/*! Returns the curvilinear abscissa of an Interface0D in the context of its 1D element. */
class LIB_VIEW_MAP_EXPORT GetCurvilinearAbscissaF0D : public UnaryFunction0D<float>
{
public:
	/*! Returns the string "GetCurvilinearAbscissaF0D" */
	string getName() const
	{
		return "GetCurvilinearAbscissaF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter.t();
		return 0;
	}
};

// GetParameterF0D
/*! Returns the parameter of an Interface0D in the context of its 1D element. */
class LIB_VIEW_MAP_EXPORT GetParameterF0D : public UnaryFunction0D<float>
{
public:
	/*! Returns the string "GetCurvilinearAbscissaF0D" */
	string getName() const
	{
		return "GetParameterF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter)
	{
		result = iter.u();
		return 0;
	}
};

// VertexOrientation2DF0D
/*! Returns a Vec2r giving the 2D oriented tangent to the 1D element to which the Interface0DIterator& belongs to and
 *  evaluated at the Interface0D pointed by this Interface0DIterator&.
 */
class LIB_VIEW_MAP_EXPORT VertexOrientation2DF0D : public UnaryFunction0D<Vec2f>
{
public:
	/*! Returns the string "VertexOrientation2DF0D" */
	string getName() const
	{
		return "VertexOrientation2DF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// VertexOrientation3DF0D
/*! Returns a Vec3r giving the 3D oriented tangent to the 1D element to which the Interface0DIterator& belongs to and
 *  evaluated at the Interface0D pointed by this Interface0DIterator&.
 */
class LIB_VIEW_MAP_EXPORT VertexOrientation3DF0D : public UnaryFunction0D<Vec3f>
{
public:
	/*! Returns the string "VertexOrientation3DF0D" */
	string getName() const
	{
		return "VertexOrientation3DF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// Curvature2DAngleF0D
/*! Returns a real giving the 2D curvature (as an angle) of the 1D element to which the Interface0DIterator&
 *  belongs to and evaluated at the Interface0D pointed by this Interface0DIterator&.
 */
class LIB_VIEW_MAP_EXPORT Curvature2DAngleF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "Curvature2DAngleF0D" */
	string getName() const
	{
		return "Curvature2DAngleF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// ZDiscontinuity
/*! Returns a real giving the distance between and Interface0D and the shape that lies behind (occludee).
 *  This distance is evaluated in the camera space and normalized between 0 and 1. Therefore, if no object is occluded
 *  by the shape to which the Interface0D belongs to, 1 is returned.
 */
class LIB_VIEW_MAP_EXPORT ZDiscontinuityF0D : public UnaryFunction0D<real>
{
public:
	/*! Returns the string "ZDiscontinuityF0D" */
	string getName() const
	{
		return "ZDiscontinuityF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// Normal2DF0D
/*! Returns a Vec2f giving the normalized 2D normal to the 1D element to which the Interface0DIterator& belongs to and
 *  evaluated at the Interface0D pointed by this Interface0DIterator&.
 */
class LIB_VIEW_MAP_EXPORT Normal2DF0D : public UnaryFunction0D<Vec2f>
{
public:
	/*! Returns the string "Normal2DF0D" */
	string getName() const
	{
		return "Normal2DF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// MaterialF0D
/*! Returns the material of the object evaluated at the Interface0D.
 *  This evaluation can be ambiguous (in the case of a TVertex for example.
 *  This functor tries to remove this ambiguity using the context offered by the 1D element to which the
 *  Interface0DIterator& belongs to and by arbitrary choosing the material of the face that lies on its left when
 *  following the 1D element if there are two different materials on each side of the point.
 *  However, there still can be problematic cases, and the user willing to deal with this cases in a specific way
 *  should implement its own getMaterial functor.
 */
class LIB_VIEW_MAP_EXPORT MaterialF0D : public UnaryFunction0D<FrsMaterial>
{
public:
	/*! Returns the string "MaterialF0D" */
	string getName() const
	{
		return "MaterialF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// ShapeIdF0D
/*! Returns the Id of the Shape the Interface0D belongs to.
 *  This evaluation can be ambiguous (in the case of a TVertex for example).
 *  This functor tries to remove this ambiguity using the context offered by the 1D element to which the
 *  Interface0DIterator& belongs to.
 *  However, there still can be problematic cases, and the user willing to deal with this cases in a specific way
 *  should implement its own getShapeIdF0D functor.
 */
class LIB_VIEW_MAP_EXPORT ShapeIdF0D : public UnaryFunction0D<Id>
{
public:
	/*! Returns the string "ShapeIdF0D" */
	string getName() const
	{
		return "ShapeIdF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// QiF0D
/*! Returns the quantitative invisibility of this Interface0D.
*  This evaluation can be ambiguous (in the case of a TVertex for example).
*  This functor tries to remove this ambiguity using the context offered by the 1D element to which the
*  Interface0DIterator& belongs to.
*  However, there still can be problematic cases, and the user willing to deal with this cases in a specific way
*  should implement its own getQIF0D functor.
*/
class LIB_VIEW_MAP_EXPORT QuantitativeInvisibilityF0D : public UnaryFunction0D<unsigned int>
{
public:
	/*! Returns the string "QuantitativeInvisibilityF0D" */
	string getName() const
	{
		return "QuantitativeInvisibilityF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// CurveNatureF0D
/*! Returns the Nature::EdgeNature of the 1D element the Interface0DIterator& belongs to. */
class LIB_VIEW_MAP_EXPORT CurveNatureF0D : public UnaryFunction0D<Nature::EdgeNature>
{
public:
	/*! Returns the string "QuantitativeInvisibilityF0D" */
	string getName() const
	{
		return "CurveNatureF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// GetShapeF0D
/*! Returns the ViewShape* containing the Interface0D */
class LIB_VIEW_MAP_EXPORT GetShapeF0D : public UnaryFunction0D< ViewShape*>
{
public:
	/*! Returns the string "GetShapeF0D" */
	string getName() const
	{
		return "GetShapeF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// GetOccludersF0D
/*! Returns a vector containing the ViewShape* occluding the Interface0D */
class LIB_VIEW_MAP_EXPORT GetOccludersF0D : public UnaryFunction0D< std::vector<ViewShape*> >
{
public:
	/*! Returns the string "GetOccludersF0D" */
	string getName() const
	{
		return "GetOccludersF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// GetOccludeeF0D
/*! Returns the ViewShape* "occluded" by the Interface0D */
class LIB_VIEW_MAP_EXPORT GetOccludeeF0D: public UnaryFunction0D< ViewShape*>
{
public:
	/*! Returns the string "GetOccludeeF0D" */
	string getName() const
	{
		return "GetOccludeeF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};


/////////////////////////// Internal ////////////////////////////

// getFEdge
LIB_VIEW_MAP_EXPORT
FEdge *getFEdge(Interface0D& it1, Interface0D& it2);

// getFEdges
LIB_VIEW_MAP_EXPORT
void getFEdges(Interface0DIterator& it, FEdge *&fe1, FEdge *&fe2);

// getViewEdges
LIB_VIEW_MAP_EXPORT
void getViewEdges(Interface0DIterator& it, ViewEdge *&ve1, ViewEdge *&ve2);

// getShapeF0D
LIB_VIEW_MAP_EXPORT
ViewShape *getShapeF0D(Interface0DIterator& it);

// getOccludersF0D
LIB_VIEW_MAP_EXPORT
void getOccludersF0D(Interface0DIterator& it, std::set<ViewShape*>& oOccluders);

// getOccludeeF0D
LIB_VIEW_MAP_EXPORT
ViewShape *getOccludeeF0D(Interface0DIterator& it);

} // end of namespace Functions0D

} /* namespace Freestyle */

#endif // __FREESTYLE_FUNCTIONS_0D_H__
