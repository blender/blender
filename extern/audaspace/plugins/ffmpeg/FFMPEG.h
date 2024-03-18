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

#pragma once

#ifdef FFMPEG_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file FFMPEG.h
 * @ingroup plugin
 * The FFMPEG class.
 */

#include "file/IFileInput.h"
#include "file/IFileOutput.h"

AUD_NAMESPACE_BEGIN

/**
 * This plugin class reads and writes sounds via ffmpeg.
 */
class AUD_PLUGIN_API FFMPEG : public IFileInput, public IFileOutput
{
private:
	// delete copy constructor and operator=
	FFMPEG(const FFMPEG&) = delete;
	FFMPEG& operator=(const FFMPEG&) = delete;

public:
	/**
	 * Creates a new ffmpeg plugin.
	 */
	FFMPEG();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();

	virtual std::shared_ptr<IReader> createReader(std::string filename, int stream = 0);
	virtual std::shared_ptr<IReader> createReader(std::shared_ptr<Buffer> buffer, int stream = 0);
	virtual std::vector<StreamInfo> queryStreams(std::string filename);
	virtual std::vector<StreamInfo> queryStreams(std::shared_ptr<Buffer> buffer);
	virtual std::shared_ptr<IWriter> createWriter(std::string filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate);
};

AUD_NAMESPACE_END
