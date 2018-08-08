/*
 * Copyright 2015, Blender Foundation.
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
 *		Dalai Felinto
 */

#ifndef __COM_OUTPUTFILEMULTIVIEWOPERATION_H__
#define __COM_OUTPUTFILEMULTIVIEWOPERATION_H__
#include "COM_NodeOperation.h"

#include "BLI_rect.h"
#include "BLI_path_util.h"

#include "DNA_color_types.h"

#include "intern/openexr/openexr_multi.h"

class OutputOpenExrSingleLayerMultiViewOperation : public OutputSingleLayerOperation {
private:
public:
	OutputOpenExrSingleLayerMultiViewOperation(const RenderData *rd, const bNodeTree *tree, DataType datatype,
	                                           ImageFormatData *format, const char *path,
	                                           const ColorManagedViewSettings *viewSettings,
	                                           const ColorManagedDisplaySettings *displaySettings,
	                                           const char *viewName);

	void *get_handle(const char *filename);
	void deinitExecution();
};

/* Writes inputs into OpenEXR multilayer channels. */
class OutputOpenExrMultiLayerMultiViewOperation : public OutputOpenExrMultiLayerOperation {
private:
public:
	OutputOpenExrMultiLayerMultiViewOperation(const RenderData *rd, const bNodeTree *tree, const char *path,
	                                          char exr_codec, bool exr_half_float, const char *viewName);

	void *get_handle(const char *filename);
	void deinitExecution();
};

/**/
class OutputStereoOperation : public OutputSingleLayerOperation {
private:
	char m_name[FILE_MAX];
	size_t m_channels;
public:
	OutputStereoOperation(const RenderData *rd, const bNodeTree *tree, DataType datatype,
	                      struct ImageFormatData *format, const char *path, const char *name,
	                      const ColorManagedViewSettings *viewSettings,
	                      const ColorManagedDisplaySettings *displaySettings, const char *viewName);
	void *get_handle(const char *filename);
	void deinitExecution();
};

#endif
