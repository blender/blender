//
//  Filename         : Modifiers.h
//  Author           : Stephane Grabli
//  Purpose          : modifiers...
//  Date of creation : 05/01/2003
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

#ifndef  MODIFIERS_H
# define MODIFIERS_H

# include "TimeStamp.h"

/* ----------------------------------------- *
 *                                           *
 *              modifiers                 *
 *                                           *
 * ----------------------------------------- */
/*! Base class for modifiers.
 *  Modifiers are used in the
 *  Operators in order to "mark"
 *  the processed Interface1D.
 */
template<class Edge>
struct EdgeModifier : public unary_function<Edge,void>
{
  /*! Default construction */
  EdgeModifier() : unary_function<Edge,void>() {}
  /*! the () operator */
  virtual void operator()(Edge& iEdge) {}
};

/*! Modifier that sets the time stamp
 *  of an Interface1D to the time stamp
 *  of the system.
 */
template<class Edge>
struct TimestampModifier : public EdgeModifier<Edge>
{
  /*! Default constructor */
  TimestampModifier() : EdgeModifier<Edge>() {}
  /*! The () operator. */
  virtual void operator()(Edge& iEdge) 
  {
    TimeStamp *timestamp = TimeStamp::instance();
    iEdge.setTimeStamp(timestamp->getTimeStamp());
  }
};

#endif // MODIFIERS_H
