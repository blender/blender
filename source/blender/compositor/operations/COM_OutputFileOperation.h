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
 *		Lukas Tönne
 */

#ifndef _COM_OutputFileOperation_h
#define _COM_OutputFileOperation_h
#include "COM_NodeOperation.h"

#include "BLI_rect.h"
#include "BLI_path_util.h"

#include "DNA_color_types.h"

#include "intern/openexr/openexr_multi.h"

/* Writes the image to a single-layer file. */
class OutputSingleLayerOperation : public NodeOperation {
private:
	const RenderData *m_rd;
	const bNodeTree *m_tree;
	
	ImageFormatData *m_format;
	char m_path[FILE_MAX];
	
	float *m_outputBuffer;
	DataType m_datatype;
	SocketReader *m_imageInput;

	const ColorManagedViewSettings *m_viewSettings;
	const ColorManagedDisplaySettings *m_displaySettings;
public:
	OutputSingleLayerOperation(const RenderData *rd, const bNodeTree *tree, DataType datatype, ImageFormatData *format, const char *path,
	                           const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings);
	
	void executeRegion(rcti *rect, unsigned int tileNumber);
	bool isOutputOperation(bool rendering) const { return true; }
	void initExecution();
	void deinitExecution();
	const CompositorPriority getRenderPriority() const { return COM_PRIORITY_LOW; }

	bool isFileOutputOperation() { return true; }
};

/* extra info for OpenEXR layers */
struct OutputOpenExrLayer {
	OutputOpenExrLayer(const char *name, DataType datatype);
	
	char name[EXR_TOT_MAXNAME - 2];
	float *outputBuffer;
	DataType datatype;
	SocketReader *imageInput;
};

/* Writes inputs into OpenEXR multilayer channels. */
class OutputOpenExrMultiLayerOperation : public NodeOperation {
private:
	typedef std::vector<OutputOpenExrLayer> LayerList;
	
	const RenderData *m_rd;
	const bNodeTree *m_tree;
	
	char m_path[FILE_MAX];
	char m_exr_codec;
	LayerList m_layers;
	
public:
	OutputOpenExrMultiLayerOperation(const RenderData *rd, const bNodeTree *tree, const char *path, char exr_codec);
	
	void add_layer(const char *name, DataType datatype);
	
	void executeRegion(rcti *rect, unsigned int tileNumber);
	bool isOutputOperation(bool rendering) const { return true; }
	void initExecution();
	void deinitExecution();
	const CompositorPriority getRenderPriority() const { return COM_PRIORITY_LOW; }

	bool isFileOutputOperation() { return true; }
};

#endif
