//
//  Filename         : StrokeLayer.h
//  Author           : Stephane Grabli
//  Purpose          : Class to define a layer of strokes.
//  Date of creation : 18/12/2002
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

#ifndef  STROKELAYER_H
# define STROKELAYER_H

# include <deque>

class Stroke;
class StrokeRenderer;
class StrokeLayer 
{
public:
  typedef std::deque<Stroke*> stroke_container;

protected:
  stroke_container _strokes;
public:
  StrokeLayer() {}
  StrokeLayer(const stroke_container& iStrokes)
  {
    _strokes = iStrokes;
  }
  StrokeLayer(const StrokeLayer& iBrother)
  {
    _strokes = iBrother._strokes;
  }
  virtual ~StrokeLayer() ;

  /*! Render method */
  void ScaleThickness(float iFactor);
  void Render(const StrokeRenderer *iRenderer );
  void RenderBasic(const StrokeRenderer *iRenderer );

  /*! clears the layer */
  void clear() ;

  /*! accessors */
  inline stroke_container::iterator strokes_begin() {return _strokes.begin();}
  inline stroke_container::iterator strokes_end() {return _strokes.end();}
  inline int strokes_size() const {return _strokes.size();}
  inline bool empty() const {return _strokes.empty();}

  /*! modifiers */
  inline void setStrokes(stroke_container& iStrokes) {_strokes = iStrokes;}
  inline void AddStroke(Stroke *iStroke) {_strokes.push_back(iStroke);}
  
};

#endif // STROKELAYER_H
