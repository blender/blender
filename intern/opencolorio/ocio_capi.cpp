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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Xavier Thomas
 *                 Lukas Toene
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <iostream>

#include <OpenColorIO/OpenColorIO.h>


#define OCIO_CAPI_IMPLEMENTATION
#include "ocio_capi.h"

ConstConfigRcPtr* OCIO_getCurrentConfig(void)
{
	ConstConfigRcPtr* config =  new ConstConfigRcPtr();
	try
	{
		*config = GetCurrentConfig();
		if(*config)
			return config;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

void OCIO_setCurrentConfig(const ConstConfigRcPtr* config)
{
	if(config)
	{
		try
		{
			SetCurrentConfig(*config);
		}
		catch(Exception & exception)
		{
			std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
		}
	}
}

ConstConfigRcPtr* OCIO_configCreateFromEnv(void)
{
	ConstConfigRcPtr* config =  new ConstConfigRcPtr();
	try
	{
		*config = Config::CreateFromEnv();
		if(*config)
			return config;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}


ConstConfigRcPtr* OCIO_configCreateFromFile(const char* filename)
{
	ConstConfigRcPtr* config =  new ConstConfigRcPtr();
	try
	{
		*config = Config::CreateFromFile(filename);
		if(*config)
			return config;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

void OCIO_configRelease(ConstConfigRcPtr* config)
{
	if(config){
		delete config;
		config =0;
	}
}

int OCIO_configGetNumColorSpaces(ConstConfigRcPtr* config)
{
	try
	{
		return (*config)->getNumColorSpaces();
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

const char* OCIO_configGetColorSpaceNameByIndex(ConstConfigRcPtr* config, int index)
{
	try
	{
		return (*config)->getColorSpaceNameByIndex(index);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

ConstColorSpaceRcPtr* OCIO_configGetColorSpace(ConstConfigRcPtr* config, const char* name)
{
	ConstColorSpaceRcPtr* cs =  new ConstColorSpaceRcPtr();
	try
	{
		*cs = (*config)->getColorSpace(name);
		if(*cs)
			return cs;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
		delete cs;
	}
	return 0;
}

int OCIO_configGetIndexForColorSpace(ConstConfigRcPtr* config, const char* name)
{
	try
	{
		return (*config)->getIndexForColorSpace(name);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return -1;
}

const char* OCIO_configGetDefaultDisplay(ConstConfigRcPtr* config)
{
	try
	{
		return (*config)->getDefaultDisplay();
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

int OCIO_configGetNumDisplays(ConstConfigRcPtr* config)
{
	try
	{
		return (*config)->getNumDisplays();
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

const char* OCIO_configGetDisplay(ConstConfigRcPtr* config, int index)
{
	try
	{
		return (*config)->getDisplay(index);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

const char* OCIO_configGetDefaultView(ConstConfigRcPtr* config, const char* display)
{
	try
	{
		return (*config)->getDefaultView(display);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

int OCIO_configGetNumViews(ConstConfigRcPtr* config, const char* display)
{
	try
	{
		return (*config)->getNumViews(display);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

const char* OCIO_configGetView(ConstConfigRcPtr* config, const char* display, int index)
{
	try
	{
		return (*config)->getView(display, index);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

const char* OCIO_configGetDisplayColorSpaceName(ConstConfigRcPtr* config, const char* display, const char* view)
{
	try
	{
		return (*config)->getDisplayColorSpaceName(display, view);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}




void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr* cs)
{
	if(cs){
		delete cs;
		cs =0;
	}
}





ConstProcessorRcPtr* OCIO_configGetProcessorWithNames(ConstConfigRcPtr* config, const char* srcName, const char* dstName)
{
	ConstProcessorRcPtr* p =  new ConstProcessorRcPtr();
	try
	{
		*p = (*config)->getProcessor(srcName, dstName);
		if(*p)
			return p;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

extern ConstProcessorRcPtr* OCIO_configGetProcessor(ConstConfigRcPtr* config, ConstTransformRcPtr* transform)
{
	ConstProcessorRcPtr* p =  new ConstProcessorRcPtr();
	try
	{
		*p = (*config)->getProcessor(*transform);
		if(*p)
			return p;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

void OCIO_processorApply(ConstProcessorRcPtr* processor, PackedImageDesc* img)
{
	try
	{
		(*processor)->apply(*img);
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
}

void OCIO_processorApplyRGB(ConstProcessorRcPtr* processor, float* pixel)
{
	(*processor)->applyRGB(pixel);
}

void OCIO_processorApplyRGBA(ConstProcessorRcPtr* processor, float* pixel)
{
	(*processor)->applyRGBA(pixel);
}

void OCIO_processorRelease(ConstProcessorRcPtr* p)
{
	if(p){
		delete p;
		p = 0;
	}
}

const char* OCIO_colorSpaceGetName(ConstColorSpaceRcPtr* cs)
{
	return (*cs)->getName();
}

const char* OCIO_colorSpaceGetFamily(ConstColorSpaceRcPtr* cs)
{
	return (*cs)->getFamily();
}


extern DisplayTransformRcPtr* OCIO_createDisplayTransform(void)
{
	DisplayTransformRcPtr* dt =  new DisplayTransformRcPtr();
	*dt = DisplayTransform::Create();
	return dt;
}

extern void OCIO_displayTransformSetInputColorSpaceName(DisplayTransformRcPtr* dt, const char * name)
{
	(*dt)->setInputColorSpaceName(name);
}

extern void OCIO_displayTransformSetDisplay(DisplayTransformRcPtr* dt, const char * name)
{
	(*dt)->setDisplay(name);
}

extern void OCIO_displayTransformSetView(DisplayTransformRcPtr* dt, const char * name)
{
	(*dt)->setView(name);
}

extern void OCIO_displayTransformRelease(DisplayTransformRcPtr* dt)
{
	if(dt){
		delete dt;
		dt = 0;
	}
}

PackedImageDesc* OCIO_createPackedImageDesc(float * data, long width, long height, long numChannels,
											long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	try
	{
		PackedImageDesc* id = new PackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);
		return id;
	}
	catch(Exception & exception)
	{
		std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
	}
	return 0;
}

void OCIO_packedImageDescRelease(PackedImageDesc* id)
{
	if(id){
		delete id;
		id = 0;
	}
}

