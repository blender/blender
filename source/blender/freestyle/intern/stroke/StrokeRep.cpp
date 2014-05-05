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

/** \file blender/freestyle/intern/stroke/StrokeRep.cpp
 *  \ingroup freestyle
 *  \brief Class to define the representation of a stroke (for display purpose)
 *  \author Stephane Grabli
 *  \date 05/03/2003
 */

#include "Stroke.h"
#include "StrokeIterators.h"
#include "StrokeAdvancedIterators.h"
#include "StrokeRenderer.h"
#include "StrokeRep.h"

#include "BKE_global.h"

using namespace std;

namespace Freestyle {

//
// STROKE VERTEX REP
/////////////////////////////////////

StrokeVertexRep::StrokeVertexRep(const StrokeVertexRep& iBrother)
{
	_point2d = iBrother._point2d;
	_texCoord = iBrother._texCoord;
	_texCoord_w_tips = iBrother._texCoord_w_tips;
	_color = iBrother._color;
	_alpha = iBrother._alpha;
}

//
// STRIP
/////////////////////////////////////

Strip::Strip(const vector<StrokeVertex*>& iStrokeVertices, bool hasTips, bool beginTip, bool endTip, float texStep)
{
	createStrip(iStrokeVertices);

	// We compute both kinds of coordinates to use different kinds of textures
	computeTexCoord (iStrokeVertices, texStep);
	computeTexCoordWithTips (iStrokeVertices, beginTip, endTip, texStep);
}

Strip::Strip(const Strip& iBrother)
{
	if (!iBrother._vertices.empty()) {
		for (vertex_container::const_iterator v = iBrother._vertices.begin(), vend = iBrother._vertices.end();
		     v != vend;
		     ++v)
		{
			_vertices.push_back(new StrokeVertexRep(**v));
		}
	}
	_averageThickness = iBrother._averageThickness;
}

Strip::~Strip()
{
	if (!_vertices.empty()) {
		for (vertex_container::iterator v = _vertices.begin(), vend = _vertices.end(); v != vend; ++v) {
			delete (*v);
		}
		_vertices.clear();
	}
}

//////////////////////////
// Strip creation
//////////////////////////

#define EPS_SINGULARITY_RENDERER 0.05
#define ZERO 0.00001
#define MAX_RATIO_LENGTH_SINGU 2
#define HUGE_COORD 1.0e4

static bool notValid (Vec2r p)
{
	return (p[0] != p[0]) || (p[1] != p[1]) || (fabs(p[0]) > HUGE_COORD) || (fabs(p[1]) > HUGE_COORD) ||
	       (p[0] < -HUGE_COORD) || (p[1] < -HUGE_COORD);
}

#if 0
static real crossP(const Vec2r& A, const Vec2r& B)
{
	return A[0] * B[1] - A[1] * B[0];
}
#endif

void Strip::createStrip (const vector<StrokeVertex*>& iStrokeVertices)
{
	//computeParameterization();
	if (iStrokeVertices.size() < 2) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Warning: strip has less than 2 vertices" << endl;
		}
		return;
	}
	_vertices.reserve(2 * iStrokeVertices.size());
	if (!_vertices.empty()) {
		for (vertex_container::iterator v = _vertices.begin(), vend = _vertices.end(); v != vend; ++v) {
			delete (*v);
		}
		_vertices.clear();
	}
	_averageThickness = 0.0;

	vector<StrokeVertex *>::const_iterator v, vend, v2, vPrev;
	StrokeVertex *sv, *sv2, *svPrev;
	int orientationErrors = 0;

	//special case of first vertex
	v = iStrokeVertices.begin();
	sv = *v;
	vPrev = v; //in case the stroke has only 2 vertices;
	++v;
	sv2 = *v;
	Vec2r dir(sv2->getPoint() - sv->getPoint());
	Vec2r orthDir(-dir[1], dir[0]);
	if (orthDir.norm() > ZERO)
		orthDir.normalize();
	Vec2r stripDir(orthDir);
	// check whether the orientation was user defined
	if (sv->attribute().isAttributeAvailableVec2f("orientation")) {
		Vec2r userDir = sv->attribute().getAttributeVec2f("orientation");
		if (userDir.norm() > 1e-6) {
			userDir.normalize();
			real dp = userDir * orthDir;
			if (dp < 0)
				userDir = userDir * (-1.0f);
			stripDir = userDir;
		}
		else {
			++orientationErrors;
		}
	}
	const float *thickness = sv->attribute().getThickness();
	_vertices.push_back(new StrokeVertexRep(sv->getPoint() + thickness[1] * stripDir));
	_vertices.push_back(new StrokeVertexRep(sv->getPoint() - thickness[0] * stripDir));

#if 0
	Vec2r userDir = _stroke->getBeginningOrientation();
	if (userDir != Vec2r(0, 0)) {
		userDir.normalize();
		real o1 = (orthDir * userDir);
		real o2 = crossP(orthDir, userDir);
		real orientation = o1 * o2;
		if (orientation > 0) {
			// then the vertex to move is v0
			if (o1 > 0)
				_vertex[0] = _vertex[1] + userDir;
			else
				_vertex[0] = _vertex[1] - userDir;
		}
		if (orientation < 0) {
			// then we must move v1
			if (o1 < 0)
				_vertex[1] = _vertex[0] + userDir;
			else
				_vertex[1] = _vertex[0] - userDir;
		}
	}
#endif

	int i = 2; // 2 because we have already processed the first vertex

	for (vend = iStrokeVertices.end(); v != vend; ++v) {
		v2 = v;
		++v2;
		if (v2 == vend)
			break;
		sv = (*v);
		sv2 = (*v2);
		svPrev = (*vPrev);
		Vec2r p(sv->getPoint()), p2(sv2->getPoint()), pPrev(svPrev->getPoint());

		//direction and orthogonal vector to the next segment
		Vec2r dir(p2 - p);
		float dirNorm = dir.norm();
		dir.normalize();
		Vec2r orthDir(-dir[1], dir[0]);
		Vec2r stripDir = orthDir;
		if (sv->attribute().isAttributeAvailableVec2f("orientation")) {
			Vec2r userDir = sv->attribute().getAttributeVec2f("orientation");
			if (userDir.norm() > 1e-6) {
				userDir.normalize();
				real dp = userDir * orthDir;
				if (dp < 0)
					userDir = userDir * (-1.0f);
				stripDir = userDir;
			}
			else {
				++orientationErrors;
			}
		}

		//direction and orthogonal vector to the previous segment
		Vec2r dirPrev(p - pPrev);
		float dirPrevNorm = dirPrev.norm();
		dirPrev.normalize();
		Vec2r orthDirPrev(-dirPrev[1], dirPrev[0]);
		Vec2r stripDirPrev = orthDirPrev;
		if (svPrev->attribute().isAttributeAvailableVec2f("orientation")) {
			Vec2r userDir = svPrev->attribute().getAttributeVec2f("orientation");
			if (userDir.norm() > 1e-6) {
				userDir.normalize();
				real dp = userDir * orthDir;
				if (dp < 0)
					userDir = userDir * (-1.0f);
				stripDirPrev = userDir;
			}
			else {
				++orientationErrors;
			}
		}

		const float *thickness = sv->attribute().getThickness();
		_averageThickness += thickness[0] + thickness[1];
		Vec2r pInter;
		int interResult;

		interResult = GeomUtils::intersect2dLine2dLine(Vec2r(pPrev + thickness[1] * stripDirPrev),
		                                               Vec2r(p + thickness[1] * stripDirPrev),
		                                               Vec2r(p + thickness[1] * stripDir),
		                                               Vec2r(p2 + thickness[1] * stripDir),
		                                               pInter);
		if (interResult == GeomUtils::DO_INTERSECT)
			_vertices.push_back(new StrokeVertexRep(pInter));
		else
			_vertices.push_back(new StrokeVertexRep(p + thickness[1] * stripDir));
		++i;

		interResult = GeomUtils::intersect2dLine2dLine(Vec2r(pPrev - thickness[0] * stripDirPrev),
		                                               Vec2r(p - thickness[0] * stripDirPrev),
		                                               Vec2r(p - thickness[0] * stripDir),
		                                               Vec2r(p2 - thickness[0] * stripDir),
		                                               pInter);
		if (interResult == GeomUtils::DO_INTERSECT)
			_vertices.push_back(new StrokeVertexRep(pInter));
		else
			_vertices.push_back(new StrokeVertexRep(p - thickness[0] * stripDir));
		++i;

		// if the angle is obtuse, we simply average the directions to avoid the singularity
		stripDir = stripDir + stripDirPrev;
		if ((dirNorm < ZERO) || (dirPrevNorm < ZERO) || (stripDir.norm() < ZERO)) {
			stripDir[0] = 0;
			stripDir[1] = 0;
		}
		else {
			stripDir.normalize();
		}

		Vec2r vec_tmp(_vertices[i - 2]->point2d() - p);
		if ((vec_tmp.norm() > thickness[1] * MAX_RATIO_LENGTH_SINGU) || (dirNorm < ZERO) || (dirPrevNorm < ZERO) ||
		    notValid(_vertices[i - 2]->point2d()) || (fabs(stripDir * dir) < EPS_SINGULARITY_RENDERER))
		{
			_vertices[i - 2]->setPoint2d(p + thickness[1] * stripDir);
		}

		vec_tmp = _vertices[i - 1]->point2d() - p;
		if ((vec_tmp.norm() > thickness[0] * MAX_RATIO_LENGTH_SINGU) || (dirNorm < ZERO) || (dirPrevNorm < ZERO) ||
		    notValid(_vertices[i - 1]->point2d()) || (fabs(stripDir * dir) < EPS_SINGULARITY_RENDERER))
		{
			_vertices[i - 1]->setPoint2d(p - thickness[0] * stripDir);
		}

		vPrev = v;
	} // end of for

	//special case of last vertex
	sv = *v;
	sv2 = *vPrev;
	dir = Vec2r(sv->getPoint() - sv2->getPoint());
	orthDir = Vec2r(-dir[1], dir[0]);
	if (orthDir.norm() > ZERO)
		orthDir.normalize();
	Vec2r stripDirLast(orthDir);
	// check whether the orientation was user defined
	if (sv->attribute().isAttributeAvailableVec2f("orientation")) {
		Vec2r userDir = sv->attribute().getAttributeVec2f("orientation");
		if (userDir.norm() > 1e-6) {
			userDir.normalize();
			real dp = userDir * orthDir;
			if (dp < 0)
				userDir = userDir * (-1.0f);
			stripDirLast = userDir;
		}
		else {
			++orientationErrors;
		}
	}
	const float *thicknessLast = sv->attribute().getThickness();
	_vertices.push_back(new StrokeVertexRep(sv->getPoint() + thicknessLast[1] * stripDirLast));
	++i;
	_vertices.push_back(new StrokeVertexRep(sv->getPoint() - thicknessLast[0] * stripDirLast));
	++i;

#if 0
	int n = i - 1;
	// check whether the orientation of the extremity was user defined
	userDir = _stroke->getEndingOrientation();
	if (userDir != Vec2r(0, 0)) {
		userDir.normalize();
		real o1 = (orthDir * userDir);
		real o2 = crossP(orthDir, userDir);
		real orientation = o1 * o2;
		if (orientation > 0) {
			// then the vertex to move is vn
			if (o1 < 0)
				_vertex[n] = _vertex[n - 1] + userDir;
			else
				_vertex[n] = _vertex[n - 1] - userDir;
		}
		if (orientation < 0) {
			// then we must move vn-1
			if (o1 > 0)
				_vertex[n - 1] = _vertex[n] + userDir;
			else
				_vertex[n - 1] = _vertex[n] - userDir;
		}
	}
#endif

	_averageThickness /= float(iStrokeVertices.size() - 2);
	//I did not use the first and last vertex for the average
	if (iStrokeVertices.size() < 3)
		_averageThickness = 0.5 * (thicknessLast[1] + thicknessLast[0] + thickness[0] + thickness[1]);

	if (orientationErrors > 0) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Warning: " << orientationErrors <<" invalid zero-length orientation vector(s) found.\n";
		}
	}

	if (i != 2 * (int)iStrokeVertices.size()) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Warning: problem with stripe size\n";
		}
	}

	cleanUpSingularities (iStrokeVertices);
}

// CLEAN UP
/////////////////////////

void Strip::cleanUpSingularities (const vector<StrokeVertex*>& iStrokeVertices)
{
	int k;
	int sizeStrip = _vertices.size();

	for (k = 0; k < sizeStrip; k++) {
		if (notValid(_vertices[k]->point2d())) {
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "Warning: strip vertex " << k << " non valid" << endl;
			}
			return;
		}
	}

	//return;
	if (iStrokeVertices.size() < 2)
		return;
	int i = 0, j;
	vector<StrokeVertex *>::const_iterator v, vend, v2, vPrev;
	StrokeVertex *sv, *sv2; //soc unused -  *svPrev;

	bool singu1 = false, singu2 = false;
	int timeSinceSingu1 = 0, timeSinceSingu2 = 0;

	//special case of first vertex
	v = iStrokeVertices.begin();
	for (vend = iStrokeVertices.end(); v != vend; v++) {
		v2 = v;
		++v2;
		if (v2 == vend)
			break;
		sv = (*v);
		sv2 = (*v2);
		Vec2r p(sv->getPoint()), p2(sv2->getPoint());

		Vec2r dir(p2 - p);
		if (dir.norm() > ZERO)
			dir.normalize();
		Vec2r dir1, dir2;
		dir1 = _vertices[2 * i + 2]->point2d() - _vertices[2 * i]->point2d();
		dir2 = _vertices[2 * i + 3]->point2d() - _vertices[2 * i + 1]->point2d();

		if ((dir1 * dir) < -ZERO) {
			singu1 = true;
			timeSinceSingu1++;
		}
		else {
			if (singu1) {
				int toto = i - timeSinceSingu1;
				if (toto < 0)
					cerr << "Stephane dit \"Toto\"" << endl;
				//traverse all the vertices of the singularity and average them
				Vec2r avP(0.0, 0.0);
				for (j = i - timeSinceSingu1; j <= i; j++)
					avP = Vec2r(avP + _vertices[2 * j]->point2d());
				avP = Vec2r( 1.0 / float(timeSinceSingu1 + 1) * avP);
				for (j = i - timeSinceSingu1; j <= i; j++)
					_vertices[2 * j]->setPoint2d(avP);
				//_vertex[2 * j] = _vertex[2 * i];
				singu1 = false;
				timeSinceSingu1 = 0;
			}
		}
		if ((dir2 * dir) < -ZERO) {
			singu2 = true;
			timeSinceSingu2++;
		}
		else {
			if (singu2) {
				int toto = i - timeSinceSingu2;
				if (toto < 0)
					cerr << "Stephane dit \"Toto\"" << endl;
				//traverse all the vertices of the singularity and average them
				Vec2r avP(0.0, 0.0);
				for (j = i - timeSinceSingu2; j <= i; j++)
					avP = Vec2r(avP + _vertices[2 * j + 1]->point2d());
				avP = Vec2r(1.0 / float(timeSinceSingu2 + 1) * avP);
				for (j = i - timeSinceSingu2; j <= i; j++)
					_vertices[2 * j + 1]->setPoint2d(avP);
				//_vertex[2 * j + 1] = _vertex[2 * i + 1];
				singu2 = false;
				timeSinceSingu2 = 0;
			}
		}
		i++;
	}

	if (singu1) {
		//traverse all the vertices of the singularity and average them
		Vec2r avP(0.0, 0.0);
		for (j = i - timeSinceSingu1; j < i; j++)
			avP = Vec2r(avP + _vertices[2 * j]->point2d());
		avP = Vec2r(1.0 / float(timeSinceSingu1) * avP);
		for (j = i - timeSinceSingu1; j < i; j++)
			_vertices[2 * j]->setPoint2d(avP);
	}
	if (singu2) {
		//traverse all the vertices of the singularity and average them
		Vec2r avP(0.0, 0.0);
		for (j = i - timeSinceSingu2; j < i; j++)
			avP = Vec2r(avP + _vertices[2 * j + 1]->point2d());
		avP = Vec2r(1.0 / float(timeSinceSingu2) * avP);
		for (j = i - timeSinceSingu2; j < i; j++)
			_vertices[2 * j + 1]->setPoint2d(avP);
	}

	for (k = 0; k < sizeStrip; k++) {
		if (notValid(_vertices[k]->point2d())) {
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "Warning: strip vertex " << k << " non valid after cleanup" << endl;
			}
			return;
		}
	}
}


// Texture coordinates
////////////////////////////////

void Strip::computeTexCoord (const vector<StrokeVertex *>& iStrokeVertices, float texStep)
{
	vector<StrokeVertex *>::const_iterator v, vend;
	StrokeVertex *sv;
	int i = 0;
	for (v = iStrokeVertices.begin(), vend = iStrokeVertices.end(); v != vend; v++) {
		sv = (*v);
		_vertices[i]->setTexCoord(Vec2r((real)(sv->curvilinearAbscissa() / (_averageThickness * texStep)), 0));
		_vertices[i]->setColor(Vec3r(sv->attribute().getColorRGB()));
		_vertices[i]->setAlpha(sv->attribute().getAlpha());
		i++;
		_vertices[i]->setTexCoord(Vec2r((real)(sv->curvilinearAbscissa() / (_averageThickness * texStep)), 1));
		_vertices[i]->setColor(Vec3r(sv->attribute().getColorRGB()));
		_vertices[i]->setAlpha(sv->attribute().getAlpha());
		i++;
#if 0
		cerr << "col=("<<sv->attribute().getColor()[0] << ", "
		     << sv->attribute().getColor()[1] << ", " << sv->attribute().getColor()[2] << ")" << endl;
#endif
	}
}

void Strip::computeTexCoordWithTips (const vector<StrokeVertex*>& iStrokeVertices, bool tipBegin, bool tipEnd, float texStep)
{
	vector<StrokeVertex*>::const_iterator v, vend;
	StrokeVertex *sv = NULL;
	StrokeVertexRep *tvRep[2] = {NULL};

	float l, fact, t;
	float u = 0, uPrev = 0;
	int tiles;
	int i = 0;
	float spacedThickness = _averageThickness * texStep;

	v = iStrokeVertices.begin();
	vend = iStrokeVertices.end();
	l = (*v)->strokeLength() / spacedThickness;
	tiles = int(l + 0.5); // round to the nearest
	fact = (float(tiles) + 0.5) / l;

#if 0
	cerr << "l=" << l << "  tiles=" << tiles << "    _averageThicnkess="
	     << _averageThickness << "    strokeLength=" << (*v)->strokeLength() << endl;
#endif

	vector<StrokeVertexRep*>::iterator currentSV = _vertices.begin();
	StrokeVertexRep *svRep;
	if (tipBegin) {
		for (; v != vend; v++) {
			sv = (*v);
			svRep = *currentSV;
			u = sv->curvilinearAbscissa() / spacedThickness * fact;
			if (u > 0.25)
				break;

			svRep->setTexCoord(Vec2r((real)u, 0.5), true);
			i++;
			++currentSV;

			svRep = *currentSV;
			svRep->setTexCoord(Vec2r((real)u, 1), true);
			i++;
			++currentSV;
			uPrev = u;
		}

		if (v != vend && i >= 2) {
			// first transition vertex
			if (fabs(u - uPrev) > ZERO)
				t = (0.25 - uPrev) / (u - uPrev);
			else
				t = 0;
			for (int k = 0; k < 2; k++) {
				tvRep[k] = new StrokeVertexRep((1 - t) * _vertices[i - 2]->point2d() + t * _vertices[i]->point2d());
				tvRep[k]->setTexCoord((1 - t) * _vertices[i - 2]->texCoord() + t * _vertices[i]->texCoord());
				// v coord is 0.5 for tvRep[0], 1.0 for tvRep[1]
				tvRep[k]->setTexCoord(Vec2r(0.25, 0.5 * (k + 1)), true);
				tvRep[k]->setColor((1 - t) * _vertices[i - 2]->color() + t * Vec3r(sv->attribute().getColorRGB()));
				tvRep[k]->setAlpha((1 - t) * _vertices[i - 2]->alpha() + t * sv->attribute().getAlpha());
				i++;
			}
			for (int k = 0; k < 2; k++) {
				currentSV = _vertices.insert(currentSV, tvRep[k]);
				++currentSV;
			}

			// copy the vertices with different texture coordinates
			for (int k = 0; k < 2; k++) {
				tvRep[k] = new StrokeVertexRep(*(_vertices[i - 2]));
				// v coord is 0.0 for tvRep[0], 0.5 for tvRep[1]
				tvRep[k]->setTexCoord(Vec2r(0.0, 0.5 * k), true); // FIXED u coord
				i++;
			}
			for (int k = 0; k < 2; k++) {
				currentSV = _vertices.insert(currentSV, tvRep[k]);
				++currentSV;
			}
		}
	}
	uPrev = 0;

	// body of the stroke
	for (; v != vend; v++) {
		sv = (*v);
		svRep = *currentSV;
		u = sv->curvilinearAbscissa() / spacedThickness * fact - 0.25;
		if (u > tiles)
			break;

		svRep->setTexCoord(Vec2r((real)u, 0), true);
		i++;
		++currentSV;

		svRep = *currentSV;
		svRep->setTexCoord(Vec2r((real)u, 0.5), true);
		i++;
		++currentSV;

		uPrev = u;
	}

	if (tipEnd) {
		if (v != vend && i >= 2) {
			// second transition vertex
			if (fabs(u - uPrev) > ZERO)
				t = (float(tiles) - uPrev) / (u - uPrev);
			else
				t = 0;
			for (int k = 0; k < 2; k++) {
				tvRep[k] = new StrokeVertexRep((1 - t) * _vertices[i - 2]->point2d() + t * _vertices[i]->point2d());
				tvRep[k]->setTexCoord((1 - t) * _vertices[i - 2]->texCoord() + t * _vertices[i]->texCoord());
				// v coord is 0.0 for tvRep[0], 0.5 for tvRep[1]
				tvRep[k]->setTexCoord(Vec2r((real)tiles, 0.5 * k), true); // FIXED u coord
				tvRep[k]->setColor((1 - t) * _vertices[i - 2]->color() + t * Vec3r(sv->attribute().getColorRGB()));
				tvRep[k]->setAlpha((1 - t) * _vertices[i - 2]->alpha() + t * sv->attribute().getAlpha());
				i++;
			}
			for (int k = 0; k < 2; k++) {
				currentSV = _vertices.insert(currentSV, tvRep[k]);
				++currentSV;
			}

			// copy the vertices with different texture coordinates
			for (int k = 0; k < 2; k++) {
				tvRep[k] = new StrokeVertexRep(*(_vertices[i - 2]));
				// v coord is 0.5 for tvRep[0], 1.0 for tvRep[1]
				tvRep[k]->setTexCoord(Vec2r(0.75, 0.5 * (k + 1)), true);
				i++;
			}
			for (int k = 0; k < 2; k++) {
				currentSV = _vertices.insert(currentSV, tvRep[k]);
				++currentSV;
			}
		}

		// end tip
		for (; v != vend; v++) {
			sv = (*v);
			svRep = *currentSV;
			u = 0.75 + sv->curvilinearAbscissa() / spacedThickness * fact - float(tiles) - 0.25;

			svRep->setTexCoord(Vec2r((real)u, 0.5), true);
			i++;
			++currentSV;

			svRep = *currentSV;
			svRep->setTexCoord(Vec2r((real)u, 1), true);
			i++;
			++currentSV;
		}
	}

#if 0
	cerr << "u=" << u << "    i=" << i << "/" << _sizeStrip << endl;

	for (i = 0; i < _sizeStrip; i++)
		_alpha[i] = 1.0;

	for (i = 0; i < _sizeStrip; i++)
		cerr << "(" << _texCoord[i][0] << ", " << _texCoord[i][1] << ") ";
		cerr << endl;

	Vec2r vec_tmp;
	for (i = 0; i < _sizeStrip / 2; i++)
		vec_tmp = _vertex[2 * i] - _vertex[2 * i + 1];
	if (vec_tmp.norm() > 4 * _averageThickness)
		cerr << "Warning (from Fredo): There is a pb in the texture coordinates computation" << endl;
#endif
}

//
// StrokeRep
/////////////////////////////////////

StrokeRep::StrokeRep()
{
	_stroke = 0;
	_strokeType = Stroke::OPAQUE_MEDIUM;
	_textureStep = 1.0;
	for (int a = 0; a < MAX_MTEX; a++) {
		_mtex[a] = NULL;
	}
	TextureManager *ptm = TextureManager::getInstance();
	if (ptm)
		_textureId = ptm->getDefaultTextureId();
#if 0
	_averageTextureAlpha = 0.5; //default value
	if (_strokeType == OIL_STROKE)
		_averageTextureAlpha = 0.75;
	if (_strokeType >= NO_BLEND_STROKE)
		_averageTextureAlpha = 1.0
#endif
}

StrokeRep::StrokeRep(Stroke *iStroke)
{
	_stroke = iStroke;
	_strokeType = iStroke->getMediumType();
	_textureId = iStroke->getTextureId();
	_textureStep = iStroke->getTextureStep();
	for (int a = 0; a < MAX_MTEX; a++) {
		if (iStroke->getMTex(a)) {
			_mtex[a] = iStroke->getMTex(a);
		}
		else {
			_mtex[a] = NULL;
		}
	}
	if (_textureId == 0) {
		TextureManager *ptm = TextureManager::getInstance();
		if (ptm)
			_textureId = ptm->getDefaultTextureId();
	}

#if 0
	_averageTextureAlpha = 0.5; //default value
	if (_strokeType == OIL_STROKE)
		_averageTextureAlpha = 0.75; 
	if (_strokeType >= NO_BLEND_STROKE)
		_averageTextureAlpha = 1.0;
#endif
	create();
}

StrokeRep::StrokeRep(const StrokeRep& iBrother)
{
	//soc unused - int i = 0;
	_stroke = iBrother._stroke;
	_strokeType = iBrother._strokeType;
	_textureId = iBrother._textureId;
	_textureStep = iBrother._textureStep;
	for (int a = 0; a < MAX_MTEX; a++) {
		if (iBrother._mtex[a]) {
			_mtex[a] = iBrother._mtex[a];
		}
		else {
			_mtex[a] = NULL;
		}
	}
	for (vector<Strip*>::const_iterator s = iBrother._strips.begin(), send = iBrother._strips.end();
	     s != send;
	     ++s)
	{
		_strips.push_back(new Strip(**s));
	}
}

StrokeRep::~StrokeRep()
{
	if (!_strips.empty()) {
		for (vector<Strip*>::iterator s = _strips.begin(), send = _strips.end(); s != send; ++s) {
			delete (*s);
		}
		_strips.clear();
	}
}

void StrokeRep::create()
{
	vector<StrokeVertex*> strip;
	StrokeInternal::StrokeVertexIterator v = _stroke->strokeVerticesBegin();
	StrokeInternal::StrokeVertexIterator vend = _stroke->strokeVerticesEnd();

	bool first = true;
	bool end = false;
	while (v != vend) {
		while ((v != vend) && (!(*v).attribute().isVisible())) {
			++v;
			first = false;
		}
		while ((v != vend) && ((*v).attribute().isVisible())) {
			strip.push_back(&(*v));
			++v;
		}
		if (v != vend) {
			// add the last vertex and create
			strip.push_back(&(*v));
		}
		else {
			end = true;
		}
		if ((!strip.empty()) && (strip.size() > 1)) {
			_strips.push_back(new Strip(strip, _stroke->hasTips(), first, end, _stroke->getTextureStep()));
			strip.clear();
		}
		first = false;
	}
}

void StrokeRep::Render(const StrokeRenderer *iRenderer)
{
	iRenderer->RenderStrokeRep(this);
}

} /* namespace Freestyle */
