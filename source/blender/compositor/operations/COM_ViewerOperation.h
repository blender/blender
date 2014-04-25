/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_ViewerOperation_h
#define _COM_ViewerOperation_h
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"
#include "BKE_global.h"

class ViewerOperation : public NodeOperation {
private:
	float *m_outputBuffer;
	float *m_depthBuffer;
	Image *m_image;
	ImageUser *m_imageUser;
	bool m_active;
	float m_centerX;
	float m_centerY;
	OrderOfChunks m_chunkOrder;
	bool m_doDepthBuffer;
	ImBuf *m_ibuf;
	bool m_useAlphaInput;
	
	const ColorManagedViewSettings *m_viewSettings;
	const ColorManagedDisplaySettings *m_displaySettings;
	
	SocketReader *m_imageInput;
	SocketReader *m_alphaInput;
	SocketReader *m_depthInput;

public:
	ViewerOperation();
	void initExecution();
	void deinitExecution();
	void executeRegion(rcti *rect, unsigned int tileNumber);
	bool isOutputOperation(bool rendering) const { if (G.background) return false; return isActiveViewerOutput(); }
	void setImage(Image *image) { this->m_image = image; }
	void setImageUser(ImageUser *imageUser) { this->m_imageUser = imageUser; }
	const bool isActiveViewerOutput() const { return this->m_active; }
	void setActive(bool active) { this->m_active = active; }
	void setCenterX(float centerX) { this->m_centerX = centerX;}
	void setCenterY(float centerY) { this->m_centerY = centerY;}
	void setChunkOrder(OrderOfChunks tileOrder) { this->m_chunkOrder = tileOrder; }
	float getCenterX() const { return this->m_centerX; }
	float getCenterY() const { return this->m_centerY; }
	OrderOfChunks getChunkOrder() const { return this->m_chunkOrder; }
	const CompositorPriority getRenderPriority() const;
	bool isViewerOperation() const { return true; }
	void setUseAlphaInput(bool value) { this->m_useAlphaInput = value; }

	void setViewSettings(const ColorManagedViewSettings *viewSettings) { this->m_viewSettings = viewSettings; }
	void setDisplaySettings(const ColorManagedDisplaySettings *displaySettings) { this->m_displaySettings = displaySettings; }

private:
	void updateImage(rcti *rect);
	void initImage();
};
#endif
