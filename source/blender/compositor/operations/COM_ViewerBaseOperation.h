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

#ifndef _COM_ViewerBaseOperation_h
#define _COM_ViewerBaseOperation_h
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"

class ViewerBaseOperation : public NodeOperation {
protected:
	float *m_outputBuffer;
	unsigned char *m_outputBufferDisplay;
	Image *m_image;
	ImageUser *m_imageUser;
	void *m_lock;
	bool m_active;
	float m_centerX;
	float m_centerY;
	OrderOfChunks m_chunkOrder;
	bool m_doColorManagement;
	bool m_doColorPredivide;

public:
	bool isOutputOperation(bool rendering) const { return isActiveViewerOutput(); }
	void initExecution();
	void deinitExecution();
	void setImage(Image *image) { this->m_image = image; }
	void setImageUser(ImageUser *imageUser) { this->m_imageUser = imageUser; }
	const bool isActiveViewerOutput() const { return this->m_active; }
	void setActive(bool active) { this->m_active = active; }
	void setCenterX(float centerX) { this->m_centerX = centerX;}
	void setCenterY(float centerY) { this->m_centerY = centerY;}
	void setChunkOrder(OrderOfChunks tileOrder) { this->m_chunkOrder = tileOrder; }
	float getCenterX() { return this->m_centerX; }
	float getCenterY() { return this->m_centerY; }
	OrderOfChunks getChunkOrder() { return this->m_chunkOrder; }
	const CompositorPriority getRenderPriority() const;
	void setColorManagement(bool doColorManagement) { this->m_doColorManagement = doColorManagement; }
	void setColorPredivide(bool doColorPredivide) { this->m_doColorPredivide = doColorPredivide; }
	bool isViewerOperation() { return true; }
		
protected:
	ViewerBaseOperation();
	void updateImage(rcti *rect);

private:
	void initImage();
};
#endif
