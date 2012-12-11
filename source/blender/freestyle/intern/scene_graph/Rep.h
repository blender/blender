//
//  Filename         : Rep.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Base class for all shapes. Inherits from BasicObjects 
//                     for references counter management (addRef, release).
//  Date of creation : 25/01/2002
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

#ifndef  REP_H
# define REP_H

# include "../system/BaseObject.h"
# include "SceneVisitor.h"
# include "../geometry/BBox.h"
# include "../geometry/Geom.h"
# include "../system/Precision.h"
# include "FrsMaterial.h"
# include "../system/Id.h"

using namespace Geometry;

class LIB_SCENE_GRAPH_EXPORT Rep : public BaseObject
{
public:
  
  inline Rep() : BaseObject() {_Id = 0; _FrsMaterial=0;}
  inline Rep(const Rep& iBrother)
    : BaseObject()
  {
    _Id = iBrother._Id;
    _Name = iBrother._Name;
    if(0 == iBrother._FrsMaterial)
      _FrsMaterial = 0;
    else
      _FrsMaterial = new FrsMaterial(*(iBrother._FrsMaterial));

    _BBox = iBrother.bbox();
  }
	inline void swap(Rep& ioOther){
		std::swap(_BBox,ioOther._BBox);
		std::swap(_Id, ioOther._Id);
		std::swap(_Name, ioOther._Name);
		std::swap(_FrsMaterial,ioOther._FrsMaterial);
	}
	Rep& operator=(const Rep& iBrother){
		if(&iBrother != this){
			_Id = iBrother._Id;
			_Name = iBrother._Name;
			if(0 == iBrother._FrsMaterial)
				_FrsMaterial = 0;
			else{
				if(_FrsMaterial == 0){
					_FrsMaterial = new FrsMaterial(*iBrother._FrsMaterial);
				}else{
					(*_FrsMaterial)=(*(iBrother._FrsMaterial));
				}
				_BBox = iBrother.bbox();
			}
		}
		return *this;
	}
  virtual ~Rep() 
  {
    if(0 != _FrsMaterial)
    {
      delete _FrsMaterial;
      _FrsMaterial = 0;
    }
  }

  /*! Accept the corresponding visitor
   *  Must be overload by 
   *  inherited classes
   */
  virtual void accept(SceneVisitor& v) {
    if(_FrsMaterial)
      v.visitFrsMaterial(*_FrsMaterial);
    v.visitRep(*this);
  }

  /*! Computes the rep bounding box.
   *  Each Inherited rep must compute 
   *  its bbox depending on the way the data
   *  are stored. So, each inherited class 
   *  must overload this method
   */
  virtual void ComputeBBox() = 0;
  
  /*! Returns the rep bounding box */
  virtual const BBox<Vec3r>& bbox() const {return _BBox;}
  inline Id getId() const {return _Id;}
  inline const string& getName() const {return _Name;}
  inline const FrsMaterial * frs_material() const {return _FrsMaterial;}

  /*! Sets the Rep bounding box */
  virtual void setBBox(const BBox<Vec3r>& iBox) {_BBox = iBox;}
  inline void setId(const Id& id) {_Id = id;}
  inline void setName(const string& name) {_Name = name;}
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

#endif // REP_H
