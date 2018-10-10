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

#ifndef __APPCANVAS_H__
#define __APPCANVAS_H__

/** \file blender/freestyle/intern/application/AppCanvas.h
 *  \ingroup freestyle
 */

#include "../stroke/Canvas.h"
#include "AppView.h"

namespace Freestyle {

class AppCanvas : public Canvas
{
public:
	AppCanvas();
	AppCanvas(AppView *iViewer);
	AppCanvas(const AppCanvas& iBrother);
	virtual ~AppCanvas();

	/*! operations that need to be done before a draw */
	virtual void preDraw();

	/*! operations that need to be done after a draw */
	virtual void postDraw();

	/*! Erases the layers and clears the canvas */
	virtual void Erase();

	/* init the canvas */
	virtual void init();

	/*! Reads a pixel area from the canvas */
	virtual void readColorPixels(int x, int y, int w, int h, RGBImage& oImage) const;
	/*! Reads a depth pixel area from the canvas */
	virtual void readDepthPixels(int x, int y, int w, int h, GrayImage& oImage) const;

	virtual BBox<Vec3r> scene3DBBox() const;

	/* abstract */
	virtual void RenderStroke(Stroke *);
	virtual void update();


	/*! accessors */
	virtual int width() const;
	virtual int height() const;
	virtual BBox<Vec2i> border() const;
	virtual float thickness() const;

	AppView *_pViewer;
	inline const AppView *viewer() const {return _pViewer;}

	/*! modifiers */
	void setViewer(AppView *iViewer);

	/* soc */
	void setPassDiffuse(float *buf, int width, int height) {
		_pass_diffuse.buf = buf;
		_pass_diffuse.width = width;
		_pass_diffuse.height = height;
	}
	void setPassZ(float *buf, int width, int height) {
		_pass_z.buf = buf;
		_pass_z.width = width;
		_pass_z.height = height;
	}

private:
	struct {
		float *buf;
		int width, height;
	} _pass_diffuse, _pass_z;
};

} /* namespace Freestyle */

#endif // __APPCANVAS_H__
