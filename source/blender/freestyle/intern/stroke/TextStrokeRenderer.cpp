/*
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
 */

#include "TextStrokeRenderer.h"
#include "Canvas.h"
#include "StrokeIterators.h"

namespace Freestyle {

TextStrokeRenderer::TextStrokeRenderer(const char *iFileName) : StrokeRenderer()
{
  if (!iFileName) {
    iFileName = "freestyle.txt";
  }
  // open the stream:
  _ofstream.open(iFileName, ios::out);
  if (!_ofstream.is_open()) {
    cerr << "couldn't open the output file " << iFileName << endl;
  }
  _ofstream << "%!FREESTYLE" << endl;
  _ofstream << "%Creator: Freestyle (http://artis.imag.fr/Software/Freestyle)" << endl;
  // Bounding box
  _ofstream << 0 << " " << 0 << " " << Canvas::getInstance()->width() << " "
            << Canvas::getInstance()->height() << endl;
  _ofstream << "%u x y z tleft tright r g b ..." << endl;
}

TextStrokeRenderer::~TextStrokeRenderer()
{
  Close();
}

void TextStrokeRenderer::RenderStrokeRep(StrokeRep *iStrokeRep) const
{
  RenderStrokeRepBasic(iStrokeRep);
}

void TextStrokeRenderer::RenderStrokeRepBasic(StrokeRep *iStrokeRep) const
{
  Stroke *stroke = iStrokeRep->getStroke();
  if (!stroke) {
    cerr << "no stroke associated with Rep" << endl;
    return;
  }

  StrokeInternal::StrokeVertexIterator v = stroke->strokeVerticesBegin();
  StrokeAttribute att;
  while (!v.isEnd()) {
    att = v->attribute();
    _ofstream << v->u() << " " << v->getProjectedX() << " " << v->getProjectedY() << " "
              << v->getProjectedZ() << " " << att.getThicknessL() << " " << att.getThicknessR()
              << " " << att.getColorR() << " " << att.getColorG() << " " << att.getColorB() << " ";
    ++v;
  }
  _ofstream << endl;
}

void TextStrokeRenderer::Close()
{
  if (_ofstream.is_open()) {
    _ofstream.close();
  }
}

} /* namespace Freestyle */
