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

/** \file gameengine/Rasterizer/RAS_FramingManager.cpp
 *  \ingroup bgerast
 */


#include "RAS_FramingManager.h"
#include "RAS_Rect.h"

	void
RAS_FramingManager::
ComputeDefaultFrustum(
	const float camnear,
	const float camfar,
	const float lens,
	const float sensor_x, const float sensor_y,
	const short sensor_fit,
	const float shift_x,
	const float shift_y,
	const float design_aspect_ratio,
	RAS_FrameFrustum & frustum
) {
	float size;
	float halfSize;
	float sizeX;
	float sizeY;
	float offsetX;
	float offsetY;

	if (sensor_fit==RAS_SENSORFIT_AUTO) {
		size = sensor_x * camnear / lens;
		halfSize = size * 0.5f;

		if (design_aspect_ratio > 1.f) {
			// halfsize defines the width
			sizeX = halfSize;
			sizeY = halfSize/design_aspect_ratio;
		} else {
			// halfsize defines the height
			sizeX = halfSize * design_aspect_ratio;
			sizeY = halfSize;
		}
	}
	else if (sensor_fit==RAS_SENSORFIT_HOR) {
		size = sensor_x * camnear / lens;
		halfSize = size * 0.5f;
		sizeX = halfSize;
		sizeY = halfSize/design_aspect_ratio;
	}
	else {
		size = sensor_y * camnear / lens;
		halfSize = size * 0.5f;
		sizeX = halfSize * design_aspect_ratio;
		sizeY = halfSize;
	}

	offsetX = size * shift_x;
	offsetY = size * shift_y;

	frustum.x2 = sizeX + offsetX;
	frustum.x1 = -sizeX + offsetX;
	frustum.y2 = sizeY + offsetY;
	frustum.y1 = -sizeY + offsetY;
	frustum.camnear = camnear;
	frustum.camfar = camfar;
}

	void
RAS_FramingManager::
ComputeDefaultOrtho(
	const float camnear,
	const float camfar,
	const float scale,
	const float design_aspect_ratio,
	const short sensor_fit,
	const float shift_x,
	const float shift_y,
	RAS_FrameFrustum & frustum
)
{
	float halfSize = scale*0.5f;
	float sizeX;
	float sizeY;
	float offsetX;
	float offsetY;

	if (sensor_fit==RAS_SENSORFIT_AUTO) {
		if (design_aspect_ratio > 1.f) {
			// halfsize defines the width
			sizeX = halfSize;
			sizeY = halfSize/design_aspect_ratio;
		} else {
			// halfsize defines the height
			sizeX = halfSize * design_aspect_ratio;
			sizeY = halfSize;
		}
	}
	else if (sensor_fit==RAS_SENSORFIT_HOR) {
		sizeX = halfSize;
		sizeY = halfSize/design_aspect_ratio;
	}
	else {
		sizeX = halfSize * design_aspect_ratio;
		sizeY = halfSize;
	}

	offsetX = scale * shift_x;
	offsetY = scale * shift_y;

	frustum.x2 = sizeX + offsetX;
	frustum.x1 = -sizeX + offsetX;
	frustum.y2 = sizeY + offsetY;
	frustum.y1 = -sizeY + offsetY;
	frustum.camnear = camnear;
	frustum.camfar = camfar;
}


	void
RAS_FramingManager::
ComputeBestFitViewRect(
	const RAS_Rect &availableViewport,
	const float design_aspect_ratio,
	RAS_Rect &viewport
) {
	// try and honour the aspect ratio when setting the 
	// drawable area. If we don't do this we are liable
	// to get a lot of distortion in the rendered image.
	
	int width = availableViewport.GetWidth();
	int height = availableViewport.GetHeight();
	float window_aspect = float(width)/float(height);

	if (window_aspect < design_aspect_ratio) {
		int v_height = (int)(width / design_aspect_ratio); 
		int left_over = (height - v_height) / 2; 
			
		viewport.SetLeft(availableViewport.GetLeft());
		viewport.SetBottom(availableViewport.GetBottom() + left_over);
		viewport.SetRight(availableViewport.GetLeft() + width);
		viewport.SetTop(availableViewport.GetBottom() + left_over + v_height);

	} else {
		int v_width = (int)(height * design_aspect_ratio);
		int left_over = (width - v_width) / 2; 

		viewport.SetLeft(availableViewport.GetLeft() + left_over);
		viewport.SetBottom(availableViewport.GetBottom());
		viewport.SetRight(availableViewport.GetLeft() + v_width + left_over);
		viewport.SetTop(availableViewport.GetBottom() + height);
	}
}

	void
RAS_FramingManager::
ComputeViewport(
	const RAS_FrameSettings &settings,
	const RAS_Rect &availableViewport,
	RAS_Rect &viewport
) {

	RAS_FrameSettings::RAS_FrameType type = settings.FrameType();
	const int winx = availableViewport.GetWidth();
	const int winy = availableViewport.GetHeight();

	const float design_width = float(settings.DesignAspectWidth());
	const float design_height = float(settings.DesignAspectHeight());

	float design_aspect_ratio = float(1);

	if (design_height == float(0)) {
		// well this is ill defined 
		// lets just scale the thing

		type = RAS_FrameSettings::e_frame_scale;
	} else {
		design_aspect_ratio = design_width/design_height;
	}

	switch (type) {

		case RAS_FrameSettings::e_frame_scale :
		case RAS_FrameSettings::e_frame_extend:
		{
			viewport.SetLeft(availableViewport.GetLeft());
			viewport.SetBottom(availableViewport.GetBottom());
			viewport.SetRight(availableViewport.GetLeft() + int(winx));
			viewport.SetTop(availableViewport.GetBottom() + int(winy));

			break;
		}

		case RAS_FrameSettings::e_frame_bars:
		{
			ComputeBestFitViewRect(
				availableViewport,
				design_aspect_ratio,
				viewport
			);
		
			break;
		}
		default :
			break;
	}
}

	void
RAS_FramingManager::
ComputeFrustum(
	const RAS_FrameSettings &settings,
	const RAS_Rect &availableViewport,
	const RAS_Rect &viewport,
	const float lens,
	const float sensor_x, const float sensor_y, const short sensor_fit,
	const float shift_x,
	const float shift_y,
	const float camnear,
	const float camfar,
	RAS_FrameFrustum &frustum
) {

	RAS_FrameSettings::RAS_FrameType type = settings.FrameType();

	const float design_width = float(settings.DesignAspectWidth());
	const float design_height = float(settings.DesignAspectHeight());

	float design_aspect_ratio = float(1);

	if (design_height == float(0)) {
		// well this is ill defined 
		// lets just scale the thing

		type = RAS_FrameSettings::e_frame_scale;
	} else {
		design_aspect_ratio = design_width/design_height;
	}
	
	ComputeDefaultFrustum(
		camnear,
		camfar,
		lens,
		sensor_x,
		sensor_y,
		sensor_fit,
		shift_x,
		shift_y,
		design_aspect_ratio,
		frustum
	);

	switch (type) {

		case RAS_FrameSettings::e_frame_extend:
		{
			float x_scale, y_scale;
			switch (sensor_fit) {
				case RAS_SENSORFIT_HOR:
				{
					x_scale = 1.0f;
					y_scale = float(viewport.GetHeight()) / float(viewport.GetWidth());
					break;
				}
				case RAS_SENSORFIT_VERT:
				{
					x_scale = float(viewport.GetWidth()) / float(viewport.GetHeight());
					y_scale = 1.0f;
					break;
				}
				case RAS_SENSORFIT_AUTO:
				default:
				{
					RAS_Rect vt;
					ComputeBestFitViewRect(
						availableViewport,
						design_aspect_ratio,
						vt
					);

					// now scale the calculated frustum by the difference
					// between vt and the viewport in each axis.
					// These are always > 1

					x_scale = float(viewport.GetWidth())/float(vt.GetWidth());
					y_scale = float(viewport.GetHeight())/float(vt.GetHeight());
					break;
				}
			}

			frustum.x1 *= x_scale;
			frustum.x2 *= x_scale;
			frustum.y1 *= y_scale;
			frustum.y2 *= y_scale;
	
			break;
		}
		case RAS_FrameSettings::e_frame_scale :
		case RAS_FrameSettings::e_frame_bars:
		default :
			break;
	}
}

	void
RAS_FramingManager::
	ComputeOrtho(
		const RAS_FrameSettings &settings,
		const RAS_Rect &availableViewport,
		const RAS_Rect &viewport,
		const float scale,
		const float camnear,
		const float camfar,
		const short sensor_fit,
		const float shift_x,
		const float shift_y,
		RAS_FrameFrustum &frustum
	)
{
	RAS_FrameSettings::RAS_FrameType type = settings.FrameType();

	const float design_width = float(settings.DesignAspectWidth());
	const float design_height = float(settings.DesignAspectHeight());

	float design_aspect_ratio = float(1);

	if (design_height == float(0)) {
		// well this is ill defined 
		// lets just scale the thing
		type = RAS_FrameSettings::e_frame_scale;
	} else {
		design_aspect_ratio = design_width/design_height;
	}

	
	ComputeDefaultOrtho(
		camnear,
		camfar,
		scale,
		design_aspect_ratio,
		sensor_fit,
		shift_x,
		shift_y,
		frustum
	);

	switch (type) {

		case RAS_FrameSettings::e_frame_extend:
		{
			float x_scale, y_scale;
			switch (sensor_fit) {
				case RAS_SENSORFIT_HOR:
				{
					x_scale = 1.0f;
					y_scale = float(viewport.GetHeight()) / float(viewport.GetWidth());
					break;
				}
				case RAS_SENSORFIT_VERT:
				{
					x_scale = float(viewport.GetWidth()) / float(viewport.GetHeight());
					y_scale = 1.0f;
					break;
				}
				case RAS_SENSORFIT_AUTO:
				default:
				{
					RAS_Rect vt;
					ComputeBestFitViewRect(
						availableViewport,
						design_aspect_ratio,
						vt
						);

					// now scale the calculated frustum by the difference
					// between vt and the viewport in each axis.
					// These are always > 1

					x_scale = float(viewport.GetWidth())/float(vt.GetWidth());
					y_scale = float(viewport.GetHeight())/float(vt.GetHeight());
					break;
				}
			}

			frustum.x1 *= x_scale;
			frustum.x2 *= x_scale;
			frustum.y1 *= y_scale;
			frustum.y2 *= y_scale;
	
			break;
		}
		case RAS_FrameSettings::e_frame_scale :
		case RAS_FrameSettings::e_frame_bars:
		default :
			break;
	}
	
}


