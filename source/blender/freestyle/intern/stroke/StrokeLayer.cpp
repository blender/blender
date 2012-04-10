
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

#include "Stroke.h"
#include "StrokeLayer.h"
#include "Canvas.h"

StrokeLayer::~StrokeLayer()
{
  clear();
}

void StrokeLayer::ScaleThickness(float iFactor)
{
  for(StrokeLayer::stroke_container::iterator s=_strokes.begin(), send=_strokes.end();
      s!=send;
      ++s){
      (*s)->ScaleThickness(iFactor);
  } 
}

void StrokeLayer::Render(const StrokeRenderer *iRenderer )
{
  for(StrokeLayer::stroke_container::iterator s=_strokes.begin(), send=_strokes.end();
      s!=send;
      ++s){
      (*s)->Render(iRenderer);
  } 
}

void StrokeLayer::RenderBasic(const StrokeRenderer *iRenderer )
{
  for(StrokeLayer::stroke_container::iterator s=_strokes.begin(), send=_strokes.end();
      s!=send;
      ++s){
      (*s)->RenderBasic(iRenderer);
      }
}
void StrokeLayer::clear()
{
  for(stroke_container::iterator s=_strokes.begin(), send=_strokes.end();
      s!=send;
      ++s)
    delete *s;
  _strokes.clear();
}
