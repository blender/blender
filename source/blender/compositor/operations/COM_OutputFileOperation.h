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
#include "BKE_utildefines.h"

#include "intern/openexr/openexr_multi.h"

/* Writes the image to a single-layer file. */
class OutputSingleLayerOperation : public NodeOperation {
private:
	const Scene *scene;
	const bNodeTree *tree;
	
	ImageFormatData *format;
	char path[FILE_MAX];
	
	float *outputBuffer;
	DataType datatype;
	SocketReader *imageInput;

public:
	OutputSingleLayerOperation(const Scene *scene, const bNodeTree *tree, DataType datatype, ImageFormatData *format, const char *path);
	
	void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
	bool isOutputOperation(bool rendering) const {return true;}
	void initExecution();
	void deinitExecution();
	const int getRenderPriority() const {return 7;}
};

/* extra info for OpenEXR layers */
struct OutputOpenExrLayer {
	OutputOpenExrLayer(const char *name, DataType datatype);
	
	char name[EXR_TOT_MAXNAME-2];
	float *outputBuffer;
	DataType datatype;
	SocketReader *imageInput;
};

/* Writes inputs into OpenEXR multilayer channels. */
class OutputOpenExrMultiLayerOperation : public NodeOperation {
private:
	typedef std::vector<OutputOpenExrLayer> LayerList;
	
	const Scene *scene;
	const bNodeTree *tree;
	
	char path[FILE_MAX];
	char exr_codec;
	LayerList layers;
	
public:
	OutputOpenExrMultiLayerOperation(const Scene *scene, const bNodeTree *tree, const char *path, char exr_codec);
	
	void add_layer(const char *name, DataType datatype);
	
	void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
	bool isOutputOperation(bool rendering) const {return true;}
	void initExecution();
	void deinitExecution();
	const int getRenderPriority() const {return 7;}
};

#endif
