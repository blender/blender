/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef __RAS_IRASTERIZER
#define __RAS_IRASTERIZER

#ifdef WIN32
#pragma warning (disable:4786)
#endif

#include "MT_CmMatrix4x4.h"
#include "MT_Matrix4x4.h"

class RAS_ICanvas;
class RAS_IPolyMaterial;
#include "RAS_MaterialBucket.h"

/**
 * 3D rendering device context interface. 
 */
class RAS_IRasterizer
{

public:

	RAS_IRasterizer(RAS_ICanvas* canv){};
	virtual ~RAS_IRasterizer(){};
	/**
	 */
	enum	{
			RAS_RENDER_3DPOLYGON_TEXT = 16384
	};
	/**
	 * Drawing types
	 */
	enum	{
			KX_BOUNDINGBOX = 1,
			KX_WIREFRAME,
			KX_SOLID,
			KX_SHADED,
			KX_TEXTURED 
	};

	/**
	 * Valid SetDepthMask parameters
	 */
	enum	{
			KX_DEPTHMASK_ENABLED =1,
			KX_DEPTHMASK_DISABLED
	};

	/**
	 */
	enum    { 	 
			KX_TWOSIDE = 512, 	 
			KX_LINES = 32768 	 
	};

	/**
	 * Stereo mode types
	 */
	enum	{
			RAS_STEREO_NOSTEREO = 1,
			RAS_STEREO_QUADBUFFERED,
			RAS_STEREO_ABOVEBELOW,
			RAS_STEREO_INTERLACED
	};
	/**
	 * Render pass identifiers for stereo.
	 */
	enum	{
			RAS_STEREO_LEFTEYE = 1,
			RAS_STEREO_RIGHTEYE
	};

	/**
	 * SetDepthMask enables or disables writing a fragment's depth value
	 * to the Z buffer.
	 */
	virtual void	SetDepthMask(int depthmask)=0;
	/**
	 * SetMaterial sets the material settings for subsequent primitives
	 * to be rendered with.
	 * The material will be cached.
	 */
	virtual void	SetMaterial(const RAS_IPolyMaterial& mat)=0;
	/**
	 * Init initialises the renderer.
	 */
	virtual bool	Init()=0;
	/**
	 * Exit cleans up the renderer.
	 */
	virtual void	Exit()=0;
	/**
	 * BeginFrame is called at the start of each frame.
	 */
	virtual bool	BeginFrame(int drawingmode, double time)=0;
	/**
	 * ClearDepthBuffer clears the depth buffer.
	 */
	virtual void	ClearDepthBuffer()=0;
	/**
	 * ClearCachingInfo clears the currently cached material.
	 */
	virtual void	ClearCachingInfo(void)=0;
	/**
	 * EndFrame is called at the end of each frame.
	 */
	virtual void	EndFrame()=0;
	/**
	 * SetRenderArea sets the render area from the 2d canvas
	 */
	virtual void	SetRenderArea()=0;

	// Stereo Functions
	/**
	 * SetStereoMode will set the stereo mode
	 */
	virtual void	SetStereoMode(const int stereomode)=0;
	/**
	 * Stereo can be used to query if the rasterizer is in stereo mode.
	 * @return true if stereo mode is enabled.
	 */
	virtual bool	Stereo()=0;
	/**
	 * Sets which eye buffer subsequent primitives will be rendered to.
	 */
	virtual void	SetEye(const int eye)=0;
	/**
	 */
	virtual void	SetEyeSeparation(const float eyeseparation)=0;
	/**
	 */
	virtual void	SetFocalLength(const float focallength)=0;
	/**
	 * SwapBuffers swaps the back buffer with the front buffer.
	 */
	virtual void	SwapBuffers()=0;
	
	// Drawing Functions
	/**
	 * IndexPrimitives: Renders primitives.
	 * @param vertexarrays is an array of vertex arrays
	 * @param indexarrays is an array of index arrays
	 * @param mode determines the type of primitive stored in the vertex/index arrays:
	 *              0 triangles
	 *              1 lines (default)
	 *              2 quads
	 * @param polymat (reserved)
	 * @param useObjectColor will render the object using @param rgbacolor instead of 
	 *  vertex colours.
	 */
	virtual void	IndexPrimitives( const vecVertexArray& vertexarrays,
							const vecIndexArrays & indexarrays,
							int mode,
							class RAS_IPolyMaterial* polymat,
							class RAS_IRenderTools* rendertools,
							bool useObjectColor,
							const MT_Vector4& rgbacolor)=0;
	/**
	 * @copydoc IndexPrimitives
	 * IndexPrimitivesEx will renormalize faces if @param vertexarrays[i].getFlag() & TV_CALCFACENORMAL
	 */
	virtual void	IndexPrimitives_Ex( const vecVertexArray& vertexarrays,
							const vecIndexArrays & indexarrays,
							int mode,
							class RAS_IPolyMaterial* polymat,
							class RAS_IRenderTools* rendertools,
							bool useObjectColor,
							const MT_Vector4& rgbacolor)=0;
	/**
	 * IndexPrimitives_3DText will render text into the polygons.
	 * The text to be rendered is from @param rendertools client object's text property.
	 */
	virtual void	IndexPrimitives_3DText( const vecVertexArray& vertexarrays,
							const vecIndexArrays & indexarrays,
							int mode,
							class RAS_IPolyMaterial* polymat,
							class RAS_IRenderTools* rendertools,
							bool useObjectColor,
							const MT_Vector4& rgbacolor)=0;

	virtual void	SetProjectionMatrix(MT_CmMatrix4x4 & mat)=0;
	/* This one should become our final version, methinks. */
	/**
	 * Set the projection matrix for the rasterizer. This projects
	 * from camera coordinates to window coordinates.
	 * @param mat The projection matrix.
	 */
	virtual void	SetProjectionMatrix(const MT_Matrix4x4 & mat)=0;
	/**
	 * Sets the modelview matrix.
	 */
	virtual void	SetViewMatrix(const MT_Matrix4x4 & mat,
						const MT_Vector3& campos,
						const MT_Point3 &camLoc,
						const MT_Quaternion &camOrientQuat)=0;
	/**
	 */
	virtual const	MT_Point3& GetCameraPosition()=0;
	/**
	 */
	virtual void	LoadViewMatrix()=0;
	/**
	 */
	virtual void	SetFog(float start,
						   float dist,
						   float r,
						   float g,
						   float b)=0;
	
	virtual void	SetFogColor(float r,
								float g,
								float b)=0;

	virtual void	SetFogStart(float start)=0;
	/**
	 */
	virtual void	SetFogEnd(float end)=0;
	/**
	 */
	virtual void	DisplayFog()=0;
	/**
	 */
	virtual void	DisableFog()=0;

	virtual void	SetBackColor(float red,
								 float green,
								 float blue,
								 float alpha)=0;
	
	/**
	 * @param drawingmode = KX_BOUNDINGBOX, KX_WIREFRAME, KX_SOLID, KX_SHADED or KX_TEXTURED.
	 */
	virtual void	SetDrawingMode(int drawingmode)=0;
	/**
	 * @return the current drawing mode: KX_BOUNDINGBOX, KX_WIREFRAME, KX_SOLID, KX_SHADED or KX_TEXTURED.
	 */
	virtual int	GetDrawingMode()=0;
	/**
	 */
	virtual void	EnableTextures(bool enable)=0;
	/**
	 * Sets face culling
	 */	
	virtual void	SetCullFace(bool enable)=0;
	/**
	 * Sets wireframe mode.
	 */
	virtual void    SetLines(bool enable)=0;
	/**
	 */
	virtual double	GetTime()=0;
	/**
	 * Generates a projection matrix from the specified frustum.
	 * @param left the left clipping plane
	 * @param right the right clipping plane
	 * @param bottom the bottom clipping plane
	 * @param top the top clipping plane
	 * @param frustnear the near clipping plane
	 * @param frustfar the far clipping plane
	 * @return a 4x4 matrix representing the projection transform.
	 */
	virtual MT_Matrix4x4 GetFrustumMatrix(
		float left,
		float right,
		float bottom,
		float top,
		float frustnear,
		float frustfar,
		bool perspective = true
	)=0;
	/**
	 * Sets the specular colour component of the lighting equation.
	 */
	virtual void	SetSpecularity(float specX,
								   float specY,
								   float specZ,
								   float specval)=0;
	
	/**
	 * Sets the specular exponent component of the lighting equation.
	 */
	virtual void	SetShinyness(float shiny)=0;
	/**
	 * Sets the diffuse colour component of the lighting equation.
	 */
	virtual void	SetDiffuse(float difX,
							   float difY,
							   float difZ,
							   float diffuse)=0;
	
};

#endif //__RAS_IRASTERIZER

