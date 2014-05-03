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

struct MTex;

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

/*! [ Thickness Shader ].
*  Applys a pattern (texture) to vary thickness.
*  The new thicknesses are the result of the multiplication
*  of the pattern and the original thickness
*/
class ThicknessVariationPatternShader : public StrokeShader
{
public:
	/*! Builds the shader.
	 *  \param pattern_name
	 *    The texture file name.
	 *  \param iMinThickness
	 *    The minimum thickness we don't want to exceed.
	 *  \param iMaxThickness
	 *    The maximum thickness we don't want to exceed.
	 *  \param stretch
	 *    Tells whether the pattern texture must be stretched or repeted to fit the stroke.
	 */
	ThicknessVariationPatternShader(const string pattern_name, float iMinThickness = 1.0f, float iMaxThickness = 5.0f,
	                                bool stretch = true);

	/*! Destructor.*/
	virtual ~ThicknessVariationPatternShader()
	{
		if (0 != _aThickness) {
			delete[] _aThickness;
			_aThickness = 0;
		}
	}

	virtual string getName() const
	{
		return "ThicknessVariationPatternShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;

private:
	float *_aThickness; // array of thickness values, in % of the max (i.e comprised between 0 and 1)
	unsigned _size;
	float _minThickness;
	float _maxThickness;
	bool _stretch;
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

/*! [ Color Shader ].
 *  Applys a pattern to vary original color.
 *  The new color is the result of the multiplication of the pattern and the original color
 */
class ColorVariationPatternShader : public StrokeShader
{
public:
	/*! Builds the shader from the pattern texture file name.
	 *  \param pattern_name
	 *    The file name of the texture file to use as pattern
	 *  \param stretch
	 *    Tells whether the texture must be strecthed or repeted to fit the stroke.
	 */
	ColorVariationPatternShader(const string pattern_name, bool stretch = true);

	/*! Destructor */
	virtual ~ColorVariationPatternShader()
	{
		if (0 != _aVariation) {
			delete[] _aVariation;
			_aVariation = 0;
		}
	}

	virtual string getName() const
	{
		return "ColorVariationPatternShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;

private:
	float *_aVariation; // array of coef values, in % of the max (i.e comprised between 0 and 1)
	unsigned _size;
	bool _stretch;
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

class CalligraphicColorShader : public StrokeShader
{
private:
	/* UNUSED */
	//  int _textureId;
	Vec2d _orientation;

public:
	CalligraphicColorShader(const Vec2d &iOrientation) : StrokeShader()
	{
		_orientation = iOrientation;
		_orientation.normalize();
	}

	virtual string getName() const
	{
		return "CalligraphicColorShader";
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
//  Texture Shaders
//
///////////////////////////////////////////////////////////////////////////////
/*! [ Texture Shader ].
*  Assigns a texture to the stroke in order to simulate
*  its marks system. This shader takes as input an integer value
*  telling which texture and blending mode to use among a set of
*  predefined textures.
*  Here are the different presets:
*  0) -> /brushes/charcoalAlpha.bmp, HUMID_MEDIUM
*  1) -> /brushes/washbrushAlpha.bmp, HUMID_MEDIUM
*  2) -> /brushes/oil.bmp, HUMID_MEDIUM
*  3) -> /brushes/oilnoblend.bmp, HUMID_MEDIUM
*  4) -> /brushes/charcoalAlpha.bmp, DRY_MEDIUM
*  5) -> /brushes/washbrushAlpha.bmp, DRY_MEDIUM
*  6) -> /brushes/opaqueDryBrushAlpha.bmp, OPAQUE_MEDIUM
*  7) -> /brushes/opaqueBrushAlpha.bmp, Stroke::OPAQUE_MEDIUM
*  Any other value will lead to the following preset:
*  default) -> /brushes/smoothAlpha.bmp, OPAQUE_MEDIUM.
*/
class TextureAssignerShader : public StrokeShader // FIXME
{
private:
	int _textureId;

public:
	/*! Builds the shader.
	 *  \param id
	 *    The number of the preset to use.
	 */
	TextureAssignerShader(int id) : StrokeShader()
	{
		_textureId = id;
	}

	virtual string getName() const
	{
		return "TextureAssignerShader";
	}

	/*! The shading method */
	virtual int shade(Stroke& stroke) const;
};

/*! [ Texture Shader ].
*  Assigns a texture and a blending mode to the stroke
*  in order to simulate its marks system.
*/
class StrokeTextureShader : public StrokeShader
{
private:
	string _texturePath;
	Stroke::MediumType _mediumType;
	bool _tips; // 0 or 1

public:
	/*! Builds the shader from the texture file name and the blending mode to use.
	 *  \param textureFile
	 *    The the texture file name.
	 *    \attention The textures must be placed in the $FREESTYLE_DIR/data/textures/brushes directory.
	 *  \param mediumType
	 *    The medium type and therefore, the blending mode that must be used for the rendering of this stroke.
	 *  \param iTips
	 *    Tells whether the texture includes tips or not.
	 *    If it is the case, the texture image must respect the following format:
	 *    \verbatim
	 *     __________
	 *    |          |
	 *    |    A     |
	 *    |__________|
	 *    |     |    |
	 *    |  B  | C  |
	 *    |_____|____|
	 * 
	 *    \endverbatim
	 *    - A : The stroke's corpus texture
	 *    - B : The stroke's left extremity texture
	 *    - C : The stroke's right extremity texture
	 */
	StrokeTextureShader(const string textureFile, Stroke::MediumType mediumType = Stroke::OPAQUE_MEDIUM,
	                    bool iTips = false)
	: StrokeShader()
	{
		_texturePath = textureFile;
		_mediumType = mediumType;
		_tips = iTips;
	}

	virtual string getName() const
	{
		return "StrokeTextureShader";
	}

	/*! The shading method */
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

// B-Spline stroke shader
class BSplineShader: public StrokeShader
{
public:
	BSplineShader() : StrokeShader() {}

	virtual string getName() const
	{
		return "BSplineShader";
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

/* Shader to inflate the curves. It keeps the extreme points positions and moves the other ones along the 2D normal.
 * The displacement value is proportional to the 2d curvature at the considered point (the higher the curvature,
 * the smaller the displacement) and to a value specified by the user.
 */
class InflateShader : public StrokeShader
{
private:
	float _amount; 
	float _curvatureThreshold;

public:
	/*! Builds an inflate shader
	 *    \param iAmount
	 *      A multiplicative coefficient that acts on the amount and direction of displacement
	 *    \param iThreshold
	 *      The curves having a 2d curvature > iThreshold at one of their points is not inflated
	 */
	InflateShader(float iAmount, float iThreshold) : StrokeShader()
	{
		_amount = iAmount;
		_curvatureThreshold = iThreshold;
	}

	virtual string getName() const
	{
		return "InflateShader";
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

/*! [ output Shader ].
 *  streams the Stroke
 */
class streamShader : public StrokeShader
{
public:
	/*! Destructor. */
	virtual ~streamShader() {}

	/*! Returns the string "streamShader".*/
	virtual string getName() const
	{
		return "streamShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;
};

/*! [ output Shader ].
 *  streams the Stroke in a file
 */
class fstreamShader : public StrokeShader
{
protected:
	mutable ofstream _stream;

public:
	/*! Builds the shader from the output file name */
	fstreamShader(const char *iFileName) : StrokeShader()
	{
		_stream.open(iFileName);
		if (!_stream.is_open()) {
			cerr << "couldn't open file " << iFileName << endl;
		}
	}

	/*! Destructor. */
	virtual ~fstreamShader()
	{
		_stream.close();
	}

	/*! Returns the string "fstreamShader".*/
	virtual string getName() const
	{
		return "fstreamShader";
	}

	/*! The shading method. */
	virtual int shade(Stroke& stroke) const;
};

/*! [ Texture Shader ].
 *  Shader to assign texture to the Stroke material.
 */

class BlenderTextureShader : public StrokeShader
{
private:
	MTex *_mtex;

public:
	/*! Builds the shader.
	 *  \param mtex
	 *    The blender texture to use.
	 */
	BlenderTextureShader(MTex *mtex) : StrokeShader()
	{
		_mtex = mtex;
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
