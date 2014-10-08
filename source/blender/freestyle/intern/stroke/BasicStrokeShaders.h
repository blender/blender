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

#ifndef __FREESTYLE_BASIC_STROKE_SHADERS_H__
#define __FREESTYLE_BASIC_STROKE_SHADERS_H__

/** \file blender/freestyle/intern/stroke/BasicStrokeShaders.h
 *  \ingroup freestyle
 *  \brief Class gathering basic stroke shaders
 *  \author Stephane Grabli
 *  \date 17/12/2002
 */

#include <fstream>

#include "Stroke.h"
#include "StrokeShader.h"

#include "../geometry/Bezier.h"
#include "../geometry/Geom.h"

extern "C" {
struct MTex;
struct bNodeTree;
}

using namespace std;

namespace Freestyle {

using namespace Geometry;

namespace StrokeShaders {

//
//  Thickness modifiers
//
//////////////////////////////////////////////////////

/*! [ Thickness Shader ].
 *  Assigns an absolute constant thickness to every vertices of the Stroke.
 */
class ConstantThicknessShader : public StrokeShader
{
public:
	/*! Builds the shader.
	 *  \param thickness
	 *    The thickness that must be assigned to the stroke.
	 */
	ConstantThicknessShader(float thickness) : StrokeShader()
	{
		_thickness = thickness;
	}

	/*! Destructor. */
	virtual ~ConstantThicknessShader() {}

	/*! Returns the string "ConstantThicknessShader".*/
	virtual string getName() const
	{
		return "ConstantThicknessShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;

private:
	float _thickness;
};

/* [ Thickness Shader ].
 *  Assigns an absolute constant external thickness to every vertices of the Stroke. The external thickness of a point
 *  is its thickness from the point to the strip border in the direction pointing outside the object the
 *  Stroke delimitates.
 */
class ConstantExternThicknessShader : public StrokeShader
{
public:
	ConstantExternThicknessShader(float thickness) : StrokeShader()
	{
		_thickness = thickness;
	}

	virtual ~ConstantExternThicknessShader() {}

	virtual string getName() const
	{
		return "ConstantExternThicknessShader";
	}

	virtual int shade(Stroke& stroke) const;

private:
	float _thickness;
};

/*! [ Thickness Shader ].
 *  Assigns thicknesses values such as the thickness increases from a thickness value A to a thickness value B between
 *  the first vertex to the midpoint vertex and then decreases from B to a A between this midpoint vertex
 *  and the last vertex.
 *  The thickness is linearly interpolated from A to B.
 */
class IncreasingThicknessShader : public StrokeShader
{
public:
	/*! Builds the shader.
	 *  \param iThicknessMin
	 *    The first thickness value.
	 *  \param iThicknessMax
	 *    The second thickness value.
	 */
	IncreasingThicknessShader(float iThicknessMin, float iThicknessMax) : StrokeShader()
	{
		_ThicknessMin = iThicknessMin;
		_ThicknessMax = iThicknessMax;
	}

	/*! Destructor.*/
	virtual ~IncreasingThicknessShader() {}

	virtual string getName() const
	{
		return "IncreasingThicknessShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;

private:
	float _ThicknessMin;
	float _ThicknessMax;
};

/*! [ Thickness shader ].
 *  Same as previous but here we allow the user to control the ratio thickness/length so that  we don't get
 *  fat short lines
 */
class ConstrainedIncreasingThicknessShader : public StrokeShader
{
private:
	float _ThicknessMin;
	float _ThicknessMax;
	float _ratio;

public:
	/*! Builds the shader.
	 *  \param iThicknessMin
	 *    The first thickness value.
	 *  \param iThicknessMax
	 *    The second thickness value.
	 *  \param iRatio
	 *    The ration thickness/length we don't want to exceed.
	 */
	ConstrainedIncreasingThicknessShader(float iThicknessMin, float iThicknessMax, float iRatio) : StrokeShader()
	{
		_ThicknessMin = iThicknessMin;
		_ThicknessMax = iThicknessMax;
		_ratio = iRatio;
	}

	/*! Destructor.*/
	virtual ~ConstrainedIncreasingThicknessShader() {}

	virtual string getName() const
	{
		return "ConstrainedIncreasingThicknessShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;
};

/*  [ Thickness Shader ].
 *  Modifys the thickness in a relative way depending on its length.
 */
class LengthDependingThicknessShader : public StrokeShader
{
private:
	float _minThickness;
	float _maxThickness;
	// We divide the strokes in 4 categories:
	// l > 300
	// 100 < l < 300
	// 50 < l < 100
	// l < 50

public:
	LengthDependingThicknessShader(float iMinThickness, float iMaxThickness) : StrokeShader()
	{
		_minThickness = iMinThickness;
		_maxThickness = iMaxThickness;
	}

	virtual ~LengthDependingThicknessShader() {}

	virtual string getName() const
	{
		return "LengthDependingThicknessShader";
	}

	virtual int shade(Stroke& stroke) const;
};


/*!  [ Thickness Shader ].
 *   Adds some noise to the stroke thickness.
 *   \see \htmlonly <a href=noise/noise.html>noise/noise.html</a>\endhtmlonly
 */
class ThicknessNoiseShader : public StrokeShader
{
private:
	float _amplitude;
	float _scale;

public:
	ThicknessNoiseShader();

	/*! Builds a Thickness Noise Shader
	 *    \param iAmplitude
	 *      The amplitude of the noise signal
	 *    \param iPeriod
	 *      The period of the noise signal
	 */
	ThicknessNoiseShader(float iAmplitude, float iPeriod);

	virtual string getName() const
	{
		return "ThicknessNoiseShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;
};


//
//  Color shaders
//
/////////////////////////////////////////////////////////
/*!  [ Color Shader ].
 *   Assigns a constant color to every vertices of the Stroke.
 */
class ConstantColorShader : public StrokeShader
{
public:
	/*! Builds the shader from a user-specified color.
	 *  \param iR
	 *    The red component
	 *  \param iG
	 *    The green component
	 *  \param iB
	 *    The blue component
	 *  \param iAlpha
	 *    The alpha value
	 */
	ConstantColorShader(float iR, float iG, float iB, float iAlpha = 1.0f) : StrokeShader()
	{
		_color[0] = iR;
		_color[1] = iG;
		_color[2] = iB;
		_color[3] = iAlpha;
	}

	virtual string getName() const
	{
		return "ConstantColorShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;

private:
	float _color[4];
};

/*!  [ Color Shader ].
 *   Assigns a varying color to the stroke.
 *   The user specifies 2 colors A and B. The stroke color will change linearly from A to B between the
 *   first and the last vertex.
 */
class IncreasingColorShader : public StrokeShader
{
private:
	float _colorMin[4];
	float _colorMax[4];

public:
	/*! Builds the shader from 2 user-specified colors.
	 *  \param iRm
	 *    The first color red component
	 *  \param iGm
	 *    The first color green component
	 *  \param iBm
	 *    The first color blue component
	 *  \param iAlpham
	 *    The first color alpha value
	 *  \param iRM
	 *    The second color red component
	 *  \param iGM
	 *    The second color green component
	 *  \param iBM
	 *    The second color blue component
	 *  \param iAlphaM
	 *    The second color alpha value
	 */
	IncreasingColorShader(float iRm, float iGm, float iBm, float iAlpham,
	                      float iRM, float iGM, float iBM, float iAlphaM)
	: StrokeShader()
	{
		_colorMin[0] = iRm;
		_colorMin[1] = iGm;
		_colorMin[2] = iBm;
		_colorMin[3] = iAlpham;

		_colorMax[0] = iRM;
		_colorMax[1] = iGM;
		_colorMax[2] = iBM;
		_colorMax[3] = iAlphaM;
	}

	virtual string getName() const
	{
		return "IncreasingColorShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;
};

/* [ Color Shader ].
 *  Assigns a color to the stroke depending on the material of the shape to which ot belongs to. (Disney shader)
 */
class MaterialColorShader : public StrokeShader
{
private:
	float _coefficient;

public:
	MaterialColorShader(float coeff = 1.0f) : StrokeShader()
	{
		_coefficient = coeff;
	}

	virtual string getName() const
	{
		return "MaterialColorShader";
	}

	virtual int shade(Stroke& stroke) const;
};

/*! [ Color Shader ].
 *  Shader to add noise to the stroke colors.
 */
class ColorNoiseShader : public StrokeShader
{
private:
	float _amplitude;
	float _scale;

public:
	ColorNoiseShader();

	/*! Builds a Color Noise Shader
	 *    \param iAmplitude
	 *      The amplitude of the noise signal
	 *    \param iPeriod
	 *      The period of the noise signal
	 */
	ColorNoiseShader(float iAmplitude, float iPeriod);

	virtual string getName() const
	{
		return "ColorNoiseShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;
};

//
//  Geometry Shaders
//
///////////////////////////////////////////////////////////////////////////////
/*! [ Geometry Shader ].
 *  Stretches the stroke at its two extremities and following the respective directions: v(1)v(0) and v(n-1)v(n).
 */
class BackboneStretcherShader : public StrokeShader
{
private:
	float _amount; 

public:
	/*! Builds the shader.
	 *  \param iAmount
	 *    The stretching amount value.
	 */
	BackboneStretcherShader(float iAmount = 2.0f) : StrokeShader()
	{
		_amount = iAmount;
	}

	virtual string getName() const
	{
		return "BackboneStretcherShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};

/*! [ Geometry Shader. ]
 *  Resamples the stroke.
 *  @see Stroke::Resample(float).
 */
class SamplingShader: public StrokeShader
{
private:
	float _sampling;

public:
	/*! Builds the shader.
	 *  \param sampling
	 *    The sampling to use for the stroke resampling
	 */
	SamplingShader(float sampling) : StrokeShader()
	{
		_sampling = sampling;
	}

	virtual string getName() const
	{
		return "SamplingShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};


class ExternalContourStretcherShader : public StrokeShader
{
private:
	float _amount;

public:
	ExternalContourStretcherShader(float iAmount = 2.0f) : StrokeShader()
	{
		_amount = iAmount;
	}

	virtual string getName() const
	{
		return "ExternalContourStretcherShader";
	}

	virtual int shade(Stroke& stroke) const;
};


// Bezier curve stroke shader
/*! [ Geometry Shader ].
 *  Transforms the stroke backbone geometry so that it corresponds to a Bezier Curve approximation of the
 *  original backbone geometry.
 *  @see \htmlonly <a href=bezier/bezier.html>bezier/bezier.html</a> \endhtmlonly
 */
class BezierCurveShader : public StrokeShader
{
private:
	float _error;

public:
	/*! Builds the shader.
	 *  \param error
	 *    The error we're allowing for the approximation.
	 *    This error is the max distance allowed between the new curve and the original geometry.
	 */
	BezierCurveShader(float error = 4.0) : StrokeShader()
	{
		_error = error;
	}

	virtual string getName() const
	{
		return "BezierCurveShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};


/*! [ Geometry Shader ].
 *  Shader to modify the Stroke geometry so that it looks more "polygonal".
 *  The basic idea is to start from the minimal stroke approximation consisting in a line joining the first vertex
 *  to the last one and to subdivide using the original stroke vertices until a certain error is reached.
 */
class PolygonalizationShader : public StrokeShader
{
private:
	float _error;

public:
	/*! Builds the shader.
	 *  \param iError
	 *    The error we want our polygonal approximation to have with respect to the original geometry.
	 *    The smaller, the closer the new stroke to the orinal one.
	 *    This error corresponds to the maximum distance between the new stroke and the old one.
	 */
	PolygonalizationShader(float iError) : StrokeShader() 
	{
		_error = iError;
	}

	virtual string getName() const
	{
		return "PolygonalizationShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};


/*! [ Geometry Shader ].
 *  Shader to modify the Stroke geometry so that it corresponds to its main direction line.
 *  This shader must be used together with the splitting operator using the curvature criterion.
 *  Indeed, the precision of the approximation will depend on the size of the stroke's pieces.
 *  The bigger the pieces, the rougher the approximation.
 */
class GuidingLinesShader : public StrokeShader
{
private:
	float _offset;

public:
	/*! Builds a Guiding Lines shader
	 *    \param iOffset
	 *      The line that replaces the stroke is initially in the middle of the initial stroke "bbox".
	 *      iOffset is the value of the displacement which is applied to this line along its normal.
	 */
	GuidingLinesShader(float iOffset) : StrokeShader()
	{
		_offset = iOffset;
	}

	virtual string getName() const
	{
		return "GuidingLinesShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};

/*! [ Geometry Shader ].
 *  Removes the stroke's extremities.
 */
class TipRemoverShader : public StrokeShader
{
public:
	/*! Builds the shader.
	 *  \param tipLength
	 *    The length of the piece of stroke we want to remove at each extremity.
	 */
	TipRemoverShader (real tipLength);

	/*! Destructor. */
	virtual ~TipRemoverShader () {}

	/*! The shading method */
	virtual string getName() const
	{
		return "TipRemoverShader";
	}

	virtual int shade(Stroke &stroke) const;

protected:
	real _tipLength; 
};

/*! [ Texture Shader ].
 *  Shader to assign texture to the Stroke material.
 */

class BlenderTextureShader : public StrokeShader
{
private:
	MTex *_mtex;
	bNodeTree *_nodeTree;

public:
	/*! Builds the shader.
	 *  \param mtex
	 *    The blender texture to use.
	 */
	BlenderTextureShader(MTex *mtex) : StrokeShader()
	{
		_mtex = mtex;
		_nodeTree = NULL;
	}

	/*! Builds the shader.
	*  \param nodetree
	*    A node tree (of new shading nodes) to define textures.
	*/
	BlenderTextureShader(bNodeTree *nodetree) : StrokeShader()
	{
		_nodeTree = nodetree;
		_mtex = NULL;
	}

	virtual string getName() const
	{
		return "BlenderTextureShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};

/*! [ Texture Shader ].
 *  Shader to assign texture to the Stroke material.
 */

class StrokeTextureStepShader : public StrokeShader
{
private:
	float _step;

public:
	/*! Builds the shader.
	 *  \param id
	 *    The number of the preset to use.
	 */
	StrokeTextureStepShader(float step) : StrokeShader()
	{
		_step = step;
	}

	virtual string getName() const
	{
		return "StrokeTextureStepShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};

} // end of namespace StrokeShaders

} /* namespace Freestyle */

#endif // __FREESTYLE_BASIC_STROKE_SHADERS_H__
