/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "FFMPEG.h"
#include "FFMPEGReader.h"
#include "FFMPEGWriter.h"
#include "file/FileManager.h"

AUD_NAMESPACE_BEGIN

FFMPEG::FFMPEG()
{
#if LIBAVCODEC_VERSION_MAJOR < 58
	av_register_all();
#endif
}

void FFMPEG::registerPlugin()
{
	std::shared_ptr<FFMPEG> plugin = std::shared_ptr<FFMPEG>(new FFMPEG);
	FileManager::registerInput(plugin);
	FileManager::registerOutput(plugin);
}

std::shared_ptr<IReader> FFMPEG::createReader(std::string filename)
{
	return std::shared_ptr<IReader>(new FFMPEGReader(filename));
}

std::shared_ptr<IReader> FFMPEG::createReader(std::shared_ptr<Buffer> buffer)
{
	return std::shared_ptr<IReader>(new FFMPEGReader(buffer));
}

std::shared_ptr<IWriter> FFMPEG::createWriter(std::string filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate)
{
	return std::shared_ptr<IWriter>(new FFMPEGWriter(filename, specs, format, codec, bitrate));
}

#ifdef FFMPEG_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	FFMPEG::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "FFMPEG";
}
#endif

AUD_NAMESPACE_END
