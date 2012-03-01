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

/** \file GPC_Canvas.h
 *  \ingroup player
 */

#ifndef __GPC_CANVAS_H__
#define __GPC_CANVAS_H__

#include "RAS_ICanvas.h"
#include "RAS_Rect.h"

#ifdef WIN32
	#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
	#include <windows.h>
#endif // WIN32

#include "GL/glew.h"

#include <map>


class GPC_Canvas : public RAS_ICanvas
{
public:
	/**
	 * Used to position banners in the canvas.
	 */
	typedef enum {
		alignTopLeft,
		alignBottomRight
	} TBannerAlignment;

	typedef int TBannerId;

protected:
	/** 
	 * Used to store info for banners drawn on top of the canvas.
	 */
	typedef struct {
		/** Where the banner will be displayed. */
		TBannerAlignment alignment;
		/** Banner display enabled. */
		bool enabled;
		/** Banner display width. */
		unsigned int displayWidth;
		/** Banner display height. */
		unsigned int displayHeight;
		/** Banner image width. */
		unsigned int imageWidth;
		/** Banner image height. */
		unsigned int imageHeight;
		/** Banner image data. */
		unsigned char* imageData;
		/** Banner OpenGL texture name. */
		unsigned int textureName;
	} TBannerData;
	typedef std::map<TBannerId, TBannerData> TBannerMap;

	/** Width of the context. */
	int m_width;
	/** Height of the context. */
	int m_height;
	/** Rect that defines the area used for rendering,
	    relative to the context */
	RAS_Rect m_displayarea;

	/** Storage for the banners to display. */
	TBannerMap m_banners;
	/** State of banner display. */
	bool m_bannersEnabled;

public:

	GPC_Canvas(int width, int height);

	virtual ~GPC_Canvas();

	void Resize(int width, int height);

	virtual void ResizeWindow(int width, int height){};

	/**
	 * @section Methods inherited from abstract base class RAS_ICanvas.
	 */
	
		int 
	GetWidth(
	) const {
		return m_width;
	}
	
		int 
	GetHeight(
	) const {
		return m_height;
	}

	const 
		RAS_Rect &
	GetDisplayArea(
	) const {
		return m_displayarea;
	};

		void
	SetDisplayArea(
		RAS_Rect *rect
	) {
		m_displayarea= *rect;
	};
	
		RAS_Rect &
	GetWindowArea(
	) {
		return m_displayarea;
	}

		void 
	BeginFrame(
	) {};

	/**
	 * Draws overlay banners and progress bars.
	 */
		void 
	EndFrame(
	);
	
	void SetViewPort(int x1, int y1, int x2, int y2);

	void ClearColor(float r, float g, float b, float a);

	/**
	 * @section Methods inherited from abstract base class RAS_ICanvas.
	 * Semantics are not yet honored.
	 */
	
	void SetMouseState(RAS_MouseState mousestate)
	{
		// not yet		
	}

	void SetMousePosition(int x, int y)
	{
		// not yet
	}

	virtual void MakeScreenShot(const char* filename);

	void ClearBuffer(int type);

	/**
	 * @section Services provided by this class.
	 */

	/**
	 * Enables display of a banner.
	 * The image data is copied inside.
	 * @param bannerWidth		Display width of the banner.
	 * @param bannerHeight		Display height of the banner.
	 * @param imageWidth		Width of the banner image in pixels.
	 * @param imageHeight		Height of the banner image in pixels.
	 * @param imageData			Pointer to the pixels of the image to display.
	 * @param alignment		Where the banner will be positioned on the canvas.
	 * @param enabled			Whether the banner will be displayed initially.
	 * @return A banner id.
	 */
	TBannerId AddBanner(
		unsigned int bannerWidth, unsigned int bannerHeight,
		unsigned int imageWidth, unsigned int imageHeight,
		unsigned char* imageData, TBannerAlignment alignment = alignTopLeft, 
		bool enabled = true);

	/**
	 * Disposes a banner.
	 * @param id Banner to be disposed.
	 */
	void DisposeBanner(TBannerId id);

	/**
	 * Disposes all the banners.
	 */
	void DisposeAllBanners();

	/**
	 * Enables or disables display of a banner.
	 * @param id		Banner id of the banner to be enabled/disabled.
	 * @param enabled	New state of the banner.
	 */
	void SetBannerEnabled(TBannerId id, bool enabled = true);

	/**
	 * Enables or disables display of all banners.
	 * @param enabled	New state of the banners.
	 */
	void SetBannerDisplayEnabled(bool enabled = true);

protected:
	/**
	 * Disposes a banner.
	 * @param it Banner to be disposed.
	 */
	void DisposeBanner(TBannerData& banner);

	/**
	 * Draws all the banners enabled.
	 */
	void DrawAllBanners(void);

	/**
	 * Draws a banner.
	 */
	void DrawBanner(TBannerData& banner);

	struct CanvasRenderState {
		int oldLighting;
		int oldDepthTest;
		int oldFog;
		int oldTexture2D;
		int oldBlend;
		int oldBlendSrc;
		int oldBlendDst;
		float oldColor[4];
		int oldWriteMask;
	};

		void			
	PushRenderState(
		CanvasRenderState & render_state
	);
		void
	PopRenderState(
		const CanvasRenderState & render_state
	);

	/** 
	 * Set up an orthogonal viewing,model and texture matrix
	 * for banners and progress bars.
	 */
		void
	SetOrthoProjection(
	);
	
	static TBannerId s_bannerId;
};

#endif // __GPC_CANVAS_H__

