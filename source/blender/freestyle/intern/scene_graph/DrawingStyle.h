//
//  Filename         : DrawingStyle.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define the drawing style of a node
//  Date of creation : 10/10/2002
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

#ifndef  DRAWINGSTYLE_H
# define DRAWINGSTYLE_H

class DrawingStyle
{
public:
  enum STYLE
  {FILLED, LINES, POINTS, INVISIBLE};
  
  inline DrawingStyle() {Style = FILLED; LineWidth = 2.f; PointSize = 2.f; LightingEnabled = true;}
  inline explicit DrawingStyle(const DrawingStyle& iBrother);
  
  virtual ~DrawingStyle() {}

  /*! operators */
  inline DrawingStyle& operator=(const DrawingStyle& ds);
  
  inline void setStyle(const STYLE iStyle) {Style = iStyle;}
  inline void setLineWidth(const float iLineWidth) {LineWidth = iLineWidth;}
  inline void setPointSize(const float iPointSize) {PointSize = iPointSize;}
  inline void setLightingEnabled(const bool on) {LightingEnabled = on;}

  inline STYLE style() const {return Style;}
  inline float lineWidth() const {return LineWidth;}
  inline float pointSize() const {return PointSize;}
  inline bool lightingEnabled() const {return LightingEnabled;}

private:
  STYLE Style;
  float LineWidth;
  float PointSize;
  bool LightingEnabled;
};

DrawingStyle::DrawingStyle(const DrawingStyle& iBrother)
{
  Style           = iBrother.Style;
  LineWidth       = iBrother.LineWidth;
  PointSize       = iBrother.PointSize;
  LightingEnabled = iBrother.LightingEnabled;
}

DrawingStyle& DrawingStyle::operator=(const DrawingStyle& ds)
{
  Style           = ds.Style;
  LineWidth       = ds.LineWidth;
  PointSize       = ds.PointSize;
  LightingEnabled = ds.LightingEnabled;

  return *this;
}

#endif // DRAWINGSTYLE_H
