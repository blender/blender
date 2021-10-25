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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2007 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ImageRender.h
 *  \ingroup bgevideotex
 */

#ifndef __IMAGERENDER_H__
#define __IMAGERENDER_H__


#include "Common.h"

#include "KX_Scene.h"
#include "KX_Camera.h"
#include "DNA_screen_types.h"
#include "RAS_ICanvas.h"
#include "RAS_IRasterizer.h"
#include "RAS_IOffScreen.h"
#include "RAS_ISync.h"

#include "ImageViewport.h"


/// class for render 3d scene
class ImageRender : public ImageViewport
{
public:
	/// constructor
	ImageRender(KX_Scene *scene, KX_Camera *camera, PyRASOffScreen *offscreen);
	ImageRender(KX_Scene *scene, KX_GameObject *observer, KX_GameObject *mirror, RAS_IPolyMaterial * mat);

	/// destructor
	virtual ~ImageRender (void);

	/// get background color
	float getBackground (int idx);
	/// set background color
	void setBackground (float red, float green, float blue, float alpha);

	/// clipping distance
	float getClip (void) { return m_clip; }
	/// set whole buffer use
	void setClip (float clip) { m_clip = clip; }
	/// render status
	bool isDone() { return m_done; }
	/// render frame (public so that it is accessible from python)
	bool Render();
	/// in case fbo is used, method to unbind
	void Unbind();
	/// wait for render to complete
	void WaitSync();

protected:
	/// true if ready to render
	bool m_render;
	/// is render done already?
	bool m_done;
	/// rendered scene
	KX_Scene * m_scene;
	/// camera for render
	KX_Camera * m_camera;
	/// do we own the camera?
	bool m_owncamera;
	/// if offscreen render
	PyRASOffScreen *m_offscreen;
	/// object to synchronize render even if no buffer transfer
	RAS_ISync *m_sync;
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
	/// engine
	KX_KetsjiEngine* m_engine;

	/// background color
	float m_background[4];


	/// render 3d scene to image
	virtual void calcImage (unsigned int texId, double ts) { calcViewport(texId, ts, GL_RGBA); }

	/// render 3d scene to image
	virtual void calcViewport (unsigned int texId, double ts, unsigned int format);

	void setBackgroundFromScene(KX_Scene *scene);
	void SetWorldSettings(KX_WorldInfo* wi);
};


#endif

