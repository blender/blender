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

#include "file/FileWriter.h"
#include "file/FileManager.h"
#include "util/Buffer.h"
#include "IReader.h"
#include "Exception.h"

AUD_NAMESPACE_BEGIN

std::shared_ptr<IWriter> FileWriter::createWriter(std::string filename,DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate)
{
	return FileManager::createWriter(filename, specs, format, codec, bitrate);
}

void FileWriter::writeReader(std::shared_ptr<IReader> reader, std::shared_ptr<IWriter> writer, unsigned int length, unsigned int buffersize)
{
	Buffer buffer(buffersize * AUD_SAMPLE_SIZE(writer->getSpecs()));
	sample_t* buf = buffer.getBuffer();

	int len;
	bool eos = false;
	int channels = writer->getSpecs().channels;

	for(unsigned int pos = 0; ((pos < length) || (length <= 0)) && !eos; pos += len)
	{
		len = buffersize;
		if((len > length - pos) && (length > 0))
			len = length - pos;
		reader->read(len, eos, buf);

		for(int i = 0; i < len * channels; i++)
		{
			// clamping!
			if(buf[i] > 1)
				buf[i] = 1;
			else if(buf[i] < -1)
				buf[i] = -1;
		}

		writer->write(len, buf);
	}
}

void FileWriter::writeReader(std::shared_ptr<IReader> reader, std::vector<std::shared_ptr<IWriter> >& writers, unsigned int length, unsigned int buffersize)
{
	Buffer buffer(buffersize * AUD_SAMPLE_SIZE(reader->getSpecs()));
	Buffer buffer2(buffersize * sizeof(sample_t));
	sample_t* buf = buffer.getBuffer();
	sample_t* buf2 = buffer2.getBuffer();

	int len;
	bool eos = false;
	int channels = reader->getSpecs().channels;

	for(unsigned int pos = 0; ((pos < length) || (length <= 0)) && !eos; pos += len)
	{
		len = buffersize;
		if((len > length - pos) && (length > 0))
			len = length - pos;
		reader->read(len, eos, buf);

		for(int channel = 0; channel < channels; channel++)
		{
			for(int i = 0; i < len; i++)
			{
				// clamping!
				if(buf[i * channels + channel] > 1)
					buf2[i] = 1;
				else if(buf[i * channels + channel] < -1)
					buf2[i] = -1;
				else
					buf2[i] = buf[i * channels + channel];
			}

			writers[channel]->write(len, buf2);
		}
	}
}

AUD_NAMESPACE_END
