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
#ifndef __RAS_OPENGLRASTERIZER
#define __RAS_OPENGLRASTERIZER

#ifdef WIN32
#pragma warning (disable:4786)
#endif

#include "MT_CmMatrix4x4.h"
#include <vector>
using namespace std;

#include "RAS_IRasterizer.h"
#include "RAS_MaterialBucket.h"
#include "RAS_ICanvas.h"

/**
 * 3D rendering device context.
 */
class RAS_OpenGLRasterizer : public RAS_IRasterizer
{

	RAS_ICanvas*	m_2DCanvas;
	
	// fogging vars
	bool			m_fogenabled;
	float			m_fogstart;
	float			m_fogdist;
	float			m_fogr;
	float			m_fogg;
	float			m_fogb;
	
	float			m_redback;
	float			m_greenback;
	float			m_blueback;
	float			m_alphaback;

	bool			m_bEXT_compiled_vertex_array;

	double			m_time;
	MT_CmMatrix4x4	m_viewmatrix;
	MT_Point3		m_campos;

	int				m_stereomode;
	int				m_curreye;
	float			m_eyeseparation;
	float			m_focallength;
	int				m_noOfScanlines;

protected:
	int				m_drawingmode;
	/** Stores the caching information for the last material activated. */
	RAS_IPolyMaterial::TCachingInfo m_materialCachingInfo;

public:
	double GetTime();
	RAS_OpenGLRasterizer(RAS_ICanvas* canv);
	virtual ~RAS_OpenGLRasterizer();



	enum
	{
			KX_BOUNDINGBOX = 1,
			KX_WIREFRAME,
			KX_SOLID,
			KX_SHADED,
			KX_TEXTURED 
	};

	enum
	{
			KX_DEPTHMASK_ENABLED =1,
			KX_DEPTHMASK_DISABLED,
	};
	virtual void	SetDepthMask(int depthmask);

	virtual void	SetMaterial(const RAS_IPolyMaterial& mat);
	virtual bool	Init();
	virtual void	Exit();
	virtual bool	BeginFrame(int drawingmode, double time);
	virtual void	ClearDepthBuffer();
	virtual void	ClearCachingInfo(void);
	virtual void	EndFrame();
	virtual void	SetRenderArea();

	virtual void	SetStereoMode(const int stereomode);
	virtual bool	Stereo();
	virtual void	SetEye(const int eye);
	virtual void	SetEyeSeparation(const float eyeseparation);
	virtual void	SetFocalLength(const float focallength);

	virtual void	SwapBuffers();
	virtual void	IndexPrimitives(
						const vecVertexArray& vertexarrays,
						const vecIndexArrays & indexarrays,
						int mode,
						class RAS_IPolyMaterial* polymat,
						class RAS_IRenderTools* rendertools,
						bool useObjectColor,
						const MT_Vector4& rgbacolor
					);

	virtual void	IndexPrimitives_Ex(
						const vecVertexArray& vertexarrays,
						const vecIndexArrays & indexarrays,
						int mode,
						class RAS_IPolyMaterial* polymat,
						class RAS_IRenderTools* rendertools,
						bool useObjectColor,
						const MT_Vector4& rgbacolor
					);

	virtual void	IndexPrimitives_3DText(
						const vecVertexArray& vertexarrays,
						const vecIndexArrays & indexarrays,
						int mode,
						class RAS_IPolyMaterial* polymat,
						class RAS_IRenderTools* rendertools,
						bool useObjectColor,
						const MT_Vector4& rgbacolor
					);

	virtual void	SetProjectionMatrix(MT_CmMatrix4x4 & mat);
	virtual void	SetProjectionMatrix(MT_Matrix4x4 & mat);
	virtual void	SetViewMatrix(
						const MT_Matrix4x4 & mat,
						const MT_Vector3& campos,
						const MT_Point3 &camLoc,
						const MT_Quaternion &camOrientQuat
					);

	virtual const	MT_Point3& GetCameraPosition();
	virtual void	LoadViewMatrix();
	
	virtual void	SetFog(
						float start,
						float dist,
						float r,
						float g,
						float b
					);

	virtual void	SetFogColor(
						float r,
						float g,
						float b
					);

	virtual void	SetFogStart(float fogstart);
	virtual void	SetFogEnd(float fogend);

	void			DisableFog();
	virtual void	DisplayFog();

	virtual void	SetBackColor(
						float red,
						float green,
						float blue,
						float alpha
					);
	
	virtual void	SetDrawingMode(int drawingmode);
	virtual int		GetDrawingMode();

	virtual void	EnableTextures(bool enable);
	virtual void	SetCullFace(bool enable);

	virtual MT_Matrix4x4 GetFrustumMatrix(
							float left,
							float right,
							float bottom,
							float top,
							float frustnear,
							float frustfar
						);

	virtual void	SetSpecularity(
						float specX,
						float specY,
						float specZ,
						float specval
					);

	virtual void	SetShinyness(float shiny);
	virtual void	SetDiffuse(
						float difX,
						float difY,
						float difZ,
						float diffuse
					);

};

#endif //__RAS_OPENGLRASTERIZER

