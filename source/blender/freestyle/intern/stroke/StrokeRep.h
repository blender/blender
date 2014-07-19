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

#ifndef __FREESTYLE_STROKE_REP_H__
#define __FREESTYLE_STROKE_REP_H__

/** \file blender/freestyle/intern/stroke/StrokeRep.h
 *  \ingroup freestyle
 *  \brief Class to define the representation of a stroke (for display purpose)
 *  \author Stephane Grabli
 *  \date 05/03/2003
 */

#include "Stroke.h"

#include "../geometry/Geom.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

extern "C" {
#include "DNA_material_types.h" // for MAX_MTEX
struct FreestyleLineStyle;
}

namespace Freestyle {

using namespace Geometry;

#if 0
//symbolic constant to call the appropriate renderers and textures
#define NO_TEXTURE_WITH_BLEND_STROKE  -2
#define NO_TEXTURE_STROKE             -1
#define PSEUDO_CHARCOAL_STROKE         0
#define WASH_BRUSH_STROKE              1
#define OIL_STROKE                     2
#define NO_BLEND_STROKE                3
#define CHARCOAL_MIN_STROKE            4
#define BRUSH_MIN_STROKE               5
#define OPAQUE_DRY_STROKE              6
#define OPAQUE_STROKE                  7

#define DEFAULT_STROKE                 0

#define NUMBER_STROKE_RENDERER         8

#endif

class StrokeVertexRep
{
public:
	StrokeVertexRep() {}

	StrokeVertexRep(const Vec2r& iPoint2d)
	{
		_point2d = iPoint2d;
	}

	StrokeVertexRep(const StrokeVertexRep& iBrother);

	virtual ~StrokeVertexRep() {}

	inline Vec2r& point2d()
	{
		return _point2d;
	}

	inline Vec2r& texCoord(bool tips=false)
	{
		if (tips) {
			return _texCoord_w_tips;
		}
		else
			return _texCoord;
	}

	inline Vec3r& color()
	{
		return _color;
	}

	inline float alpha()
	{
		return _alpha;
	}

	inline void setPoint2d(const Vec2r& p)
	{
		_point2d = p;
	}

	inline void setTexCoord(const Vec2r& p, bool tips=false)
	{
		if (tips) {
			_texCoord_w_tips = p;
		}
		else {
			_texCoord = p;
		}
	}

	inline void setColor(const Vec3r& p)
	{
		_color = p;
	}

	inline void setAlpha(float a)
	{
		_alpha = a;
	}

protected:
	Vec2r _point2d;
	Vec2r _texCoord;
	Vec2r _texCoord_w_tips;
	Vec3r _color;
	float _alpha;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeVertexRep")
#endif
};

class Strip
{
public:
	typedef std::vector<StrokeVertexRep*> vertex_container;

protected:
	vertex_container _vertices;
	float _averageThickness;

public:
	Strip(const std::vector<StrokeVertex*>& iStrokeVertices, bool hasTex = false,
			bool tipBegin = false, bool tipEnd = false, float texStep = 1.0);
	Strip(const Strip& iBrother);
	virtual ~Strip();

protected:
	void createStrip(const std::vector<StrokeVertex*>& iStrokeVertices);
	void cleanUpSingularities(const std::vector<StrokeVertex*>& iStrokeVertices);
	void setVertexColor (const std::vector<StrokeVertex*>& iStrokeVertices);
	void computeTexCoord (const std::vector<StrokeVertex*>& iStrokeVertices, float texStep);
	void computeTexCoordWithTips (const std::vector<StrokeVertex*>& iStrokeVertices, bool tipBegin, bool tipEnd, float texStep);

public:
	inline int sizeStrip() const
	{
		return _vertices.size();
	}

	inline vertex_container& vertices()
	{
		return _vertices;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Strip")
#endif
};

class StrokeRep
{
protected:
	Stroke *_stroke;
	vector<Strip*> _strips;
	Stroke::MediumType _strokeType;
	unsigned int _textureId;
	float _textureStep;
	MTex *_mtex[MAX_MTEX];
	Material *_material;
	FreestyleLineStyle *_lineStyle;
	bool _useShadingNodes;
	bool _hasTex;

	// float _averageTextureAlpha;

public:
	StrokeRep();
	StrokeRep(const StrokeRep&);
	StrokeRep(Stroke *iStroke);
	virtual ~StrokeRep();

	/*! Creates the strips */
	virtual void create();

	/*! Renders the stroke using a Renderer */
	virtual void Render(const StrokeRenderer *iRenderer);

	/*! accessors */
	inline Stroke::MediumType getMediumType() const
	{
		return _strokeType;
	}

	inline unsigned getTextureId() const
	{
		return _textureId;
	}

	inline MTex *getMTex(int idx) const
	{
		return _mtex[idx];
	}

	inline Material *getMaterial() const
	{
		return _material;
	}

	inline FreestyleLineStyle *getLineStyle() const
	{
		return _lineStyle;
	}

	inline bool useShadingNodes() const
	{
		return _useShadingNodes;
	}

	inline bool hasTex() const
	{
		return _hasTex;
	}

	inline vector<Strip*>& getStrips()
	{
		return _strips;
	}

	inline unsigned int getNumberOfStrips() const
	{
		return _strips.size();
	}

	inline Stroke *getStroke()
	{
		return _stroke;
	}

	/*! modifiers */
	inline void setMediumType(Stroke::MediumType itype)
	{
		_strokeType = itype;
	}

	inline void setTextureId(unsigned textureId)
	{
		_textureId = textureId;
	}

	inline void setMaterial(Material *mat)
	{
		_material = mat;
	}
	/*
	inline void setMTex(int idx, MTex *mtex_ptr)
	{
		_mtex[idx] = mtex_ptr;
	}*/

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeRep")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_STROKE_REP_H__
