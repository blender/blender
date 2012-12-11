//
//  Filename         : Chain.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define a chain of viewedges.
//  Date of creation : 09/01/2003
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

#ifndef  CHAIN_H
# define CHAIN_H

# include "../view_map/ViewMap.h"
# include "Curve.h"
/*! Class to represent a 1D elements issued
 *  from the chaining process.
 *  A Chain is the last step before the Stroke and
 *  is used in the Splitting and Creation processes.
 */
class Chain : public Curve
{
protected:
  // tmp
  Id * _splittingId;
  FEdge *_fedgeB; // the last FEdge of the ViewEdge passed to the last call for push_viewedge_back().
public:
  /*! Defult constructor. */
  Chain() : Curve() {_splittingId=0;_fedgeB=0;}
  /*! Builds a chain from its Id. */
  Chain(const Id& id) : Curve(id) {_splittingId=0;_fedgeB=0;}
  /*! Copy Constructor */
  Chain(const Chain& iBrother) : Curve(iBrother) {_splittingId=iBrother._splittingId;_fedgeB=iBrother._fedgeB;}
  /*! Destructor. */
  virtual ~Chain() {
    // only the last splitted deletes this id
    if(_splittingId){
      if(*_splittingId == _Id)
        delete _splittingId;
    }
  }
  /*! Returns the string "Chain" */
  virtual string getExactTypeName() const {
    return "Chain";
  }

  /*! Adds a ViewEdge at the end of the chain
   *  \param iViewEdge
   *    The ViewEdge that must be added.
   *  \param orientation
   *    The orientation with which this ViewEdge
   *    must be processed.
   */
  void push_viewedge_back(ViewEdge *iViewEdge, bool orientation) ;
  /*! Adds a ViewEdge at the beginning of the chain
   *  \param iViewEdge
   *    The ViewEdge that must be added.
   *  \param orientation
   *    The orientation with which this ViewEdge
   *    must be processed.
   */
  void push_viewedge_front(ViewEdge *iViewEdge, bool orientation) ;

  inline void setSplittingId(Id * sid){_splittingId = sid;}
  inline Id* getSplittingId() {return _splittingId;}
};

#endif // CHAIN_H
