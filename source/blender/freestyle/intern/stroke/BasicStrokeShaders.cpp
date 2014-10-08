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

/** \file blender/freestyle/intern/stroke/BasicStrokeShaders.cpp
 *  \ingroup freestyle
 *  \brief Class gathering basic stroke shaders
 *  \author Stephane Grabli
 *  \date 17/12/2002
 */

#include <fstream>

#include "AdvancedFunctions0D.h"
#include "AdvancedFunctions1D.h"
#include "BasicStrokeShaders.h"
#include "StrokeIO.h"
#include "StrokeIterators.h"
#include "StrokeRenderer.h"

#include "../system/PseudoNoise.h"
#include "../system/RandGen.h"
#include "../system/StringUtils.h"

#include "../view_map/Functions0D.h"
#include "../view_map/Functions1D.h"

#include "BKE_global.h"

extern "C" {
#  include "IMB_imbuf.h"
#  include "IMB_imbuf_types.h"
}

namespace Freestyle {

// Internal function

static void convert(ImBuf *imBuf, float **oArray, unsigned &oSize)
{
	oSize = imBuf->x;
	*oArray = new float[oSize];

	char *pix;
	for (unsigned int i = 0; i < oSize; ++i) {
		pix = (char *) imBuf->rect + i * 4;
		(*oArray)[i] = ((float) pix[2]) / 255.0f;
	}
}

namespace StrokeShaders {

//
//  Thickness modifiers
//
//////////////////////////////////////////////////////////

int ConstantThicknessShader::shade(Stroke& stroke) const
{
	StrokeInternal::StrokeVertexIterator v, vend;
	int i = 0;
	int size = stroke.strokeVerticesSize();
	for (v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		// XXX What's the use of i here? And is not the thickness always overriden by the last line of the loop?
		if ((1 == i) || (size - 2 == i))
			v->attribute().setThickness(_thickness / 4.0, _thickness / 4.0);
		if ((0 == i) || (size - 1 == i))
			v->attribute().setThickness(0, 0);

		v->attribute().setThickness(_thickness / 2.0, _thickness / 2.0);
	}
	return 0;
}

int ConstantExternThicknessShader::shade(Stroke& stroke) const
{
	StrokeInternal::StrokeVertexIterator v, vend;
	int i = 0;
	int size = stroke.strokeVerticesSize();
	for (v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		// XXX What's the use of i here? And is not the thickness always overriden by the last line of the loop?
		if ((1 == i) || (size - 2 == i))
			v->attribute().setThickness(_thickness / 2.0, 0);
		if ((0 == i) || (size - 1 == i))
			v->attribute().setThickness(0, 0);

		v->attribute().setThickness(_thickness, 0);
	}
	return 0;
}

int IncreasingThicknessShader::shade(Stroke& stroke) const
{
	int n = stroke.strokeVerticesSize() - 1, i;
	StrokeInternal::StrokeVertexIterator v, vend;
	for (i = 0, v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd();
	     v != vend;
	     ++v, ++i)
	{
		float t;
		if (i < (float)n / 2.0f)
			t = (1.0 - (float)i / (float)n) * _ThicknessMin + (float)i / (float)n * _ThicknessMax;
		else
			t = (1.0 - (float)i / (float)n) * _ThicknessMax + (float)i / (float)n * _ThicknessMin;
		v->attribute().setThickness(t / 2.0, t / 2.0);
	}
	return 0;
}

int ConstrainedIncreasingThicknessShader::shade(Stroke& stroke) const
{
	float slength = stroke.getLength2D();
	float maxT = min(_ratio * slength, _ThicknessMax);
	int n = stroke.strokeVerticesSize() - 1, i;
	StrokeInternal::StrokeVertexIterator v, vend;
	for (i = 0, v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd();
	     v != vend;
	     ++v, ++i)
	{
		// XXX Why not using an if/else here? Else, if last condition is true, everything else is computed for nothing!
		float t;
		if (i < (float)n / 2.0f)
			t = (1.0 - (float)i / (float)n) * _ThicknessMin + (float)i / (float)n * maxT;
		else
			t = (1.0 - (float)i / (float)n) * maxT + (float)i / (float)n * _ThicknessMin;
		v->attribute().setThickness(t / 2.0, t / 2.0);
		if (i == n - 1)
			v->attribute().setThickness(_ThicknessMin / 2.0, _ThicknessMin / 2.0);
	}
	return 0;
}


int LengthDependingThicknessShader::shade(Stroke& stroke) const
{
	float step = (_maxThickness - _minThickness) / 3.0f;
	float l = stroke.getLength2D();
	float thickness = 0.0f;
	if (l > 300.0f)
		thickness = _minThickness + 3.0f * step;
	else if ((l < 300.0f) && (l > 100.0f))
		thickness = _minThickness + 2.0f * step;
	else if ((l < 100.0f) && (l > 50.0f))
		thickness = _minThickness + 1.0f * step;
	else // else if (l <  50.0f), tsst...
		thickness = _minThickness;

	StrokeInternal::StrokeVertexIterator v, vend;
	int i = 0;
	int size = stroke.strokeVerticesSize();
	for (v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		// XXX What's the use of i here? And is not the thickness always overriden by the last line of the loop?
		if ((1 == i) || (size - 2 == i))
			v->attribute().setThickness(thickness / 4.0, thickness / 4.0);
		if ((0 == i) || (size - 1 == i))
			v->attribute().setThickness(0, 0);

		v->attribute().setThickness(thickness / 2.0, thickness / 2.0);
	}
	return 0;
}

static const unsigned NB_VALUE_NOISE = 512;

ThicknessNoiseShader::ThicknessNoiseShader() : StrokeShader()
{
	_amplitude = 1.0f;
	_scale = 1.0f / 2.0f / (float)NB_VALUE_NOISE;
}

ThicknessNoiseShader::ThicknessNoiseShader(float iAmplitude, float iPeriod) : StrokeShader()
{
	_amplitude = iAmplitude;
	_scale = 1.0f / iPeriod / (float)NB_VALUE_NOISE;
}

int ThicknessNoiseShader::shade(Stroke& stroke) const
{
	StrokeInternal::StrokeVertexIterator v = stroke.strokeVerticesBegin(), vend;
	real initU1 = v->strokeLength() * real(NB_VALUE_NOISE) + RandGen::drand48() * real(NB_VALUE_NOISE);
	real initU2 = v->strokeLength() * real(NB_VALUE_NOISE) + RandGen::drand48() * real(NB_VALUE_NOISE);

	real bruit, bruit2;
	PseudoNoise mynoise, mynoise2;
	for (vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		bruit = mynoise.turbulenceSmooth(_scale * v->curvilinearAbscissa() + initU1, 2); // 2 : nbOctaves
		bruit2 = mynoise2.turbulenceSmooth(_scale * v->curvilinearAbscissa() + initU2, 2); // 2 : nbOctaves
		const float *originalThickness = v->attribute().getThickness();
		float r = bruit * _amplitude + originalThickness[0];
		float l = bruit2 * _amplitude + originalThickness[1];
		v->attribute().setThickness(r, l);
	}

	return 0;
}

//
//  Color shaders
//
///////////////////////////////////////////////////////////////////////////////

int ConstantColorShader::shade(Stroke& stroke) const
{
	StrokeInternal::StrokeVertexIterator v, vend;
	for (v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		v->attribute().setColor(_color[0], _color[1], _color[2]);
		v->attribute().setAlpha(_color[3]);
	}
	return 0;
}

int IncreasingColorShader::shade(Stroke& stroke) const
{
	StrokeInternal::StrokeVertexIterator v, vend;
	int n = stroke.strokeVerticesSize() - 1, yo;
	float newcolor[4];
	for (yo = 0, v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd();
	     v != vend;
	     ++v, ++yo)
	{
		for (int i = 0; i < 4; ++i) {
			newcolor[i] = (1.0 - (float) yo / (float)n) * _colorMin[i] + (float)yo / (float)n * _colorMax[i];
		}
		v->attribute().setColor(newcolor[0], newcolor[1], newcolor[2]);
		v->attribute().setAlpha(newcolor[3]);
	}
	return 0;
}

int MaterialColorShader::shade(Stroke& stroke) const
{
	Interface0DIterator v, vend;
	Functions0D::MaterialF0D fun;
	StrokeVertex *sv;
	for (v = stroke.verticesBegin(), vend = stroke.verticesEnd(); v != vend; ++v) {
		if (fun(v) < 0)
			return -1;
		const float *diffuse = fun.result.diffuse();
		sv = dynamic_cast<StrokeVertex*>(&(*v));
		sv->attribute().setColor(diffuse[0] * _coefficient, diffuse[1] * _coefficient, diffuse[2] * _coefficient);
		sv->attribute().setAlpha(diffuse[3]);
	}
	return 0;
}

ColorNoiseShader::ColorNoiseShader() : StrokeShader()
{
	_amplitude = 1.0f;
	_scale = 1.0f / 2.0f / (float)NB_VALUE_NOISE;
}

ColorNoiseShader::ColorNoiseShader(float iAmplitude, float iPeriod) : StrokeShader()
{
	_amplitude = iAmplitude;
	_scale = 1.0f / iPeriod / (float)NB_VALUE_NOISE;
}

int ColorNoiseShader::shade(Stroke& stroke) const
{
	StrokeInternal::StrokeVertexIterator v = stroke.strokeVerticesBegin(), vend;
	real initU = v->strokeLength() * real(NB_VALUE_NOISE) + RandGen::drand48() * real(NB_VALUE_NOISE);

	real bruit;
	PseudoNoise mynoise;
	for (vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		bruit = mynoise.turbulenceSmooth(_scale * v->curvilinearAbscissa() + initU, 2); // 2 : nbOctaves
		const float *originalColor = v->attribute().getColor();
		float r = bruit * _amplitude + originalColor[0];
		float g = bruit * _amplitude + originalColor[1];
		float b = bruit * _amplitude + originalColor[2];
		v->attribute().setColor(r, g, b);
	}
	return 0;
}

//
//  Texture Shaders
//
///////////////////////////////////////////////////////////////////////////////

int BlenderTextureShader::shade(Stroke& stroke) const
{
	if (_mtex)
		return stroke.setMTex(_mtex);
	if (_nodeTree) {
		stroke.setNodeTree(_nodeTree);
		return 0;
	}
	return -1;
}

int StrokeTextureStepShader::shade(Stroke& stroke) const
{
	stroke.setTextureStep(_step);
	return 0;
}

//
//  Geometry Shaders
//
///////////////////////////////////////////////////////////////////////////////

int BackboneStretcherShader::shade(Stroke& stroke) const
{
	float l = stroke.getLength2D();
	if (l <= 1.0e-6)
		return 0;

	StrokeInternal::StrokeVertexIterator v0 = stroke.strokeVerticesBegin();
	StrokeInternal::StrokeVertexIterator v1 = v0;
	++v1;
	StrokeInternal::StrokeVertexIterator vn = stroke.strokeVerticesEnd();
	--vn;
	StrokeInternal::StrokeVertexIterator vn_1 = vn;
	--vn_1;


	Vec2d first((v0)->x(), (v0)->y());
	Vec2d last((vn)->x(), (vn)->y());

	Vec2d d1(first - Vec2d((v1)->x(), (v1)->y()));
	d1.normalize();
	Vec2d dn(last - Vec2d((vn_1)->x(), (vn_1)->y()));
	dn.normalize();

	Vec2d newFirst(first + _amount * d1);
	(v0)->setPoint(newFirst[0], newFirst[1]);
	Vec2d newLast(last + _amount * dn);
	(vn)->setPoint(newLast[0], newLast[1]);

	stroke.UpdateLength();
	return 0;
}

int SamplingShader::shade(Stroke& stroke) const
{
	stroke.Resample(_sampling);
	stroke.UpdateLength();
	return 0;
}

int ExternalContourStretcherShader::shade(Stroke& stroke) const
{
	//float l = stroke.getLength2D();
	Interface0DIterator it;
	Functions0D::Normal2DF0D fun;
	StrokeVertex *sv;
	for (it = stroke.verticesBegin(); !it.isEnd(); ++it) {
		if (fun(it) < 0)
			return -1;
		Vec2f n(fun.result);
		sv = dynamic_cast<StrokeVertex*>(&(*it));
		Vec2d newPoint(sv->x() + _amount * n.x(), sv->y() + _amount * n.y());
		sv->setPoint(newPoint[0], newPoint[1]);
	}
	stroke.UpdateLength();
	return 0;
}


//!! Bezier curve stroke shader
int BezierCurveShader::shade(Stroke& stroke) const
{
	if (stroke.strokeVerticesSize() < 4)
		return 0;

	// Build the Bezier curve from this set of data points:
	vector<Vec2d> data;
	StrokeInternal::StrokeVertexIterator v = stroke.strokeVerticesBegin(), vend;
	data.push_back(Vec2d(v->x(), v->y())); //first one
	StrokeInternal::StrokeVertexIterator previous = v;
	++v;
	for (vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		if (!((fabs(v->x() - (previous)->x()) < M_EPSILON) && ((fabs(v->y() - (previous)->y()) < M_EPSILON))))
			data.push_back(Vec2d(v->x(), v->y()));
		previous = v;
	}

	// here we build the bezier curve
	BezierCurve bcurve(data, _error);

	// bad performances are here !!! // FIXME
	vector<Vec2d> CurveVertices;
	vector<BezierCurveSegment*>& bsegments = bcurve.segments();
	vector<BezierCurveSegment*>::iterator s = bsegments.begin(), send;
	vector<Vec2d>& segmentsVertices = (*s)->vertices();
	vector<Vec2d>::iterator p, pend;
	// first point
	CurveVertices.push_back(segmentsVertices[0]);
	for (send = bsegments.end(); s != send; ++s) {
		segmentsVertices = (*s)->vertices();
		p = segmentsVertices.begin();
		++p;
		for (pend = segmentsVertices.end(); p != pend; ++p) {
			CurveVertices.push_back((*p));
		}
	}

	// Resample the Stroke depending on the number of vertices of the bezier curve:
	int originalSize = CurveVertices.size();
#if 0
	float sampling = stroke.ComputeSampling(originalSize);
	stroke.Resample(sampling);
#endif
	stroke.Resample(originalSize);
	int newsize = stroke.strokeVerticesSize();
	int nExtraVertex = 0;
	if (newsize < originalSize) {
		cerr << "Warning: unsufficient resampling" << endl;
	}
	else {
		nExtraVertex = newsize - originalSize;
		if (nExtraVertex != 0) {
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "Bezier Shader : Stroke " << stroke.getId() << " have not been resampled" << endl;
			}
		}
	}

	// assigns the new coordinates:
	p = CurveVertices.begin();
	vector<Vec2d>::iterator last = p;
	int n;
	StrokeInternal::StrokeVertexIterator it, itend;
	for (n = 0, it = stroke.strokeVerticesBegin(), itend = stroke.strokeVerticesEnd(), pend = CurveVertices.end();
	     (it != itend) && (p != pend);
	     ++it, ++p, ++n)
	{
		it->setX(p->x());
		it->setY(p->y());
		last = p;
	}
	stroke.UpdateLength();

	// Deal with extra vertices:
	if (nExtraVertex == 0)
		return 0;

	// nExtraVertex should stay unassigned
	vector<StrokeAttribute> attributes;
	vector<StrokeVertex*> verticesToRemove;
	for (int i = 0; i < nExtraVertex; ++i, ++it, ++n) {
		verticesToRemove.push_back(&(*it));
		if (it.isEnd()) {
			// XXX Shocking! :P Shouldn't we break in this case???
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "messed up!" << endl;
			}
		}
	}
	for (it = stroke.strokeVerticesBegin(); it != itend; ++it) {
		attributes.push_back(it->attribute());
	}

	for (vector<StrokeVertex*>::iterator vr = verticesToRemove.begin(), vrend = verticesToRemove.end();
	     vr != vrend;
	     ++vr)
	{
		stroke.RemoveVertex(*vr);
	}

	vector<StrokeAttribute>::iterator a = attributes.begin(), aend = attributes.end();
	int index = 0;
	int index1 = (int)floor((float)originalSize / 2.0);
	int index2 = index1 + nExtraVertex;
	for (it = stroke.strokeVerticesBegin(), itend = stroke.strokeVerticesEnd();
	     (it != itend) && (a != aend);
	     ++it)
	{
		(it)->setAttribute(*a);
		if ((index <= index1) || (index > index2)) {
			++a;
		}
		++index;
	}
	return 0;
}


class CurvePiece
{
public:
	StrokeInternal::StrokeVertexIterator _begin;
	StrokeInternal::StrokeVertexIterator _last;
	Vec2d A;
	Vec2d B;
	int size;
	float _error;

	CurvePiece(StrokeInternal::StrokeVertexIterator b, StrokeInternal::StrokeVertexIterator l, int iSize)
	{
		_error = 0.0f;
		_begin = b;
		_last = l;
		A = Vec2d((_begin)->x(), (_begin)->y());
		B = Vec2d((_last)->x(), (_last)->y());
		size = iSize;
	}

	float error()
	{
		float maxE = 0.0f;
		for (StrokeInternal::StrokeVertexIterator it = _begin; it != _last; ++it) {
			Vec2d P(it->x(), it->y());
			float d = GeomUtils::distPointSegment(P, A, B);
			if (d > maxE)
				maxE = d;
		}
		_error = maxE;
		return maxE;
	}

	//! Subdivides the curve into two pieces.
	//  The first piece is this same object (modified)
	//  The second piece is returned by the method
	CurvePiece *subdivide()
	{
		StrokeInternal::StrokeVertexIterator it = _begin;
		int ns = size - 1; // number of segments (ns > 1)
		int ns1 = ns / 2;
		int ns2 = ns - ns1;
		for (int i = 0; i < ns1; ++it, ++i);

		CurvePiece *second = new CurvePiece(it, _last, ns2 + 1);
		size = ns1 + 1;
		_last = it;
		B = Vec2d((_last)->x(), (_last)->y());
		return second;
	}
};

int PolygonalizationShader::shade(Stroke& stroke) const
{
	vector<CurvePiece*> _pieces;
	vector<CurvePiece*> _results;
	vector<CurvePiece*>::iterator cp, cpend;

	// Compute first approx:
	StrokeInternal::StrokeVertexIterator a = stroke.strokeVerticesBegin();
	StrokeInternal::StrokeVertexIterator b = stroke.strokeVerticesEnd();
	--b;
	int size = stroke.strokeVerticesSize();

	CurvePiece *piece = new CurvePiece(a, b, size);
	_pieces.push_back(piece);

	while (!_pieces.empty()) {
		piece = _pieces.back();
		_pieces.pop_back();
		if (piece->size > 2 && piece->error() > _error) {
			CurvePiece *second = piece->subdivide();
			_pieces.push_back(second);
			_pieces.push_back(piece);
		}
		else {
			_results.push_back(piece);
		}
	}

	// actually modify the geometry for each piece:
	for (cp = _results.begin(), cpend = _results.end(); cp != cpend; ++cp) {
		a = (*cp)->_begin;
		b = (*cp)->_last;
		Vec2d u = (*cp)->B - (*cp)->A;
		Vec2d n(u[1], -u[0]);
		n.normalize();
		//Vec2d n(0, 0);
		float offset = ((*cp)->_error);
		StrokeInternal::StrokeVertexIterator v;
		for (v = a; v != b; ++v) {
			v->setPoint((*cp)->A.x() + v->u() * u.x() + n.x() * offset,
			            (*cp)->A.y() + v->u() * u.y() + n.y() * offset);
		}
#if 0
		u.normalize();
		(*a)->setPoint((*a)->x() - u.x() * 10, (*a)->y() - u.y() * 10);
#endif
	}
	stroke.UpdateLength();

	// delete stuff
	for (cp = _results.begin(), cpend = _results.end(); cp != cpend; ++cp) {
		delete (*cp);
	}
	_results.clear();
	return 0;
}

int GuidingLinesShader::shade(Stroke& stroke) const
{
	Functions1D::Normal2DF1D norm_fun;
	StrokeInternal::StrokeVertexIterator a = stroke.strokeVerticesBegin();
	StrokeInternal::StrokeVertexIterator b = stroke.strokeVerticesEnd();
	--b;
	int size = stroke.strokeVerticesSize();
	CurvePiece piece(a, b, size);

	Vec2d u = piece.B - piece.A;
	Vec2f n(u[1], -u[0]);
	n.normalize();
	if (norm_fun(stroke) < 0)
		return -1;
	Vec2f strokeN(norm_fun.result);
	if (n * strokeN < 0) {
		n[0] = -n[0];
		n[1] = -n[1];
	}
	float offset = (piece.error()) / 2.0f * _offset;
	StrokeInternal::StrokeVertexIterator v, vend;
	for (v = a, vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		v->setPoint(piece.A.x() + v->u() * u.x() + n.x() * offset,
		            piece.A.y() + v->u() * u.y() + n.y() * offset);
	}
	stroke.UpdateLength();
	return 0;
}

/////////////////////////////////////////
//
//  Tip Remover
//
/////////////////////////////////////////


TipRemoverShader::TipRemoverShader(real tipLength) : StrokeShader()
{
	_tipLength = tipLength;
}

int TipRemoverShader::shade(Stroke& stroke) const
{
	int originalSize = stroke.strokeVerticesSize();

	if (originalSize < 4)
		return 0;

	StrokeInternal::StrokeVertexIterator v, vend;
	vector<StrokeVertex*> verticesToRemove;
	vector<StrokeAttribute> oldAttributes;
	for (v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd(); v != vend; ++v) {
		if ((v->curvilinearAbscissa() < _tipLength) || (v->strokeLength() - v->curvilinearAbscissa() < _tipLength)) {
			verticesToRemove.push_back(&(*v));
		}
		oldAttributes.push_back(v->attribute());
	}

	if (originalSize - verticesToRemove.size() < 2)
		return 0;

	vector<StrokeVertex*>::iterator sv, svend;
	for (sv = verticesToRemove.begin(), svend = verticesToRemove.end(); sv != svend; ++sv) {
		stroke.RemoveVertex((*sv));
	}

	// Resample so that our new stroke have the same number of vertices than before
	stroke.Resample(originalSize);

	if ((int)stroke.strokeVerticesSize() != originalSize) //soc
		cerr << "Warning: resampling problem" << endl;

	// assign old attributes to new stroke vertices:
	vector<StrokeAttribute>::iterator a = oldAttributes.begin(), aend = oldAttributes.end();
	for (v = stroke.strokeVerticesBegin(), vend = stroke.strokeVerticesEnd();
	     (v != vend) && (a != aend);
	     ++v, ++a)
	{
		v->setAttribute(*a);
	}
	// we're done!
	return 0;
}

} // end of namespace StrokeShaders

} /* namespace Freestyle */
