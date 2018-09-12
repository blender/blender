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

#ifndef __COM_PREVIEWOPERATION_H__
#define __COM_PREVIEWOPERATION_H__
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "DNA_color_types.h"
#include "BLI_rect.h"
#include "BKE_global.h"

class PreviewOperation : public NodeOperation {
protected:
	unsigned char *m_outputBuffer;

	/**
	 * \brief holds reference to the SDNA bNode, where this nodes will render the preview image for
	 */
	bNodePreview *m_preview;
	SocketReader *m_input;
	float m_divider;

	const ColorManagedViewSettings *m_viewSettings;
	const ColorManagedDisplaySettings *m_displaySettings;
public:
	PreviewOperation(const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings);
	void verifyPreview(bNodeInstanceHash *previews, bNodeInstanceKey key);

	bool isOutputOperation(bool /*rendering*/) const { return !G.background; }
	void initExecution();
	void deinitExecution();
	CompositorPriority getRenderPriority() const;

	void executeRegion(rcti *rect, unsigned int tileNumber);
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	bool isPreviewOperation() const { return true; }

};
#endif
