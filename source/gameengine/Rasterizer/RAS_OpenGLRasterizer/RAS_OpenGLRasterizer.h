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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RAS_OpenGLRasterizer.h
 *  \ingroup bgerastogl
 */

#ifndef __RAS_OPENGLRASTERIZER_H__
#define __RAS_OPENGLRASTERIZER_H__

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include "MT_CmMatrix4x4.h"
#include <vector>
using namespace std;

#include "RAS_IRasterizer.h"
#include "RAS_MaterialBucket.h"
#include "RAS_IPolygonMaterial.h"

class RAS_IStorage;
class RAS_ICanvas;
class RAS_OpenGLLight;

#define RAS_MAX_TEXCO  8     /* match in BL_Material */
#define RAS_MAX_ATTRIB 16    /* match in BL_BlenderShader */

enum RAS_STORAGE_TYPE	{
	RAS_AUTO_STORAGE,
	RAS_IMMEDIATE,
	RAS_VA,
	RAS_VBO,
};

struct OglDebugShape
{
	enum SHAPE_TYPE{
		LINE,
		CIRCLE,
	};
	SHAPE_TYPE m_type;
	MT_Vector3 m_pos;
	MT_Vector3 m_param;
	MT_Vector3 m_param2;
	MT_Vector3 m_color;
};

/**
 * 3D rendering device context.
 */
class RAS_OpenGLRasterizer : public RAS_IRasterizer
{
	RAS_ICanvas *m_2DCanvas;
	
	/* fogging vars */
	bool m_fogenabled;
	float m_fogstart;
	float m_fogdist;
	float m_fogr;
	float m_fogg;
	float m_fogb;
	
	float m_redback;
	float m_greenback;
	float m_blueback;
	float m_alphaback;
	
	float m_ambr;
	float m_ambg;
	float m_ambb;
	double m_time;
	MT_Matrix4x4 m_viewmatrix;
	MT_Matrix4x4 m_viewinvmatrix;
	MT_Point3 m_campos;
	bool m_camortho;

	StereoMode m_stereomode;
	StereoEye m_curreye;
	float m_eyeseparation;
	float m_focallength;
	bool m_setfocallength;
	int m_noOfScanlines;

	short m_prevafvalue;

	/* motion blur */
	int m_motionblur;
	float m_motionblurvalue;

	bool m_usingoverrideshader;

	/* Render tools */
	void *m_clientobject;
	void *m_auxilaryClientInfo;
	std::vector<RAS_OpenGLLight *> m_lights;
	int m_lastlightlayer;
	bool m_lastlighting;
	void *m_lastauxinfo;
	unsigned int m_numgllights;

protected:
	int m_drawingmode;
	TexCoGen m_texco[RAS_MAX_TEXCO];
	TexCoGen m_attrib[RAS_MAX_ATTRIB];
	int m_attrib_layer[RAS_MAX_ATTRIB];
	int m_texco_num;
	int m_attrib_num;
	/* int m_last_alphablend; */
	bool m_last_frontface;

	/* Stores the caching information for the last material activated. */
	RAS_IPolyMaterial::TCachingInfo m_materialCachingInfo;

	/* Making use of a Strategy design pattern for storage behavior.
	 * Examples of concrete strategies: Vertex Arrays, VBOs, Immediate Mode*/
	int m_storage_type;
	RAS_IStorage *m_storage;
	RAS_IStorage *m_failsafe_storage; /* So derived mesh can use immediate mode */

public:
	double GetTime();
	RAS_OpenGLRasterizer(RAS_ICanvas *canv, int storage=RAS_AUTO_STORAGE);
	virtual ~RAS_OpenGLRasterizer();

	/*enum DrawType
	{
			KX_BOUNDINGBOX = 1,
			KX_WIREFRAME,
			KX_SOLID,
			KX_SHADED,
			KX_TEXTURED
	};

	enum DepthMask
	{
			KX_DEPTHMASK_ENABLED =1,
			KX_DEPTHMASK_DISABLED,
	};*/
	virtual void SetDepthMask(DepthMask depthmask);

	virtual bool SetMaterial(const RAS_IPolyMaterial &mat);
	virtual bool Init();
	virtual void Exit();
	virtual bool BeginFrame(double time);
	virtual void ClearColorBuffer();
	virtual void ClearDepthBuffer();
	virtual void ClearCachingInfo(void);
	virtual void EndFrame();
	virtual void SetRenderArea();

	virtual void SetStereoMode(const StereoMode stereomode);
	virtual RAS_IRasterizer::StereoMode GetStereoMode();
	virtual bool Stereo();
	virtual bool InterlacedStereo();
	virtual void SetEye(const StereoEye eye);
	virtual StereoEye GetEye();
	virtual void SetEyeSeparation(const float eyeseparation);
	virtual float GetEyeSeparation();
	virtual void SetFocalLength(const float focallength);
	virtual float GetFocalLength();

	virtual void SwapBuffers();

	virtual void IndexPrimitives(class RAS_MeshSlot &ms);
	virtual void IndexPrimitivesMulti(class RAS_MeshSlot &ms);
	virtual void IndexPrimitives_3DText(class RAS_MeshSlot &ms, class RAS_IPolyMaterial *polymat);

	virtual void SetProjectionMatrix(MT_CmMatrix4x4 &mat);
	virtual void SetProjectionMatrix(const MT_Matrix4x4 &mat);
	virtual void SetViewMatrix(const MT_Matrix4x4 &mat, const MT_Matrix3x3 &ori, const MT_Point3 &pos, bool perspective);

	virtual const MT_Point3& GetCameraPosition();
	virtual bool GetCameraOrtho();
	
	virtual void SetFog(float start, float dist, float r, float g, float b);
	virtual void SetFogColor(float r, float g, float b);
	virtual void SetFogStart(float fogstart);
	virtual void SetFogEnd(float fogend);
	void DisableFog();
	virtual void DisplayFog();
	virtual bool IsFogEnabled();

	virtual void SetBackColor(float red, float green, float blue, float alpha);
	
	virtual void SetDrawingMode(int drawingmode);
	virtual int GetDrawingMode();

	virtual void SetCullFace(bool enable);
	virtual void SetLines(bool enable);

	virtual MT_Matrix4x4 GetFrustumMatrix(
	        float left, float right, float bottom, float top,
	        float frustnear, float frustfar, 
	        float focallength, bool perspective);
	virtual MT_Matrix4x4 GetOrthoMatrix(
	        float left, float right, float bottom, float top,
	        float frustnear, float frustfar);

	virtual void SetSpecularity(float specX, float specY, float specZ, float specval);
	virtual void SetShinyness(float shiny);
	virtual void SetDiffuse(float difX, float difY, float difZ, float diffuse);
	virtual void SetEmissive(float eX, float eY, float eZ, float e);

	virtual void SetAmbientColor(float red, float green, float blue);
	virtual void SetAmbient(float factor);

	virtual void SetPolygonOffset(float mult, float add);

	virtual void FlushDebugShapes();

	virtual void DrawDebugLine(const MT_Vector3 &from,const MT_Vector3 &to, const MT_Vector3 &color)
	{
		OglDebugShape line;
		line.m_type = OglDebugShape::LINE;
		line.m_pos= from;
		line.m_param = to;
		line.m_color = color;
		m_debugShapes.push_back(line);
	}

	virtual void DrawDebugCircle(const MT_Vector3 &center, const MT_Scalar radius, const MT_Vector3 &color,
	                             const MT_Vector3 &normal, int nsector)
	{
		OglDebugShape line;
		line.m_type = OglDebugShape::CIRCLE;
		line.m_pos= center;
		line.m_param = normal;
		line.m_color = color;
		line.m_param2.x() = radius;
		line.m_param2.y() = (float) nsector;
		m_debugShapes.push_back(line);
	}

	std::vector <OglDebugShape>	m_debugShapes;

	virtual void SetTexCoordNum(int num);
	virtual void SetAttribNum(int num);
	virtual void SetTexCoord(TexCoGen coords, int unit);
	virtual void SetAttrib(TexCoGen coords, int unit, int layer = 0);

	void TexCoord(const RAS_TexVert &tv);

	const MT_Matrix4x4 &GetViewMatrix() const;
	const MT_Matrix4x4 &GetViewInvMatrix() const;
	
	virtual void EnableMotionBlur(float motionblurvalue);
	virtual void DisableMotionBlur();
	virtual float GetMotionBlurValue() { return m_motionblurvalue; }
	virtual int GetMotionBlurState() { return m_motionblur; }
	virtual void SetMotionBlurState(int newstate)
	{
		if (newstate < 0)
			m_motionblur = 0;
		else if (newstate > 2)
			m_motionblur = 2;
		else 
			m_motionblur = newstate;
	}

	virtual void SetAlphaBlend(int alphablend);
	virtual void SetFrontFace(bool ccw);
	
	virtual void SetAnisotropicFiltering(short level);
	virtual short GetAnisotropicFiltering();

	virtual void SetMipmapping(MipmapOption val);
	virtual MipmapOption GetMipmapping();

	virtual void SetUsingOverrideShader(bool val);
	virtual bool GetUsingOverrideShader();

	/**
	 * Render Tools
	 */
	void EnableOpenGLLights();
	void DisableOpenGLLights();
	void ProcessLighting(bool uselights, const MT_Transform &viewmat);

	void RenderBox2D(int xco, int yco, int width, int height, float percentage);
	void RenderText3D(int fontid, const char *text, int size, int dpi,
	                  const float color[4], const double mat[16], float aspect);
	void RenderText2D(RAS_TEXT_RENDER_MODE mode, const char *text,
	                  int xco, int yco, int width, int height);

	void applyTransform(double *oglmatrix, int objectdrawmode);

	void PushMatrix();
	void PopMatrix();

	bool RayHit(struct KX_ClientObjectInfo *client, class KX_RayCast *result, void * const data);
	bool NeedRayCast(struct KX_ClientObjectInfo *) { return true; }

	RAS_ILightObject* CreateLight();
	void AddLight(RAS_ILightObject* lightobject);

	void RemoveLight(RAS_ILightObject* lightobject);
	int ApplyLights(int objectlayer, const MT_Transform& viewmat);

	void MotionBlur();

	void SetClientObject(void *obj);

	void SetAuxilaryClientInfo(void *inf);


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_OpenGLRasterizer")
#endif
};

#endif  /* __RAS_OPENGLRASTERIZER_H__ */
