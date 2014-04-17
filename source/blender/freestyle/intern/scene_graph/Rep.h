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

#ifndef __FREESTYLE_REP_H__
#define __FREESTYLE_REP_H__

/** \file blender/freestyle/intern/scene_graph/Rep.h
 *  \ingroup freestyle
 *  \brief Base class for all shapes. Inherits from BasicObjects for references counter management (addRef, release).
 *  \author Stephane Grabli
 *  \date 25/01/2002
 */

#include "FrsMaterial.h"
#include "SceneVisitor.h"

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"

#include "../system/BaseObject.h"
#include "../system/Id.h"
#include "../system/Precision.h"

namespace Freestyle {

using namespace Geometry;

class Rep : public BaseObject
{
public:
	inline Rep() : BaseObject()
	{
		_Id = 0;
		_FrsMaterial = 0;
	}

	inline Rep(const Rep& iBrother) : BaseObject()
	{
		_Id = iBrother._Id;
		_Name = iBrother._Name;
		if (0 == iBrother._FrsMaterial)
			_FrsMaterial = 0;
		else
			_FrsMaterial = new FrsMaterial(*(iBrother._FrsMaterial));

		_BBox = iBrother.bbox();
	}

	inline void swap(Rep& ioOther)
	{
		std::swap(_BBox, ioOther._BBox);
		std::swap(_Id, ioOther._Id);
		std::swap(_Name, ioOther._Name);
		std::swap(_FrsMaterial, ioOther._FrsMaterial);
	}

	Rep& operator=(const Rep& iBrother)
	{
		if (&iBrother != this) {
			_Id = iBrother._Id;
			_Name = iBrother._Name;
			if (0 == iBrother._FrsMaterial) {
				_FrsMaterial = 0;
			}
			else {
				if (_FrsMaterial == 0) {
					_FrsMaterial = new FrsMaterial(*iBrother._FrsMaterial);
				}
				else {
					(*_FrsMaterial) = (*(iBrother._FrsMaterial));
				}
				_BBox = iBrother.bbox();
			}
		}
		return *this;
	}

	virtual ~Rep() 
	{
		if (0 != _FrsMaterial) {
			delete _FrsMaterial;
			_FrsMaterial = 0;
		}
	}

	/*! Accept the corresponding visitor
	 *  Must be overload by inherited classes
	 */
	virtual void accept(SceneVisitor& v)
	{
		if (_FrsMaterial)
			v.visitFrsMaterial(*_FrsMaterial);
		v.visitRep(*this);
	}

	/*! Computes the rep bounding box.
	 *  Each Inherited rep must compute its bbox depending on the way the data are stored. So, each inherited class
	 *  must overload this method
	 */
	virtual void ComputeBBox() = 0;

	/*! Returns the rep bounding box */
	virtual const BBox<Vec3r>& bbox() const
	{
		return _BBox;
	}

	inline Id getId() const
	{
		return _Id;
	}

	inline const string& getName() const
	{
		return _Name;
	}

	inline const FrsMaterial *frs_material() const
	{
		return _FrsMaterial;
	}

	/*! Sets the Rep bounding box */
	virtual void setBBox(const BBox<Vec3r>& iBox)
	{
		_BBox = iBox;
	}

	inline void setId(const Id& id)
	{
		_Id = id;
	}

	inline void setName(const string& name)
	{
		_Name = name;
	}

	inline void setFrsMaterial(const FrsMaterial& iMaterial) 
	{
		_FrsMaterial = new FrsMaterial(iMaterial);
	}

private:
	BBox<Vec3r> _BBox;
	Id _Id;
	string _Name;
	FrsMaterial *_FrsMaterial;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_REP_H__
