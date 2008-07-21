//
//  Filename         : NodeDrawingStyle.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define a Drawing Style to be applied 
//                     to the underlying children. Inherits from NodeGroup.
//  Date of creation : 06/02/2002
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

#ifndef  NODEDRAWINGSTYLE_H
# define NODEDRAWINGSTYLE_H

# include "../system/FreestyleConfig.h"
# include "NodeGroup.h"
# include "DrawingStyle.h"

class LIB_SCENE_GRAPH_EXPORT NodeDrawingStyle : public NodeGroup
{
public:

  inline NodeDrawingStyle() : NodeGroup() {}
  virtual ~NodeDrawingStyle() {}

  inline const DrawingStyle& drawingStyle() const { return _DrawingStyle; }
  inline void setDrawingStyle(const DrawingStyle& iDrawingStyle) { _DrawingStyle = iDrawingStyle; }

  /*! Sets the style. Must be one of FILLED, LINES, POINTS, INVISIBLE. */
  inline void setStyle(const DrawingStyle::STYLE iStyle) { _DrawingStyle.setStyle(iStyle); }
  /*! Sets the line width in the LINES style case */
  inline void setLineWidth(const float iLineWidth) { _DrawingStyle.setLineWidth(iLineWidth); }
  /*! Sets the Point size in the POINTS style case */
  inline void setPointSize(const float iPointSize) { _DrawingStyle.setPointSize(iPointSize); }
  /*! Enables or disables the lighting. TRUE = enable */
  inline void setLightingEnabled(const bool iEnableLighting) { _DrawingStyle.setLightingEnabled(iEnableLighting); }

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor& v);

  /*! accessors */
  inline DrawingStyle::STYLE style() const {return _DrawingStyle.style();}
  inline float lineWidth() const {return _DrawingStyle.lineWidth();}
  inline float pointSize() const {return _DrawingStyle.pointSize();}
  inline bool lightingEnabled() const {return _DrawingStyle.lightingEnabled();}

private:
  DrawingStyle _DrawingStyle;
};

#endif // NODEDRAWINGSTYLE_H
