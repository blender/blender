#ifndef WXEDGEBUILDER_H
# define WXEDGEBUILDER_H

//
//  Filename         : WSBuilder.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class inherited from WingedEdgeBuilder and 
//                     designed to build a WX (WingedEdge + extended info(silhouette etc...)) 
//                     structure from a polygonal model
//  Date of creation : 28/05/03
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

# include "WingedEdgeBuilder.h"
# include "../scene_graph/IndexedFaceSet.h"

class LIB_WINGED_EDGE_EXPORT WXEdgeBuilder : public WingedEdgeBuilder
{
public:
	WXEdgeBuilder() : WingedEdgeBuilder() {}
  virtual ~WXEdgeBuilder() {}
  VISIT_DECL(IndexedFaceSet)

protected:
  virtual void buildWVertices(WShape& shape,
			      const real *vertices,
			      unsigned vsize);
};

#endif // WXEDGEBUILDER_H
