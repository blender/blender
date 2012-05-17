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

#include "COM_ViewerBaseOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "WM_api.h"
#include "WM_types.h"
#include "PIL_time.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"

extern "C" {
	#include "MEM_guardedalloc.h"
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}


ViewerBaseOperation::ViewerBaseOperation() : NodeOperation()
{
	this->setImage(NULL);
	this->setImageUser(NULL);
	this->outputBuffer = NULL;
	this->outputBufferDisplay = NULL;
	this->active = false;
	this->doColorManagement = true;
}

void ViewerBaseOperation::initExecution()
{
	// When initializing the tree during initial load the width and height can be zero.
	initImage();
}

void ViewerBaseOperation::initImage()
{
	Image *anImage = this->image;
	ImBuf *ibuf = BKE_image_acquire_ibuf(anImage, this->imageUser, &this->lock);
	
	if (!ibuf) return;
	if (ibuf->x != (int)getWidth() || ibuf->y != (int)getHeight()) {
		imb_freerectImBuf(ibuf);
		imb_freerectfloatImBuf(ibuf);
		IMB_freezbuffloatImBuf(ibuf);
		ibuf->x = getWidth();
		ibuf->y = getHeight();
		imb_addrectImBuf(ibuf);
		imb_addrectfloatImBuf(ibuf);
		anImage->ok = IMA_OK_LOADED;
	}
	
	/* now we combine the input with ibuf */
	this->outputBuffer = ibuf->rect_float;
	this->outputBufferDisplay = (unsigned char*)ibuf->rect;
	
	BKE_image_release_ibuf(this->image, this->lock);
}
void ViewerBaseOperation:: updateImage(rcti *rect)
{
	/// @todo: introduce new event to update smaller area
	WM_main_add_notifier(NC_WINDOW|ND_DRAW, NULL);
}

void ViewerBaseOperation::deinitExecution()
{
	this->outputBuffer = NULL;
}

const int ViewerBaseOperation::getRenderPriority() const
{
	if (this->isActiveViewerOutput()) {
		return 8;
	}
	else {
		return 0;
	}
}
