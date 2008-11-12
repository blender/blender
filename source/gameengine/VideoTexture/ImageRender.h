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

	/// destructor
	virtual ~ImageRender (void);

	/// get background color
	unsigned char * getBackground (void) { return m_background; }
	/// set background color
	void setBackground (unsigned char red, unsigned char green, unsigned char blue);

protected:
	/// rendered scene
	KX_Scene * m_scene;
	/// camera for render
	KX_Camera * m_camera;

	/// screen area for rendering
	ScrArea m_area;
	/// rendering device
	RAS_ICanvas * m_canvas;
	/// rasterizer
	RAS_IRasterizer * m_rasterizer;
	/// render tools
	RAS_IRenderTools * m_rendertools;

	/// background colour
	unsigned char m_background[3];


	/// render 3d scene to image
	virtual void calcImage (unsigned int texId);

	/// refresh lights
	void refreshLights (void);
	/// methods from KX_KetsjiEngine
	bool BeginFrame();
	void EndFrame();
	void Render();
	void SetupRenderFrame(KX_Scene *scene, KX_Camera* cam);
	void RenderFrame(KX_Scene* scene, KX_Camera* cam);
	void SetBackGround(KX_WorldInfo* wi);
	void SetWorldSettings(KX_WorldInfo* wi);
};


#endif

