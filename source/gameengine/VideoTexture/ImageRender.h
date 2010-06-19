/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

#if !defined IMAGERENDER_H
#define IMAGERENDER_H


#include "Common.h"

#include <KX_Scene.h>
#include <KX_Camera.h>
#include <DNA_screen_types.h>
#include <RAS_ICanvas.h>
#include <RAS_IRasterizer.h>
#include <RAS_IRenderTools.h>

#include "ImageViewport.h"


/// class for render 3d scene
class ImageRender : public ImageViewport
{
public:
	/// constructor
	ImageRender (KX_Scene * scene, KX_Camera * camera);
	ImageRender (KX_Scene * scene, KX_GameObject * observer, KX_GameObject * mirror, RAS_IPolyMaterial * mat);

	/// destructor
	virtual ~ImageRender (void);

	/// get background color
    int getBackground (int idx) { return (idx < 0 || idx > 3) ? 0 : int(m_background[idx]*255.f); }
	/// set background color
	void setBackground (int red, int green, int blue, int alpha);

	/// clipping distance
	float getClip (void) { return m_clip; }
	/// set whole buffer use
    void setClip (float clip) { m_clip = clip; }

protected:
    /// true if ready to render
    bool m_render;
	/// rendered scene
	KX_Scene * m_scene;
	/// camera for render
	KX_Camera * m_camera;
    /// do we own the camera?
    bool m_owncamera;
    /// for mirror operation
    KX_GameObject * m_observer;
    KX_GameObject * m_mirror;
	float m_clip;						// clipping distance
    float m_mirrorHalfWidth;            // mirror width in mirror space
    float m_mirrorHalfHeight;           // mirror height in mirror space
    MT_Point3 m_mirrorPos;              // mirror center position in local space
    MT_Vector3 m_mirrorZ;               // mirror Z axis in local space
    MT_Vector3 m_mirrorY;               // mirror Y axis in local space
    MT_Vector3 m_mirrorX;               // mirror X axis in local space
    /// canvas
    RAS_ICanvas* m_canvas;
    /// rasterizer
    RAS_IRasterizer* m_rasterizer;
    /// render tools
    RAS_IRenderTools* m_rendertools;
    /// engine
    KX_KetsjiEngine* m_engine;

	/// background colour
	float  m_background[4];


	/// render 3d scene to image
	virtual void calcImage (unsigned int texId, double ts);

	void Render();
	void SetupRenderFrame(KX_Scene *scene, KX_Camera* cam);
	void RenderFrame(KX_Scene* scene, KX_Camera* cam);
	void SetBackGround(KX_WorldInfo* wi);
	void SetWorldSettings(KX_WorldInfo* wi);
};


#endif

