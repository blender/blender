/* SPDX-FileCopyrightText: 2012-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "TextStrokeRenderer.h"
#include "Canvas.h"
#include "StrokeIterators.h"

namespace Freestyle {

TextStrokeRenderer::TextStrokeRenderer(const char *iFileName)
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

} /* namespace Freestyle */
