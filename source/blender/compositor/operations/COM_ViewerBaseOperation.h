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
	float *outputBuffer;
	unsigned char *outputBufferDisplay;
	Image * image;
	ImageUser * imageUser;
	void *lock;
	bool active;
	const bNodeTree* tree;
	float centerX;
	float centerY;
	OrderOfChunks chunkOrder;
	bool doColorManagement;
	bool doColorPredivide;

public:
	bool isOutputOperation(bool rendering) const {return true;}
	void initExecution();
	void deinitExecution();
	void setImage(Image* image) {this->image = image;}
	void setImageUser(ImageUser* imageUser) {this->imageUser = imageUser;}
	const bool isActiveViewerOutput() const {return active;}
	void setActive(bool active) {this->active = active;}
	void setbNodeTree(const bNodeTree* tree) {this->tree = tree;}
	void setCenterX(float centerX) {this->centerX = centerX;}
	void setCenterY(float centerY) {this->centerY = centerY;}
	void setChunkOrder(OrderOfChunks tileOrder) {this->chunkOrder = tileOrder;}
	float getCenterX() { return this->centerX; }
	float getCenterY() { return this->centerY; }
	OrderOfChunks getChunkOrder() { return this->chunkOrder; }
	const int getRenderPriority() const;
	void setColorManagement(bool doColorManagement) {this->doColorManagement = doColorManagement;}
	void setColorPredivide(bool doColorPredivide) {this->doColorPredivide = doColorPredivide;}
	bool isViewerOperation() {return true;}
		
protected:
	ViewerBaseOperation();
	void updateImage(rcti*rect);

private:
	void initImage();
};
#endif
