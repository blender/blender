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

/** \file RAS_IRasterizer.h
 *  \ingroup bgerast
 */

#ifndef __RAS_IRASTERIZER_H__
#define __RAS_IRASTERIZER_H__

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include "STR_HashedString.h"

#include "MT_CmMatrix4x4.h"
#include "MT_Matrix4x4.h"

#include "RAS_TexVert.h"

#include <vector>
using namespace std;

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_ICanvas;
class RAS_IPolyMaterial;
class RAS_MeshSlot;
class RAS_ILightObject;
class SCA_IScene;
class RAS_IOffScreen;
class RAS_ISync;

typedef vector<unsigned short> KX_IndexArray;
typedef vector<RAS_TexVert> KX_VertexArray;
typedef vector<KX_VertexArray *> vecVertexArray;
typedef vector<KX_IndexArray *> vecIndexArrays;

/**
 * 3D rendering device context interface. 
 */
class RAS_IRasterizer
{
public:
	enum RAS_TEXT_RENDER_MODE {
		RAS_TEXT_RENDER_NODEF = 0,
		RAS_TEXT_NORMAL,
		RAS_TEXT_PADDED,
		RAS_TEXT_MAX,
	};

	RAS_IRasterizer(RAS_ICanvas* canv) {};
	virtual ~RAS_IRasterizer() {};

	/**
	 * Drawing types
	 */
	enum DrawType {
		KX_BOUNDINGBOX = 1,
		KX_WIREFRAME,
		KX_SOLID,
		KX_SHADED,
		KX_TEXTURED,
		KX_SHADOW,
	};

	/**
	 * Drawing modes
	 */

	enum DrawMode {
		KX_MODE_LINES = 1,
		KX_MODE_TRIANGLES,
		KX_MODE_QUADS,
	};

	/**
	 * Valid SetDepthMask parameters
	 */
	enum DepthMask {
		KX_DEPTHMASK_ENABLED = 1,
		KX_DEPTHMASK_DISABLED,
	};

	/**
	 */
	enum {
		RAS_RENDER_3DPOLYGON_TEXT = 64, /* GEMAT_TEXT */
		KX_BACKCULL = 16,               /* GEMAT_BACKCULL */
		KX_TEX = 4096,                  /* GEMAT_TEX */
		KX_LINES = 32768,
	};

	/**
	 * Stereo mode types
	 */
	enum StereoMode {
		RAS_STEREO_NOSTEREO = 1,
		RAS_STEREO_QUADBUFFERED,
		RAS_STEREO_ABOVEBELOW,
		RAS_STEREO_INTERLACED,
		RAS_STEREO_ANAGLYPH,
		RAS_STEREO_SIDEBYSIDE,
		RAS_STEREO_VINTERLACE,
		RAS_STEREO_DOME,
		RAS_STEREO_3DTVTOPBOTTOM,

		RAS_STEREO_MAXSTEREO
	};

	/**
	 * Texture gen modes.
	 */
	enum TexCoGen {
		RAS_TEXCO_GEN,      /* < GPU will generate texture coordinates */
		RAS_TEXCO_ORCO,     /* < Vertex coordinates (object space) */
		RAS_TEXCO_GLOB,     /* < Vertex coordinates (world space) */
		RAS_TEXCO_UV,       /* < UV coordinates */
		RAS_TEXCO_OBJECT,   /* < Use another object's position as coordinates */
		RAS_TEXCO_LAVECTOR, /* < Light vector as coordinates */
		RAS_TEXCO_VIEW,     /* < View vector as coordinates */
		RAS_TEXCO_STICKY,   /* < Sticky coordinates */
		RAS_TEXCO_WINDOW,   /* < Window coordinates */
		RAS_TEXCO_NORM,     /* < Normal coordinates */
		RAS_TEXTANGENT,     /* < */
		RAS_TEXCO_VCOL,     /* < Vertex Color */
		RAS_TEXCO_DISABLE,  /* < Disable this texture unit (cached) */
	};

	/**
	 * Render pass identifiers for stereo.
	 */
	enum StereoEye {
		RAS_STEREO_LEFTEYE = 1,
		RAS_STEREO_RIGHTEYE,
	};

	/**
	 * Mipmap options
	 */
	enum MipmapOption {
		RAS_MIPMAP_NONE,
		RAS_MIPMAP_NEAREST,
		RAS_MIPMAP_LINEAR,

		RAS_MIPMAP_MAX,  /* Should always be last */
	};

	/**
	 * SetDepthMask enables or disables writing a fragment's depth value
	 * to the Z buffer.
	 */
	virtual void SetDepthMask(DepthMask depthmask) = 0;

	/**
	 * SetMaterial sets the material settings for subsequent primitives
	 * to be rendered with.
	 * The material will be cached.
	 */
	virtual bool SetMaterial(const RAS_IPolyMaterial &mat) = 0;

	/**
	 * Init initializes the renderer.
	 */
	virtual bool Init() = 0;

	/**
	 * Exit cleans up the renderer.
	 */
	virtual void Exit() = 0;

	/**
	 * BeginFrame is called at the start of each frame.
	 */
	virtual bool BeginFrame(double time) = 0;

	/**
	 * ClearColorBuffer clears the color buffer.
	 */
	virtual void ClearColorBuffer() = 0;

	/**
	 * ClearDepthBuffer clears the depth buffer.
	 */
	virtual void ClearDepthBuffer() = 0;

	/**
	 * ClearCachingInfo clears the currently cached material.
	 */
	virtual void ClearCachingInfo(void) = 0;

	/**
	 * EndFrame is called at the end of each frame.
	 */
	virtual void EndFrame() = 0;

	/**
	 * SetRenderArea sets the render area from the 2d canvas.
	 * Returns true if only of subset of the canvas is used.
	 */
	virtual void SetRenderArea() = 0;

	// Stereo Functions
	/**
	 * SetStereoMode will set the stereo mode
	 */
	virtual void SetStereoMode(const StereoMode stereomode) = 0;

	/**
	 * Stereo can be used to query if the rasterizer is in stereo mode.
	 * \return true if stereo mode is enabled.
	 */
	virtual bool Stereo() = 0;
	virtual StereoMode GetStereoMode() = 0;
	virtual bool InterlacedStereo() = 0;

	/**
	 * Sets which eye buffer subsequent primitives will be rendered to.
	 */
	virtual void SetEye(const StereoEye eye) = 0;
	virtual StereoEye GetEye() = 0;

	/**
	 * Sets the distance between eyes for stereo mode.
	 */
	virtual void SetEyeSeparation(const float eyeseparation) = 0;
	virtual float GetEyeSeparation() = 0;

	/**
	 * Sets the focal length for stereo mode.
	 */
	virtual void SetFocalLength(const float focallength) = 0;
	virtual float GetFocalLength() = 0;

	/**
	 * Create an offscreen render buffer that can be used as target for render.
	 * For the time being, it is only used in VideoTexture for custom render.
	 */
	virtual RAS_IOffScreen *CreateOffScreen(int width, int height, int samples, int target) = 0;

	/**
	 * Create a sync object
	 * For use with offscreen render
	 */
	virtual RAS_ISync *CreateSync(int type) = 0;

	/**
	 * SwapBuffers swaps the back buffer with the front buffer.
	 */
	virtual void SwapBuffers() = 0;
	
	// Drawing Functions
	/**
	 * IndexPrimitives: Renders primitives from mesh slot.
	 */
	virtual void IndexPrimitives(class RAS_MeshSlot &ms) = 0;

	/**
	 * IndexPrimitives_3DText will render text into the polygons.
	 */
	virtual void IndexPrimitives_3DText(class RAS_MeshSlot &ms, class RAS_IPolyMaterial *polymat) = 0;
 
	virtual void SetProjectionMatrix(MT_CmMatrix4x4 &mat) = 0;

	/* This one should become our final version, methinks. */
	/**
	 * Set the projection matrix for the rasterizer. This projects
	 * from camera coordinates to window coordinates.
	 * \param mat The projection matrix.
	 */
	virtual void SetProjectionMatrix(const MT_Matrix4x4 &mat) = 0;

	/**
	 * Sets the modelview matrix.
	 */
	virtual void SetViewMatrix(const MT_Matrix4x4 &mat, const MT_Matrix3x3 &ori,
	                           const MT_Point3 &pos, const MT_Vector3 &scale, bool perspective) = 0;

	/**
	 */
	virtual const MT_Point3& GetCameraPosition() = 0;
	virtual bool GetCameraOrtho() = 0;

	/**
	 * Fog
	 */
	virtual void SetFog(short type, float start, float dist, float intensity, float color[3]) = 0;
	virtual void DisplayFog() = 0;
	virtual void EnableFog(bool enable) = 0;

	virtual void SetBackColor(float color[3]) = 0;
	
	/**
	 * \param drawingmode = KX_BOUNDINGBOX, KX_WIREFRAME, KX_SOLID, KX_SHADED or KX_TEXTURED.
	 */
	virtual void SetDrawingMode(int drawingmode) = 0;

	/**
	 * \return the current drawing mode: KX_BOUNDINGBOX, KX_WIREFRAME, KX_SOLID, KX_SHADED or KX_TEXTURED.
	 */
	virtual int GetDrawingMode() = 0;

	/**
	 * Sets face culling
	 */
	virtual void SetCullFace(bool enable) = 0;

	/**
	 * Sets wireframe mode.
	 */
	virtual void SetLines(bool enable) = 0;

	/**
	 */
	virtual double GetTime() = 0;

	/**
	 * Generates a projection matrix from the specified frustum.
	 * \param left the left clipping plane
	 * \param right the right clipping plane
	 * \param bottom the bottom clipping plane
	 * \param top the top clipping plane
	 * \param frustnear the near clipping plane
	 * \param frustfar the far clipping plane
	 * \return a 4x4 matrix representing the projection transform.
	 */
	virtual MT_Matrix4x4 GetFrustumMatrix(
	        float left, float right, float bottom, float top,
	        float frustnear, float frustfar,
	        float focallength = 0.0f, bool perspective = true) = 0;

	/**
	 * Generates a orthographic projection matrix from the specified frustum.
	 * \param left the left clipping plane
	 * \param right the right clipping plane
	 * \param bottom the bottom clipping plane
	 * \param top the top clipping plane
	 * \param frustnear the near clipping plane
	 * \param frustfar the far clipping plane
	 * \return a 4x4 matrix representing the projection transform.
	 */
	virtual MT_Matrix4x4 GetOrthoMatrix(
	        float left, float right, float bottom, float top,
	        float frustnear, float frustfar) = 0;

	/**
	 * Sets the specular color component of the lighting equation.
	 */
	virtual void SetSpecularity(float specX, float specY, float specZ, float specval) = 0;
	
	/**
	 * Sets the specular exponent component of the lighting equation.
	 */
	virtual void SetShinyness(float shiny) = 0;

	/**
	 * Sets the diffuse color component of the lighting equation.
	 */
	virtual void SetDiffuse(float difX,float difY, float difZ, float diffuse) = 0;

	/**
	 * Sets the emissive color component of the lighting equation.
	 */ 
	virtual void SetEmissive(float eX, float eY, float eZ, float e) = 0;
	
	virtual void SetAmbientColor(float color[3]) = 0;
	virtual void SetAmbient(float factor) = 0;

	/**
	 * Sets a polygon offset.  z depth will be: z1 = mult*z0 + add
	 */
	virtual void	SetPolygonOffset(float mult, float add) = 0;
	
	virtual void DrawDebugLine(SCA_IScene *scene, const MT_Vector3 &from, const MT_Vector3 &to, const MT_Vector3& color) = 0;
	virtual void DrawDebugCircle(SCA_IScene *scene, const MT_Vector3 &center, const MT_Scalar radius,
								 const MT_Vector3 &color, const MT_Vector3 &normal, int nsector) = 0;
	virtual void FlushDebugShapes(SCA_IScene *scene) = 0;
	
	virtual void SetTexCoordNum(int num) = 0;
	virtual void SetAttribNum(int num) = 0;
	virtual void SetTexCoord(TexCoGen coords, int unit) = 0;
	virtual void SetAttrib(TexCoGen coords, int unit, int layer = 0) = 0;

	virtual const MT_Matrix4x4 &GetViewMatrix() const = 0;
	virtual const MT_Matrix4x4 &GetViewInvMatrix() const = 0;

	virtual bool QueryLists() { return false; }
	virtual bool QueryArrays() { return false; }
	
	virtual void EnableMotionBlur(float motionblurvalue) = 0;
	virtual void DisableMotionBlur() = 0;
	
	virtual float GetMotionBlurValue() = 0;
	virtual int GetMotionBlurState() = 0;
	virtual void SetMotionBlurState(int newstate) = 0;

	virtual void SetAlphaBlend(int alphablend) = 0;
	virtual void SetFrontFace(bool ccw) = 0;

	virtual void SetAnisotropicFiltering(short level) = 0;
	virtual short GetAnisotropicFiltering() = 0;

	virtual void SetMipmapping(MipmapOption val) = 0;
	virtual MipmapOption GetMipmapping() = 0;

	virtual void SetUsingOverrideShader(bool val) = 0;
	virtual bool GetUsingOverrideShader() = 0;

	/**
	 * Render Tools
	 */
	virtual void applyTransform(float *oglmatrix, int drawingmode) = 0;

	/**
	 * Renders 2D boxes.
	 * \param xco			Position on the screen (origin in lower left corner).
	 * \param yco			Position on the screen (origin in lower left corner).
	 * \param width			Width of the canvas to draw to.
	 * \param height		Height of the canvas to draw to.
	 * \param percentage	Percentage of bar.
	 */
	virtual void RenderBox2D(int xco, int yco, int width, int height, float percentage) = 0;

	/**
	 * Renders 3D text string using BFL.
	 * \param fontid	The id of the font.
	 * \param text		The string to render.
	 * \param size		The size of the text.
	 * \param dpi		The resolution of the text.
	 * \param color		The color of the object.
	 * \param mat		The Matrix of the text object.
	 * \param aspect	A scaling factor to compensate for the size.
	 */
	virtual void RenderText3D(
	        int fontid, const char *text, int size, int dpi,
	        const float color[4], const float mat[16], float aspect) = 0;

	/**
	 * Renders 2D text string.
	 * \param mode      The type of text
	 * \param text		The string to render.
	 * \param xco		Position on the screen (origin in lower left corner).
	 * \param yco		Position on the screen (origin in lower left corner).
	 * \param width		Width of the canvas to draw to.
	 * \param height	Height of the canvas to draw to.
	 */
	virtual void RenderText2D(
	        RAS_TEXT_RENDER_MODE mode, const char *text,
	        int xco, int yco, int width, int height) = 0;

	virtual void ProcessLighting(bool uselights, const MT_Transform &trans) = 0;

	virtual void PushMatrix() = 0;

	virtual void PopMatrix() = 0;

	virtual RAS_ILightObject *CreateLight() = 0;

	virtual void AddLight(RAS_ILightObject *lightobject) = 0;

	virtual void RemoveLight(RAS_ILightObject *lightobject) = 0;

	virtual void MotionBlur() = 0;

	virtual void SetClientObject(void *obj) = 0;

	virtual void SetAuxilaryClientInfo(void *inf) = 0;

	/**
	 * Prints information about what the hardware supports.
	 */
	virtual void PrintHardwareInfo() = 0;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_IRasterizer")
#endif
};

#endif  /* __RAS_IRASTERIZER_H__ */
