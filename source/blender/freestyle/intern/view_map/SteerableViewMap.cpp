/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/view_map/SteerableViewMap.cpp
 *  \ingroup freestyle
 *  \brief Convenient access to the steerable ViewMap to which any element of the ViewMap belongs to.
 *  \author Stephane Grabli
 *  \date 01/07/2003
 */

#include <sstream>

#include "Silhouette.h"
#include "SteerableViewMap.h"

#include "../geometry/Geom.h"

#include "../image/ImagePyramid.h"
#include "../image/Image.h"

#include "BKE_global.h"
#include "BLI_math.h"

extern "C" {
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}

namespace Freestyle {

using namespace Geometry;

SteerableViewMap::SteerableViewMap(unsigned int nbOrientations)
{
	_nbOrientations = nbOrientations;
	_bound = cos(M_PI / (float)_nbOrientations);
	for (unsigned int i = 0; i < _nbOrientations; ++i) {
		_directions.push_back(Vec2d(cos((float)i * M_PI / (float)_nbOrientations),
		                            sin((float)i * M_PI / (float)_nbOrientations)));
	}
	Build();
}

void SteerableViewMap::Build()
{
	_imagesPyramids = new ImagePyramid *[_nbOrientations + 1]; // one more map to store the complete visible VM
	memset((_imagesPyramids), 0, (_nbOrientations + 1) * sizeof(ImagePyramid *));
}

SteerableViewMap::SteerableViewMap(const SteerableViewMap& iBrother)
{
	_nbOrientations = iBrother._nbOrientations;
	unsigned int i;
	_bound = iBrother._bound;
	_directions = iBrother._directions;
	_mapping = iBrother._mapping;
	_imagesPyramids = new ImagePyramid *[_nbOrientations + 1]; // one more map to store the complete visible VM
	for (i = 0; i <= _nbOrientations; ++i)
		_imagesPyramids[i] = new GaussianPyramid(*(dynamic_cast<GaussianPyramid*>(iBrother._imagesPyramids[i])));
}

SteerableViewMap::~SteerableViewMap()
{
	Clear();
}

void SteerableViewMap::Clear()
{
	unsigned int i;
	if (_imagesPyramids) {
		for (i = 0; i <= _nbOrientations; ++i) {
			if (_imagesPyramids[i])
				delete (_imagesPyramids)[i];
		}
		delete[] _imagesPyramids;
		_imagesPyramids = 0;
	} 
	if (!_mapping.empty()) {
		for (map<unsigned int, double*>::iterator m = _mapping.begin(), mend = _mapping.end();  m != mend; ++m) {
			delete[] (*m).second;
		}
		_mapping.clear();
	}
}

void SteerableViewMap::Reset()
{
	Clear();
	Build();
}

double SteerableViewMap::ComputeWeight(const Vec2d& dir, unsigned i)
{
	double dotp = fabs(dir * _directions[i]);
	if (dotp < _bound)
		return 0.0;
	if (dotp > 1.0)
		dotp = 1.0;

	return cos((float)_nbOrientations / 2.0 * acos(dotp));
}

double *SteerableViewMap::AddFEdge(FEdge *iFEdge)
{
	unsigned i;
	unsigned id = iFEdge->getId().getFirst();
	map<unsigned int, double *>::iterator o = _mapping.find(id);
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

unsigned SteerableViewMap::getSVMNumber(const Vec2f& orient)
{
	Vec2f dir(orient);
	//soc unsigned res = 0;
	real norm = dir.norm();
	if (norm < 1.0e-6) {
		return _nbOrientations + 1;
	}
	dir /= norm;
	double maxw = 0.0f;
	unsigned winner = _nbOrientations + 1;
	for (unsigned int i = 0; i < _nbOrientations; ++i) {
		double w = ComputeWeight(dir, i);
		if (w > maxw) {
			maxw = w;
			winner = i;
		}
	}
	return winner;
}

unsigned SteerableViewMap::getSVMNumber(unsigned id)
{
	map<unsigned int, double *>::iterator o = _mapping.find(id);
	if (o != _mapping.end()) {
		double *wvalues = (*o).second;
		double maxw = 0.0; 
		unsigned winner = _nbOrientations + 1;
		for (unsigned i = 0; i < _nbOrientations; ++i) {
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

void SteerableViewMap::buildImagesPyramids(GrayImage **steerableBases, bool copy, unsigned iNbLevels, float iSigma)
{
	for (unsigned int i = 0; i <= _nbOrientations; ++i) {
		ImagePyramid *svm = (_imagesPyramids)[i];
		if (svm)
			delete svm;
		if (copy)
			svm = new GaussianPyramid(*(steerableBases[i]), iNbLevels, iSigma);
		else
			svm = new GaussianPyramid(steerableBases[i], iNbLevels, iSigma);
		_imagesPyramids[i] = svm;
	}
}

float SteerableViewMap::readSteerableViewMapPixel(unsigned iOrientation, int iLevel, int x, int y)
{
	ImagePyramid *pyramid = _imagesPyramids[iOrientation];
	if (!pyramid) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Warning: this steerable ViewMap level doesn't exist" << endl;
		}
		return 0.0f;
	}
	if ((x < 0) || (x >= pyramid->width()) || (y < 0) || (y >= pyramid->height()))
		return 0;
	//float v = pyramid->pixel(x, pyramid->height() - 1 - y, iLevel) * 255.0f;
	// We encode both the directionality and the lines counting on 8 bits (because of frame buffer). Thus, we allow
	// until 8 lines to pass through the same pixel, so that we can discretize the Pi/_nbOrientations angle into
	// 32 slices. Therefore, for example, in the vertical direction, a vertical line will have the value 32 on
	// each pixel it passes through.
	float v = pyramid->pixel(x, pyramid->height() - 1 - y, iLevel) / 32.0f;
	return v;
}

float SteerableViewMap::readCompleteViewMapPixel(int iLevel, int x, int y)
{
	return readSteerableViewMapPixel(_nbOrientations, iLevel, x, y);
}

unsigned int SteerableViewMap::getNumberOfPyramidLevels() const
{
	if (_imagesPyramids[0])
		return _imagesPyramids[0]->getNumberOfLevels();
	return 0;
}

void SteerableViewMap::saveSteerableViewMap() const
{
	for (unsigned int i = 0; i <= _nbOrientations; ++i) {
		if (_imagesPyramids[i] == 0) {
			cerr << "SteerableViewMap warning: orientation " << i <<
			        " of steerable View Map whas not been computed yet" << endl;
			continue;
		}
		int ow = _imagesPyramids[i]->width(0);
		int oh = _imagesPyramids[i]->height(0);

		//soc QString base("SteerableViewMap");
		string base("SteerableViewMap");
		stringstream filename;

		for (int j = 0; j < _imagesPyramids[i]->getNumberOfLevels(); ++j) { //soc
			float coeff = 1.0f; // 1 / 255.0f; // 100 * 255; // * pow(2, j);
			//soc QImage qtmp(ow, oh, QImage::Format_RGB32);
			ImBuf *ibuf = IMB_allocImBuf(ow, oh, 32, IB_rect);
			int rowbytes = ow * 4;
			char *pix;

			for (int y = 0; y < oh; ++y) { //soc
				for (int x = 0; x < ow; ++x) { //soc
					int c = (int)(coeff * _imagesPyramids[i]->pixel(x, y, j));
					if (c > 255)
						c = 255;
					//int c = (int)(_imagesPyramids[i]->pixel(x, y, j));

					//soc qtmp.setPixel(x, y, qRgb(c, c, c));
					pix = (char *)ibuf->rect + y * rowbytes + x * 4;
					pix[0] = pix[1] = pix[2] = c;
				}
			}

			//soc qtmp.save(base+QString::number(i)+"-"+QString::number(j)+".png", "PNG");
			filename << base;
			filename << i << "-" << j << ".png";
			ibuf->ftype = PNG;
			IMB_saveiff(ibuf, const_cast<char *>(filename.str().c_str()), 0);
		}
#if 0
		QString base("SteerableViewMap");
		for (unsigned j = 0; j < _imagesPyramids[i]->getNumberOfLevels(); ++j) {
			GrayImage *img = _imagesPyramids[i]->getLevel(j);
			int ow = img->width();
			int oh = img->height();
			float coeff = 1.0f; // 100 * 255; // * pow(2, j);
			QImage qtmp(ow, oh, 32);
			for (unsigned int y = 0; y < oh; ++y) {
				for (unsigned int x = 0; x < ow; ++x) {
					int c = (int)(coeff * img->pixel(x, y));
					if (c > 255)
						c = 255;
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
