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

#include "COM_OutputFileOperation.h"
#include <string.h>
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_color_types.h"

extern "C" {
#  include "MEM_guardedalloc.h"
#  include "IMB_imbuf.h"
#  include "IMB_colormanagement.h"
#  include "IMB_imbuf_types.h"
}

void add_exr_channels(void *exrhandle, const char *layerName, const DataType datatype,
                      const char *viewName, const size_t width, bool use_half_float, float *buf)
{
	/* create channels */
	switch (datatype) {
		case COM_DT_VALUE:
			IMB_exr_add_channel(exrhandle, layerName, "V", viewName, 1, width, buf ? buf : NULL, use_half_float);
			break;
		case COM_DT_VECTOR:
			IMB_exr_add_channel(exrhandle, layerName, "X", viewName, 3, 3 * width, buf ? buf : NULL, use_half_float);
			IMB_exr_add_channel(exrhandle, layerName, "Y", viewName, 3, 3 * width, buf ? buf + 1 : NULL, use_half_float);
			IMB_exr_add_channel(exrhandle, layerName, "Z", viewName, 3, 3 * width, buf ? buf + 2 : NULL, use_half_float);
			break;
		case COM_DT_COLOR:
			IMB_exr_add_channel(exrhandle, layerName, "R", viewName, 4, 4 * width, buf ? buf : NULL, use_half_float);
			IMB_exr_add_channel(exrhandle, layerName, "G", viewName, 4, 4 * width, buf ? buf + 1 : NULL, use_half_float);
			IMB_exr_add_channel(exrhandle, layerName, "B", viewName, 4, 4 * width, buf ? buf + 2 : NULL, use_half_float);
			IMB_exr_add_channel(exrhandle, layerName, "A", viewName, 4, 4 * width, buf ? buf + 3 : NULL, use_half_float);
			break;
		default:
			break;
	}
}

void free_exr_channels(void *exrhandle, const RenderData *rd, const char *layerName, const DataType datatype)
{
	SceneRenderView *srv;

	/* check renderdata for amount of views */
	for (srv = (SceneRenderView *) rd->views.first; srv; srv = srv->next) {
		float *rect = NULL;

		if (BKE_scene_multiview_is_render_view_active(rd, srv) == false)
			continue;

		/* the pointer is stored in the first channel of each datatype */
		switch (datatype) {
			case COM_DT_VALUE:
				rect = IMB_exr_channel_rect(exrhandle, layerName, "V", srv->name);
				break;
			case COM_DT_VECTOR:
				rect = IMB_exr_channel_rect(exrhandle, layerName, "X", srv->name);
				break;
			case COM_DT_COLOR:
				rect = IMB_exr_channel_rect(exrhandle, layerName, "R", srv->name);
				break;
			default:
				break;
		}
		if (rect)
			MEM_freeN(rect);
	}
}

int get_datatype_size(DataType datatype)
{
	switch (datatype) {
		case COM_DT_VALUE:  return 1;
		case COM_DT_VECTOR: return 3;
		case COM_DT_COLOR:  return 4;
		default:            return 0;
	}
}

static float *init_buffer(unsigned int width, unsigned int height, DataType datatype)
{
	// When initializing the tree during initial load the width and height can be zero.
	if (width != 0 && height != 0) {
		int size = get_datatype_size(datatype);
		return (float *)MEM_callocN(width * height * size * sizeof(float), "OutputFile buffer");
	}
	else
		return NULL;
}

static void write_buffer_rect(rcti *rect, const bNodeTree *tree,
                              SocketReader *reader, float *buffer, unsigned int width, DataType datatype)
{
	float color[4];
	int i, size = get_datatype_size(datatype);

	if (!buffer) return;
	int x1 = rect->xmin;
	int y1 = rect->ymin;
	int x2 = rect->xmax;
	int y2 = rect->ymax;
	int offset = (y1 * width + x1) * size;
	int x;
	int y;
	bool breaked = false;

	for (y = y1; y < y2 && (!breaked); y++) {
		for (x = x1; x < x2 && (!breaked); x++) {
			reader->readSampled(color, x, y, COM_PS_NEAREST);
			
			for (i = 0; i < size; ++i)
				buffer[offset + i] = color[i];
			offset += size;
			
			if (tree->test_break && tree->test_break(tree->tbh))
				breaked = true;
		}
		offset += (width - (x2 - x1)) * size;
	}
}


OutputSingleLayerOperation::OutputSingleLayerOperation(
        const RenderData *rd, const bNodeTree *tree, DataType datatype, ImageFormatData *format, const char *path,
        const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings, const char *viewName)
{
	this->m_rd = rd;
	this->m_tree = tree;
	
	this->addInputSocket(datatype);
	
	this->m_outputBuffer = NULL;
	this->m_datatype = datatype;
	this->m_imageInput = NULL;
	
	this->m_format = format;
	BLI_strncpy(this->m_path, path, sizeof(this->m_path));

	this->m_viewSettings = viewSettings;
	this->m_displaySettings = displaySettings;
	this->m_viewName = viewName;
}

void OutputSingleLayerOperation::initExecution()
{
	this->m_imageInput = getInputSocketReader(0);
	this->m_outputBuffer = init_buffer(this->getWidth(), this->getHeight(), this->m_datatype);
}

void OutputSingleLayerOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
	write_buffer_rect(rect, this->m_tree, this->m_imageInput, this->m_outputBuffer, this->getWidth(), this->m_datatype);
}

void OutputSingleLayerOperation::deinitExecution()
{
	if (this->getWidth() * this->getHeight() != 0) {
		
		int size = get_datatype_size(this->m_datatype);
		ImBuf *ibuf = IMB_allocImBuf(this->getWidth(), this->getHeight(), this->m_format->planes, 0);
		Main *bmain = G.main; /* TODO, have this passed along */
		char filename[FILE_MAX];
		const char *suffix;
		
		ibuf->channels = size;
		ibuf->rect_float = this->m_outputBuffer;
		ibuf->mall |= IB_rectfloat; 
		ibuf->dither = this->m_rd->dither_intensity;
		
		IMB_colormanagement_imbuf_for_write(ibuf, true, false, m_viewSettings, m_displaySettings,
		                                    this->m_format);

		suffix = BKE_scene_multiview_view_suffix_get(this->m_rd, this->m_viewName);

		BKE_image_path_from_imformat(
		        filename, this->m_path, bmain->name, this->m_rd->cfra, this->m_format,
		        (this->m_rd->scemode & R_EXTENSION) != 0, true, suffix);

		if (0 == BKE_imbuf_write(ibuf, filename, this->m_format))
			printf("Cannot save Node File Output to %s\n", filename);
		else
			printf("Saved: %s\n", filename);
		
		IMB_freeImBuf(ibuf);
	}
	this->m_outputBuffer = NULL;
	this->m_imageInput = NULL;
}

/******************************* MultiLayer *******************************/

OutputOpenExrLayer::OutputOpenExrLayer(const char *name_, DataType datatype_, bool use_layer_)
{
	BLI_strncpy(this->name, name_, sizeof(this->name));
	this->datatype = datatype_;
	this->use_layer = use_layer_;
	
	/* these are created in initExecution */
	this->outputBuffer = 0;
	this->imageInput = 0;
}

OutputOpenExrMultiLayerOperation::OutputOpenExrMultiLayerOperation(
        const RenderData *rd, const bNodeTree *tree, const char *path,
        char exr_codec, bool exr_half_float, const char *viewName)
{
	this->m_rd = rd;
	this->m_tree = tree;
	
	BLI_strncpy(this->m_path, path, sizeof(this->m_path));
	this->m_exr_codec = exr_codec;
	this->m_exr_half_float = exr_half_float;
	this->m_viewName = viewName;
}

void OutputOpenExrMultiLayerOperation::add_layer(const char *name, DataType datatype, bool use_layer)
{
	this->addInputSocket(datatype);
	this->m_layers.push_back(OutputOpenExrLayer(name, datatype, use_layer));
}

void OutputOpenExrMultiLayerOperation::initExecution()
{
	for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
		if (this->m_layers[i].use_layer) {
			SocketReader *reader = getInputSocketReader(i);
			this->m_layers[i].imageInput = reader;
			this->m_layers[i].outputBuffer = init_buffer(this->getWidth(), this->getHeight(), this->m_layers[i].datatype);
		}
	}
}

void OutputOpenExrMultiLayerOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
	for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
		OutputOpenExrLayer &layer = this->m_layers[i];
		if (layer.imageInput)
			write_buffer_rect(rect, this->m_tree, layer.imageInput, layer.outputBuffer, this->getWidth(), layer.datatype);
	}
}

void OutputOpenExrMultiLayerOperation::deinitExecution()
{
	unsigned int width = this->getWidth();
	unsigned int height = this->getHeight();
	if (width != 0 && height != 0) {
		Main *bmain = G.main; /* TODO, have this passed along */
		char filename[FILE_MAX];
		const char *suffix;
		void *exrhandle = IMB_exr_get_handle();

		suffix = BKE_scene_multiview_view_suffix_get(this->m_rd, this->m_viewName);
		BKE_image_path_from_imtype(
		        filename, this->m_path, bmain->name, this->m_rd->cfra, R_IMF_IMTYPE_MULTILAYER,
		        (this->m_rd->scemode & R_EXTENSION) != 0, true, suffix);
		BLI_make_existing_file(filename);

		for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
			OutputOpenExrLayer &layer = this->m_layers[i];
			if (!layer.imageInput)
				continue; /* skip unconnected sockets */
			
			add_exr_channels(exrhandle, this->m_layers[i].name, this->m_layers[i].datatype, "", width,
			                 this->m_exr_half_float, this->m_layers[i].outputBuffer);
		}
		
		/* when the filename has no permissions, this can fail */
		if (IMB_exr_begin_write(exrhandle, filename, width, height, this->m_exr_codec, NULL)) {
			IMB_exr_write_channels(exrhandle);
		}
		else {
			/* TODO, get the error from openexr's exception */
			/* XXX nice way to do report? */
			printf("Error Writing Render Result, see console\n");
		}
		
		IMB_exr_close(exrhandle);
		for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
			if (this->m_layers[i].outputBuffer) {
				MEM_freeN(this->m_layers[i].outputBuffer);
				this->m_layers[i].outputBuffer = NULL;
			}
			
			this->m_layers[i].imageInput = NULL;
		}
	}
}

