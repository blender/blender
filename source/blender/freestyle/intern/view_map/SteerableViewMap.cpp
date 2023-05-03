/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Convenient access to the steerable ViewMap to which any element of the ViewMap belongs
 * to.
 */

#include <sstream>

#include "Silhouette.h"
#include "SteerableViewMap.h"

#include "../geometry/Geom.h"

#include "../image/Image.h"
#include "../image/ImagePyramid.h"

#include "BLI_math.h"
#include "BLI_sys_types.h"

#include "BKE_global.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace Freestyle {

using namespace Geometry;

SteerableViewMap::SteerableViewMap(uint nbOrientations)
{
  _nbOrientations = nbOrientations;
  _bound = cos(M_PI / float(_nbOrientations));
  for (uint i = 0; i < _nbOrientations; ++i) {
    _directions.emplace_back(cos(float(i) * M_PI / float(_nbOrientations)),
                             sin(float(i) * M_PI / float(_nbOrientations)));
  }
  Build();
}

void SteerableViewMap::Build()
{
  _imagesPyramids =
      new ImagePyramid *[_nbOrientations + 1];  // one more map to store the complete visible VM
  memset((_imagesPyramids), 0, (_nbOrientations + 1) * sizeof(ImagePyramid *));
}

SteerableViewMap::SteerableViewMap(const SteerableViewMap &iBrother)
{
  _nbOrientations = iBrother._nbOrientations;
  uint i;
  _bound = iBrother._bound;
  _directions = iBrother._directions;
  _mapping = iBrother._mapping;
  _imagesPyramids =
      new ImagePyramid *[_nbOrientations + 1];  // one more map to store the complete visible VM
  for (i = 0; i <= _nbOrientations; ++i) {
    _imagesPyramids[i] = new GaussianPyramid(
        *(dynamic_cast<GaussianPyramid *>(iBrother._imagesPyramids[i])));
  }
}

SteerableViewMap::~SteerableViewMap()
{
  Clear();
}

void SteerableViewMap::Clear()
{
  uint i;
  if (_imagesPyramids) {
    for (i = 0; i <= _nbOrientations; ++i) {
      if (_imagesPyramids[i]) {
        delete (_imagesPyramids)[i];
      }
    }
    delete[] _imagesPyramids;
    _imagesPyramids = nullptr;
  }
  if (!_mapping.empty()) {
    for (map<uint, double *>::iterator m = _mapping.begin(), mend = _mapping.end(); m != mend; ++m)
    {
      delete[](*m).second;
    }
    _mapping.clear();
  }
}

void SteerableViewMap::Reset()
{
  Clear();
  Build();
}

double SteerableViewMap::ComputeWeight(const Vec2d &dir, uint i)
{
  double dotp = fabs(dir * _directions[i]);
  if (dotp < _bound) {
    return 0.0;
  }
  if (dotp > 1.0) {
    dotp = 1.0;
  }

  return cos(float(_nbOrientations) / 2.0 * acos(dotp));
}

double *SteerableViewMap::AddFEdge(FEdge *iFEdge)
{
  uint i;
  uint id = iFEdge->getId().getFirst();
  map<uint, double *>::iterator o = _mapping.find(id);
  if (o != _mapping.end()) {
    return (*o).second;
  }
  double *res = new double[_nbOrientations];
  for (i = 0; i < _nbOrientations; ++i) {
    res[i] = 0.0;
  }
  Vec3r o2d3 = iFEdge->orientation2d();
  Vec2r o2d2(o2d3.x(), o2d3.y());
  real norm = o2d2.norm();
  if (norm < 1.0e-6) {
    return res;
  }
  o2d2 /= norm;

  for (i = 0; i < _nbOrientations; ++i) {
    res[i] = ComputeWeight(o2d2, i);
  }
  _mapping[id] = res;
  return res;
}

uint SteerableViewMap::getSVMNumber(Vec2f dir)
{
  // soc unsigned res = 0;
  real norm = dir.norm();
  if (norm < 1.0e-6) {
    return _nbOrientations + 1;
  }
  dir /= norm;
  double maxw = 0.0f;
  uint winner = _nbOrientations + 1;
  for (uint i = 0; i < _nbOrientations; ++i) {
    double w = ComputeWeight(dir, i);
    if (w > maxw) {
      maxw = w;
      winner = i;
    }
  }
  return winner;
}

uint SteerableViewMap::getSVMNumber(uint id)
{
  map<uint, double *>::iterator o = _mapping.find(id);
  if (o != _mapping.end()) {
    double *wvalues = (*o).second;
    double maxw = 0.0;
    uint winner = _nbOrientations + 1;
    for (uint i = 0; i < _nbOrientations; ++i) {
      double w = wvalues[i];
      if (w > maxw) {
        maxw = w;
        winner = i;
      }
    }
    return winner;
  }
  return _nbOrientations + 1;
}

void SteerableViewMap::buildImagesPyramids(GrayImage **steerableBases,
                                           bool copy,
                                           uint iNbLevels,
                                           float iSigma)
{
  for (uint i = 0; i <= _nbOrientations; ++i) {
    ImagePyramid *svm = (_imagesPyramids)[i];
    delete svm;
    if (copy) {
      svm = new GaussianPyramid(*(steerableBases[i]), iNbLevels, iSigma);
    }
    else {
      svm = new GaussianPyramid(steerableBases[i], iNbLevels, iSigma);
    }
    _imagesPyramids[i] = svm;
  }
}

float SteerableViewMap::readSteerableViewMapPixel(uint iOrientation, int iLevel, int x, int y)
{
  ImagePyramid *pyramid = _imagesPyramids[iOrientation];
  if (!pyramid) {
    if (G.debug & G_DEBUG_FREESTYLE) {
      cout << "Warning: this steerable ViewMap level doesn't exist" << endl;
    }
    return 0.0f;
  }
  if ((x < 0) || (x >= pyramid->width()) || (y < 0) || (y >= pyramid->height())) {
    return 0;
  }
  // float v = pyramid->pixel(x, pyramid->height() - 1 - y, iLevel) * 255.0f;
  // We encode both the directionality and the lines counting on 8 bits (because of frame buffer).
  // Thus, we allow until 8 lines to pass through the same pixel, so that we can discretize the
  // Pi/_nbOrientations angle into 32 slices. Therefore, for example, in the vertical direction, a
  // vertical line will have the value 32 on each pixel it passes through.
  float v = pyramid->pixel(x, pyramid->height() - 1 - y, iLevel) / 32.0f;
  return v;
}

float SteerableViewMap::readCompleteViewMapPixel(int iLevel, int x, int y)
{
  return readSteerableViewMapPixel(_nbOrientations, iLevel, x, y);
}

uint SteerableViewMap::getNumberOfPyramidLevels() const
{
  if (_imagesPyramids[0]) {
    return _imagesPyramids[0]->getNumberOfLevels();
  }
  return 0;
}

void SteerableViewMap::saveSteerableViewMap() const
{
  for (uint i = 0; i <= _nbOrientations; ++i) {
    if (_imagesPyramids[i] == nullptr) {
      cerr << "SteerableViewMap warning: orientation " << i
           << " of steerable View Map whas not been computed yet" << endl;
      continue;
    }
    int ow = _imagesPyramids[i]->width(0);
    int oh = _imagesPyramids[i]->height(0);

    // soc QString base("SteerableViewMap");
    string base("SteerableViewMap");
    stringstream filepath;

    for (int j = 0; j < _imagesPyramids[i]->getNumberOfLevels(); ++j) {  // soc
      float coeff = 1.0f;  // 1 / 255.0f; // 100 * 255; // * pow(2, j);
      // soc QImage qtmp(ow, oh, QImage::Format_RGB32);
      ImBuf *ibuf = IMB_allocImBuf(ow, oh, 32, IB_rect);
      int rowbytes = ow * 4;
      char *pix;

      for (int y = 0; y < oh; ++y) {    // soc
        for (int x = 0; x < ow; ++x) {  // soc
          int c = int(coeff * _imagesPyramids[i]->pixel(x, y, j));
          if (c > 255) {
            c = 255;
          }
          // int c = (int)(_imagesPyramids[i]->pixel(x, y, j));

          // soc qtmp.setPixel(x, y, qRgb(c, c, c));
          pix = (char *)ibuf->rect + y * rowbytes + x * 4;
          pix[0] = pix[1] = pix[2] = c;
        }
      }

      // soc qtmp.save(base+QString::number(i)+"-"+QString::number(j)+".png", "PNG");
      filepath << base;
      filepath << i << "-" << j << ".png";
      ibuf->ftype = IMB_FTYPE_PNG;
      IMB_saveiff(ibuf, const_cast<char *>(filepath.str().c_str()), 0);
    }
#if 0
    QString base("SteerableViewMap");
    for (unsigned j = 0; j < _imagesPyramids[i]->getNumberOfLevels(); ++j) {
      GrayImage *img = _imagesPyramids[i]->getLevel(j);
      int ow = img->width();
      int oh = img->height();
      float coeff = 1.0f;  // 100 * 255; // * pow(2, j);
      QImage qtmp(ow, oh, 32);
      for (unsigned int y = 0; y < oh; ++y) {
        for (unsigned int x = 0; x < ow; ++x) {
          int c = (int)(coeff * img->pixel(x, y));
          if (c > 255) {
            c = 255;
          }
          //int c = (int)(_imagesPyramids[i]->pixel(x, y, j));
          qtmp.setPixel(x, y, qRgb(c, c, c));
        }
      }
      qtmp.save(base + QString::number(i) + "-" + QString::number(j) + ".png", "PNG");
    }
#endif
  }
}

} /* namespace Freestyle */
