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

#include "PySound.h"
#include "PySource.h"
#include "PyThreadPool.h"

#ifdef WITH_CONVOLUTION
#include "PyHRTF.h"
#include "PyImpulseResponse.h"
#endif

#include "Exception.h"
#include "file/File.h"
#include "file/FileWriter.h"
#include "util/StreamBuffer.h"
#include "generator/Sawtooth.h"
#include "generator/Silence.h"
#include "generator/Sine.h"
#include "generator/Square.h"
#include "generator/Triangle.h"
#include "fx/Accumulator.h"
#include "fx/ADSR.h"
#include "fx/Delay.h"
#include "fx/Envelope.h"
#include "fx/Fader.h"
#include "fx/Highpass.h"
#include "fx/IIRFilter.h"
#include "fx/Limiter.h"
#include "fx/Loop.h"
#include "fx/Lowpass.h"
#include "fx/Modulator.h"
#include "fx/MutableSound.h"
#include "fx/Pitch.h"
#include "fx/Reverse.h"
#include "fx/SoundList.h"
#include "fx/Sum.h"
#include "fx/Threshold.h"
#include "fx/Volume.h"
#include "respec/ChannelMapper.h"
#include "respec/ChannelMapperReader.h"
#include "respec/LinearResample.h"
#include "respec/JOSResample.h"
#include "respec/JOSResampleReader.h"
#include "sequence/Double.h"
#include "sequence/PingPong.h"
#include "sequence/Superpose.h"

#ifdef WITH_CONVOLUTION
#include "fx/BinauralSound.h"
#include "fx/ConvolverSound.h"
#endif

#include <cstring>
#include <structmember.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/ndarrayobject.h>

using namespace aud;

extern PyObject* AUDError;

static void
Sound_dealloc(Sound* self)
{
	if(self->sound)
		delete reinterpret_cast<std::shared_ptr<ISound>*>(self->sound);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Sound_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		static const char* kwlist[] = {"filename", nullptr};
		const char* filename = nullptr;

		if(!PyArg_ParseTupleAndKeywords(args, kwds, "s:Sound", const_cast<char**>(kwlist), &filename))
		{
			Py_DECREF(self);
			return nullptr;
		}

		try
		{
			self->sound = new std::shared_ptr<ISound>(new File(filename));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_data_doc,
			 "data()\n\n"
			 "Retrieves the data of the sound as numpy array.\n\n"
			 ":return: A two dimensional numpy float array.\n"
			 ":rtype: :class:`numpy.ndarray`\n\n"
			 ".. note:: Best efficiency with cached sounds.");

static PyObject *
Sound_data(Sound* self)
{
	std::shared_ptr<ISound> sound = *reinterpret_cast<std::shared_ptr<ISound>*>(self->sound);

	auto stream_buffer = std::dynamic_pointer_cast<StreamBuffer>(sound);
	if(!stream_buffer)
		stream_buffer = std::make_shared<StreamBuffer>(sound);
	Specs specs = stream_buffer->getSpecs();
	auto buffer = stream_buffer->getBuffer();

	npy_intp dimensions[2];
	dimensions[0] = buffer->getSize() / AUD_SAMPLE_SIZE(specs);
	dimensions[1] = specs.channels;

	PyArrayObject* array = reinterpret_cast<PyArrayObject*>(PyArray_SimpleNew(2, dimensions, NPY_FLOAT));

	sample_t* data = reinterpret_cast<sample_t*>(PyArray_DATA(array));

	std::memcpy(data, buffer->getBuffer(), buffer->getSize());

	return reinterpret_cast<PyObject*>(array);
}

PyDoc_STRVAR(M_aud_Sound_write_doc,
			 "write(filename, rate, channels, format, container, codec, bitrate, buffersize)\n\n"
			 "Writes the sound to a file.\n\n"
			 ":arg filename: The path to write to.\n"
			 ":type filename: string\n"
			 ":arg rate: The sample rate to write with.\n"
			 ":type rate: int\n"
			 ":arg channels: The number of channels to write with.\n"
			 ":type channels: int\n"
			 ":arg format: The sample format to write with.\n"
			 ":type format: int\n"
			 ":arg container: The container format for the file.\n"
			 ":type container: int\n"
			 ":arg codec: The codec to use in the file.\n"
			 ":type codec: int\n"
			 ":arg bitrate: The bitrate to write with.\n"
			 ":type bitrate: int\n"
			 ":arg buffersize: The size of the writing buffer.\n"
			 ":type buffersize: int\n");

static PyObject *
Sound_write(Sound* self, PyObject* args, PyObject* kwds)
{
	const char* filename = nullptr;
	int rate = RATE_INVALID;
	Channels channels = CHANNELS_INVALID;
	SampleFormat format = FORMAT_INVALID;
	Container container = CONTAINER_INVALID;
	Codec codec = CODEC_INVALID;
	int bitrate = 0;
	int buffersize = 0;

	static const char* kwlist[] = {"filename", "rate", "channels", "format", "container", "codec", "bitrate", "buffersize", nullptr};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|iiiiiii:write", const_cast<char**>(kwlist), &filename, &rate, &channels, &format, &container, &codec, &bitrate, &buffersize))
		return nullptr;

	try
	{
		std::shared_ptr<IReader> reader = (*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound))->createReader();

		DeviceSpecs specs;
		specs.specs = reader->getSpecs();

		if((rate != RATE_INVALID) && (specs.rate != rate))
		{
			specs.rate = rate;
			reader = std::make_shared<JOSResampleReader>(reader, rate);
		}

		if((channels != CHANNELS_INVALID) && (specs.channels != channels))
		{
			specs.channels = channels;
			reader = std::make_shared<ChannelMapperReader>(reader, channels);
		}

		if(format == FORMAT_INVALID)
			format = FORMAT_S16;
		specs.format = format;

		const char* invalid_container_error = "Container could not be determined from filename.";

		if(container == CONTAINER_INVALID)
		{
			std::string path = filename;

			if(path.length() < 4)
			{
				PyErr_SetString(AUDError, invalid_container_error);
				return nullptr;
			}

			std::string extension = path.substr(path.length() - 4);

			if(extension == ".ac3")
				container = CONTAINER_AC3;
			else if(extension == "flac")
				container = CONTAINER_FLAC;
			else if(extension == ".mkv")
				container = CONTAINER_MATROSKA;
			else if(extension == ".mp2")
				container = CONTAINER_MP2;
			else if(extension == ".mp3")
				container = CONTAINER_MP3;
			else if(extension == ".ogg")
				container = CONTAINER_OGG;
			else if(extension == ".wav")
				container = CONTAINER_WAV;
			else
			{
				PyErr_SetString(AUDError, invalid_container_error);
				return nullptr;
			}
		}

		if(codec == CODEC_INVALID)
		{
			switch(container)
			{
			case CONTAINER_AC3:
				codec = CODEC_AC3;
				break;
			case CONTAINER_FLAC:
				codec = CODEC_FLAC;
				break;
			case CONTAINER_MATROSKA:
				codec = CODEC_OPUS;
				break;
			case CONTAINER_MP2:
				codec = CODEC_MP2;
				break;
			case CONTAINER_MP3:
				codec = CODEC_MP3;
				break;
			case CONTAINER_OGG:
				codec = CODEC_VORBIS;
				break;
			case CONTAINER_WAV:
				codec = CODEC_PCM;
				break;
			default:
				PyErr_SetString(AUDError, "Unknown container, cannot select default codec.");
				return nullptr;
			}
		}

		if(buffersize <= 0)
			buffersize = AUD_DEFAULT_BUFFER_SIZE;

		std::shared_ptr<IWriter> writer = FileWriter::createWriter(filename, specs, container, codec, bitrate);
		FileWriter::writeReader(reader, writer, 0, buffersize);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(M_aud_Sound_buffer_doc,
			 "buffer(data, rate)\n\n"
			 "Creates a sound from a data buffer.\n\n"
			 ":arg data: The data as two dimensional numpy array.\n"
			 ":type data: numpy.ndarray\n"
			 ":arg rate: The sample rate.\n"
			 ":type rate: double\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_buffer(PyTypeObject* type, PyObject* args)
{
	PyArrayObject* array = nullptr;
	double rate = RATE_INVALID;

	if(!PyArg_ParseTuple(args, "Od:buffer", &array, &rate))
		return nullptr;

	if((!PyObject_TypeCheck(reinterpret_cast<PyObject*>(array), &PyArray_Type)) || (PyArray_TYPE(array) != NPY_FLOAT))
	{
		PyErr_SetString(PyExc_TypeError, "The data needs to be supplied as float32 numpy array!");
		return nullptr;
	}

	if(PyArray_NDIM(array) > 2)
	{
		PyErr_SetString(PyExc_TypeError, "The array needs to have one or two dimensions!");
		return nullptr;
	}

	if(rate <= 0)
	{
		PyErr_SetString(PyExc_TypeError, "The sample rate has to be positive!");
		return nullptr;
	}

	Specs specs;
	specs.rate = rate;
	specs.channels = CHANNELS_MONO;

	if(PyArray_NDIM(array) == 2)
		specs.channels = static_cast<Channels>(PyArray_DIM(array, 1));

	int size = PyArray_DIM(array, 0) * AUD_SAMPLE_SIZE(specs);

	std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(size);

	std::memcpy(buffer->getBuffer(), PyArray_DATA(array), size);

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<StreamBuffer>(new StreamBuffer(buffer, specs));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_cache_doc,
			 "cache()\n\n"
			 "Caches a sound into RAM.\n"
			 "This saves CPU usage needed for decoding and file access if the "
			 "underlying sound reads from a file on the harddisk, but it "
			 "consumes a lot of memory.\n\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: Only known-length factories can be buffered.\n\n"
			 ".. warning:: Raw PCM data needs a lot of space, only buffer "
			 "short factories.");

static PyObject *
Sound_cache(Sound* self)
{
	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new StreamBuffer(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_file_doc,
			 "file(filename)\n\n"
			 "Creates a sound object of a sound file.\n\n"
			 ":arg filename: Path of the file.\n"
			 ":type filename: string\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. warning:: If the file doesn't exist or can't be read you will "
			 "not get an exception immediately, but when you try to start "
			 "playback of that sound.");

static PyObject *
Sound_file(PyTypeObject* type, PyObject* args)
{
	const char* filename = nullptr;

	if(!PyArg_ParseTuple(args, "s:file", &filename))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new File(filename));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_sawtooth_doc,
			 "sawtooth(frequency, rate=48000)\n\n"
			 "Creates a sawtooth sound which plays a sawtooth wave.\n\n"
			 ":arg frequency: The frequency of the sawtooth wave in Hz.\n"
			 ":type frequency: float\n"
			 ":arg rate: The sampling rate in Hz. It's recommended to set this "
			 "value to the playback device's samling rate to avoid resamping.\n"
			 ":type rate: int\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_sawtooth(PyTypeObject* type, PyObject* args)
{
	float frequency;
	double rate = 48000;

	if(!PyArg_ParseTuple(args, "f|d:sawtooth", &frequency, &rate))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new Sawtooth(frequency, (SampleRate)rate));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_silence_doc,
			 "silence(rate=48000)\n\n"
			 "Creates a silence sound which plays simple silence.\n\n"
			 ":arg rate: The sampling rate in Hz. It's recommended to set this "
			 "value to the playback device's samling rate to avoid resamping.\n"
			 ":type rate: int\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_silence(PyTypeObject* type, PyObject* args)
{
	double rate = 48000;

	if(!PyArg_ParseTuple(args, "|d:sawtooth", &rate))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new Silence((SampleRate)rate));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_sine_doc,
			 "sine(frequency, rate=48000)\n\n"
			 "Creates a sine sound which plays a sine wave.\n\n"
			 ":arg frequency: The frequency of the sine wave in Hz.\n"
			 ":type frequency: float\n"
			 ":arg rate: The sampling rate in Hz. It's recommended to set this "
			 "value to the playback device's samling rate to avoid resamping.\n"
			 ":type rate: int\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_sine(PyTypeObject* type, PyObject* args)
{
	float frequency;
	double rate = 48000;

	if(!PyArg_ParseTuple(args, "f|d:sine", &frequency, &rate))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new Sine(frequency, (SampleRate)rate));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_square_doc,
			 "square(frequency, rate=48000)\n\n"
			 "Creates a square sound which plays a square wave.\n\n"
			 ":arg frequency: The frequency of the square wave in Hz.\n"
			 ":type frequency: float\n"
			 ":arg rate: The sampling rate in Hz. It's recommended to set this "
			 "value to the playback device's samling rate to avoid resamping.\n"
			 ":type rate: int\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_square(PyTypeObject* type, PyObject* args)
{
	float frequency;
	double rate = 48000;

	if(!PyArg_ParseTuple(args, "f|d:square", &frequency, &rate))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new Square(frequency, (SampleRate)rate));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_triangle_doc,
			 "triangle(frequency, rate=48000)\n\n"
			 "Creates a triangle sound which plays a triangle wave.\n\n"
			 ":arg frequency: The frequency of the triangle wave in Hz.\n"
			 ":type frequency: float\n"
			 ":arg rate: The sampling rate in Hz. It's recommended to set this "
			 "value to the playback device's samling rate to avoid resamping.\n"
			 ":type rate: int\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_triangle(PyTypeObject* type, PyObject* args)
{
	float frequency;
	double rate = 48000;

	if(!PyArg_ParseTuple(args, "f|d:triangle", &frequency, &rate))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new Triangle(frequency, (SampleRate)rate));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_accumulate_doc,
			 "accumulate(additive=False)\n\n"
			 "Accumulates a sound by summing over positive input differences thus generating a monotonic sigal. "
			 "If additivity is set to true negative input differences get added too, but positive ones with a factor of two. "
			 "Note that with additivity the signal is not monotonic anymore.\n\n"
			 ":arg additive: Whether the accumulation should be additive or not.\n"
			 ":type time: bool\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_accumulate(Sound* self, PyObject* args)
{
	bool additive = false;
	PyObject* additiveo;

	if(!PyArg_ParseTuple(args, "|O:accumulate", &additiveo))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		if(additiveo != nullptr)
		{
			if(!PyBool_Check(additiveo))
			{
				PyErr_SetString(PyExc_TypeError, "additive is not a boolean!");
				return nullptr;
			}

			additive = additiveo == Py_True;
		}

		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Accumulator(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), additive));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_ADSR_doc,
			 "ADSR(attack,decay,sustain,release)\n\n"
			 "Attack-Decay-Sustain-Release envelopes the volume of a sound. "
			 "Note: there is currently no way to trigger the release with this API.\n\n"
			 ":arg attack: The attack time in seconds.\n"
			 ":type attack: float\n"
			 ":arg decay: The decay time in seconds.\n"
			 ":type decay: float\n"
			 ":arg sustain: The sustain level.\n"
			 ":type sustain: float\n"
			 ":arg release: The release level.\n"
			 ":type release: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_ADSR(Sound* self, PyObject* args)
{
	float attack, decay, sustain, release;

	if(!PyArg_ParseTuple(args, "ffff:ADSR", &attack, &decay, &sustain, &release))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new ADSR(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), attack, decay, sustain, release));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_delay_doc,
			 "delay(time)\n\n"
			 "Delays by playing adding silence in front of the other sound's "
			 "data.\n\n"
			 ":arg time: How many seconds of silence should be added before "
			 "the sound.\n"
			 ":type time: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_delay(Sound* self, PyObject* args)
{
	float delay;

	if(!PyArg_ParseTuple(args, "f:delay", &delay))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Delay(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), delay));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_envelope_doc,
			 "envelope(attack, release, threshold, arthreshold)\n\n"
			 "Delays by playing adding silence in front of the other sound's "
			 "data.\n\n"
			 ":arg attack: The attack factor.\n"
			 ":type attack: float\n"
			 ":arg release: The release factor.\n"
			 ":type release: float\n"
			 ":arg threshold: The general threshold value.\n"
			 ":type threshold: float\n"
			 ":arg arthreshold: The attack/release threshold value.\n"
			 ":type arthreshold: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_envelope(Sound* self, PyObject* args)
{
	float attack, release, threshold, arthreshold;

	if(!PyArg_ParseTuple(args, "ffff:envelope", &attack, &release, &threshold, &arthreshold))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Envelope(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), attack, release, threshold, arthreshold));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_fadein_doc,
			 "fadein(start, length)\n\n"
			 "Fades a sound in by raising the volume linearly in the given "
			 "time interval.\n\n"
			 ":arg start: Time in seconds when the fading should start.\n"
			 ":type start: float\n"
			 ":arg length: Time in seconds how long the fading should last.\n"
			 ":type length: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: Before the fade starts it plays silence.");

static PyObject *
Sound_fadein(Sound* self, PyObject* args)
{
	float start, length;

	if(!PyArg_ParseTuple(args, "ff:fadein", &start, &length))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Fader(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), FADE_IN, start, length));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_fadeout_doc,
			 "fadeout(start, length)\n\n"
			 "Fades a sound in by lowering the volume linearly in the given "
			 "time interval.\n\n"
			 ":arg start: Time in seconds when the fading should start.\n"
			 ":type start: float\n"
			 ":arg length: Time in seconds how long the fading should last.\n"
			 ":type length: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: After the fade this sound plays silence, so that "
			 "the length of the sound is not altered.");

static PyObject *
Sound_fadeout(Sound* self, PyObject* args)
{
	float start, length;

	if(!PyArg_ParseTuple(args, "ff:fadeout", &start, &length))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Fader(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), FADE_OUT, start, length));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_filter_doc,
			 "filter(b, a = (1))\n\n"
			 "Filters a sound with the supplied IIR filter coefficients.\n"
			 "Without the second parameter you'll get a FIR filter.\n"
			 "If the first value of the a sequence is 0 it will be set to 1 "
			 "automatically.\n"
			 "If the first value of the a sequence is neither 0 nor 1, all "
			 "filter coefficients will be scaled by this value so that it is 1 "
			 "in the end, you don't have to scale yourself.\n\n"
			 ":arg b: The nominator filter coefficients.\n"
			 ":type b: sequence of float\n"
			 ":arg a: The denominator filter coefficients.\n"
			 ":type a: sequence of float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_filter(Sound* self, PyObject* args)
{
	PyObject* py_b;
	PyObject* py_a = nullptr;
	Py_ssize_t py_a_len;
	Py_ssize_t py_b_len;

	if(!PyArg_ParseTuple(args, "O|O:filter", &py_b, &py_a))
		return nullptr;

	if(!PySequence_Check(py_b) || (py_a != nullptr && !PySequence_Check(py_a)))
	{
		PyErr_SetString(PyExc_TypeError, "Parameter is not a sequence!");
		return nullptr;
	}

	py_a_len= py_a ? PySequence_Size(py_a) : 0;
	py_b_len= PySequence_Size(py_b);

	if(!py_b_len || ((py_a != nullptr) && !py_a_len))
	{
		PyErr_SetString(PyExc_ValueError, "The sequence has to contain at least one value!");
		return nullptr;
	}

	std::vector<float> a, b;
	PyObject* py_value;
	float value;

	for(Py_ssize_t i = 0; i < py_b_len; i++)
	{
		py_value = PySequence_GetItem(py_b, i);
		value= (float)PyFloat_AsDouble(py_value);
		Py_DECREF(py_value);

		if(value == -1.0f && PyErr_Occurred()) {
			return nullptr;
		}

		b.push_back(value);
	}

	if(py_a)
	{
		for(Py_ssize_t i = 0; i < py_a_len; i++)
		{
			py_value = PySequence_GetItem(py_a, i);
			value= (float)PyFloat_AsDouble(py_value);
			Py_DECREF(py_value);

			if(value == -1.0f && PyErr_Occurred()) {
				return nullptr;
			}

			a.push_back(value);
		}

		if(a[0] == 0)
			a[0] = 1;
	}
	else
		a.push_back(1);

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new IIRFilter(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), b, a));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_highpass_doc,
			 "highpass(frequency, Q=0.5)\n\n"
			 "Creates a second order highpass filter based on the transfer "
			 "function H(s) = s^2 / (s^2 + s/Q + 1)\n\n"
			 ":arg frequency: The cut off trequency of the highpass.\n"
			 ":type frequency: float\n"
			 ":arg Q: Q factor of the lowpass.\n"
			 ":type Q: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_highpass(Sound* self, PyObject* args)
{
	float frequency;
	float Q = 0.5;

	if(!PyArg_ParseTuple(args, "f|f:highpass", &frequency, &Q))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Highpass(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), frequency, Q));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_limit_doc,
			 "limit(start, end)\n\n"
			 "Limits a sound within a specific start and end time.\n\n"
			 ":arg start: Start time in seconds.\n"
			 ":type start: float\n"
			 ":arg end: End time in seconds.\n"
			 ":type end: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_limit(Sound* self, PyObject* args)
{
	float start, end;

	if(!PyArg_ParseTuple(args, "ff:limit", &start, &end))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Limiter(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), start, end));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_loop_doc,
			 "loop(count)\n\n"
			 "Loops a sound.\n\n"
			 ":arg count: How often the sound should be looped. "
			 "Negative values mean endlessly.\n"
			 ":type count: integer\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: This is a filter function, you might consider using "
			 ":attr:`Handle.loop_count` instead.");

static PyObject *
Sound_loop(Sound* self, PyObject* args)
{
	int loop;

	if(!PyArg_ParseTuple(args, "i:loop", &loop))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Loop(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), loop));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_lowpass_doc,
			 "lowpass(frequency, Q=0.5)\n\n"
			 "Creates a second order lowpass filter based on the transfer "
			 "function H(s) = 1 / (s^2 + s/Q + 1)\n\n"
			 ":arg frequency: The cut off trequency of the lowpass.\n"
			 ":type frequency: float\n"
			 ":arg Q: Q factor of the lowpass.\n"
			 ":type Q: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_lowpass(Sound* self, PyObject* args)
{
	float frequency;
	float Q = 0.5;

	if(!PyArg_ParseTuple(args, "f|f:lowpass", &frequency, &Q))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Lowpass(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), frequency, Q));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_modulate_doc,
			 "modulate(sound)\n\n"
			 "Modulates two factories.\n\n"
			 ":arg sound: The sound to modulate over the other.\n"
			 ":type sound: :class:`Sound`\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: The two factories have to have the same specifications "
			 "(channels and samplerate).");

static PyObject *
Sound_modulate(Sound* self, PyObject* object)
{
	PyTypeObject* type = Py_TYPE(self);

	if(!PyObject_TypeCheck(object, type))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Sound!");
		return nullptr;
	}

	Sound* parent = (Sound*)type->tp_alloc(type, 0);
	Sound* child = (Sound*)object;

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Modulator(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), *reinterpret_cast<std::shared_ptr<ISound>*>(child->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_pitch_doc,
			 "pitch(factor)\n\n"
			 "Changes the pitch of a sound with a specific factor.\n\n"
			 ":arg factor: The factor to change the pitch with.\n"
			 ":type factor: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: This is done by changing the sample rate of the "
			 "underlying sound, which has to be an integer, so the factor "
			 "value rounded and the factor may not be 100 % accurate.\n\n"
			 ".. note:: This is a filter function, you might consider using "
			 ":attr:`Handle.pitch` instead.");

static PyObject *
Sound_pitch(Sound* self, PyObject* args)
{
	float factor;

	if(!PyArg_ParseTuple(args, "f:pitch", &factor))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Pitch(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), factor));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_rechannel_doc,
			 "rechannel(channels)\n\n"
			 "Rechannels the sound.\n\n"
			 ":arg channels: The new channel configuration.\n"
			 ":type channels: int\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_rechannel(Sound* self, PyObject* args)
{
	int channels;

	if(!PyArg_ParseTuple(args, "i:rechannel", &channels))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			DeviceSpecs specs;
			specs.channels = static_cast<Channels>(channels);
			specs.rate = RATE_INVALID;
			specs.format = FORMAT_INVALID;
			parent->sound = new std::shared_ptr<ISound>(new ChannelMapper(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), specs));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_resample_doc,
			 "resample(rate, high_quality)\n\n"
			 "Resamples the sound.\n\n"
			 ":arg rate: The new sample rate.\n"
			 ":type rate: double\n"
			 ":arg high_quality: When true use a higher quality but slower resampler.\n"
			 ":type high_quality: bool\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_resample(Sound* self, PyObject* args)
{
	double rate;
	PyObject* high_qualityo;
	bool high_quality = false;

	if(!PyArg_ParseTuple(args, "d|O:resample", &rate, &high_qualityo))
		return nullptr;

	if(!PyBool_Check(high_qualityo))
	{
		PyErr_SetString(PyExc_TypeError, "high_quality is not a boolean!");
		return nullptr;
	}

	high_quality = high_qualityo == Py_True;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			DeviceSpecs specs;
			specs.channels = CHANNELS_INVALID;
			specs.rate = rate;
			specs.format = FORMAT_INVALID;
			if(high_quality)
				parent->sound = new std::shared_ptr<ISound>(new JOSResample(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), specs));
			else
				parent->sound = new std::shared_ptr<ISound>(new LinearResample(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), specs));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_reverse_doc,
			 "reverse()\n\n"
			 "Plays a sound reversed.\n\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: The sound has to have a finite length and has to be "
			 "seekable. It's recommended to use this only with factories	 with "
			 "fast and accurate seeking, which is not true for encoded audio "
			 "files, such ones should be buffered using :meth:`cache` before "
			 "being played reversed.\n\n"
			 ".. warning:: If seeking is not accurate in the underlying sound "
			 "you'll likely hear skips/jumps/cracks.");

static PyObject *
Sound_reverse(Sound* self)
{
	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Reverse(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_sum_doc,
			 "sum()\n\n"
			 "Sums the samples of a sound.\n\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_sum(Sound* self)
{
	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Sum(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_threshold_doc,
			 "threshold(threshold = 0)\n\n"
			 "Makes a threshold wave out of an audio wave by setting all samples "
			 "with a amplitude >= threshold to 1, all <= -threshold to -1 and "
			 "all between to 0.\n\n"
			 ":arg threshold: Threshold value over which an amplitude counts "
			 "non-zero.\n"
			 ":type threshold: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_threshold(Sound* self, PyObject* args)
{
	float threshold = 0;

	if(!PyArg_ParseTuple(args, "|f:threshold", &threshold))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Threshold(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), threshold));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_volume_doc,
			 "volume(volume)\n\n"
			 "Changes the volume of a sound.\n\n"
			 ":arg volume: The new volume..\n"
			 ":type volume: float\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: Should be in the range [0, 1] to avoid clipping.\n\n"
			 ".. note:: This is a filter function, you might consider using "
			 ":attr:`Handle.volume` instead.");

static PyObject *
Sound_volume(Sound* self, PyObject* args)
{
	float volume;

	if(!PyArg_ParseTuple(args, "f:volume", &volume))
		return nullptr;

	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Volume(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), volume));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_join_doc,
			 "join(sound)\n\n"
			 "Plays two factories in sequence.\n\n"
			 ":arg sound: The sound to play second.\n"
			 ":type sound: :class:`Sound`\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: The two factories have to have the same specifications "
			 "(channels and samplerate).");

static PyObject *
Sound_join(Sound* self, PyObject* object)
{
	PyTypeObject* type = Py_TYPE(self);

	if(!PyObject_TypeCheck(object, type))
	{
		PyErr_SetString(PyExc_TypeError, "Object has to be of type Sound!");
		return nullptr;
	}

	Sound* parent;
	Sound* child = (Sound*)object;

	parent = (Sound*)type->tp_alloc(type, 0);
	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Double(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), *reinterpret_cast<std::shared_ptr<ISound>*>(child->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_mix_doc,
			 "mix(sound)\n\n"
			 "Mixes two factories.\n\n"
			 ":arg sound: The sound to mix over the other.\n"
			 ":type sound: :class:`Sound`\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`\n\n"
			 ".. note:: The two factories have to have the same specifications "
			 "(channels and samplerate).");

static PyObject *
Sound_mix(Sound* self, PyObject* object)
{
	PyTypeObject* type = Py_TYPE(self);

	if(!PyObject_TypeCheck(object, type))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Sound!");
		return nullptr;
	}

	Sound* parent = (Sound*)type->tp_alloc(type, 0);
	Sound* child = (Sound*)object;

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new Superpose(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), *reinterpret_cast<std::shared_ptr<ISound>*>(child->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_pingpong_doc,
			 "pingpong()\n\n"
			 "Plays a sound forward and then backward.\n"
			 "This is like joining a sound with its reverse.\n\n"
			 ":return: The created :class:`Sound` object.\n"
			 ":rtype: :class:`Sound`");

static PyObject *
Sound_pingpong(Sound* self)
{
	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new PingPong(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_list_doc,
	"list()\n\n"
	"Creates an empty sound list that can contain several sounds.\n\n"
	":arg random: wether the playback will be random or not.\n"
	":type random: int\n"
	":return: The created :class:`Sound` object.\n"
	":rtype: :class:`Sound`");

static PyObject *
Sound_list(PyTypeObject* type, PyObject* args)
{
	int random;

	if(!PyArg_ParseTuple(args, "i:random", &random))
		return nullptr;

	Sound* self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		try
		{
			self->sound = new std::shared_ptr<ISound>(new SoundList(random));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sound_mutable_doc,
	"mutable()\n\n"
	"Creates a sound that will be restarted when sought backwards.\n"
	"If the original sound is a sound list, the playing sound can change.\n\n"
	":return: The created :class:`Sound` object.\n"
	":rtype: :class:`Sound`");

static PyObject *
Sound_mutable(Sound* self)
{
	PyTypeObject* type = Py_TYPE(self);
	Sound* parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new MutableSound(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_list_addSound_doc,
	"addSound(sound)\n\n"
	"Adds a new sound to a sound list.\n\n"
	":arg sound: The sound that will be added to the list.\n"
	":type sound: :class:`Sound`\n\n"
	".. note:: You can only add a sound to a sound list.");
	
static PyObject *
Sound_list_addSound(Sound* self, PyObject* object)
{
	PyTypeObject* type = Py_TYPE(self);

	if(!PyObject_TypeCheck(object, type))
	{
		PyErr_SetString(PyExc_TypeError, "Object has to be of type Sound!");
		return nullptr;
	}

	Sound* child = (Sound*)object;
	try
	{
		(*reinterpret_cast<std::shared_ptr<SoundList>*>(self->sound))->addSound(*reinterpret_cast<std::shared_ptr<ISound>*>(child->sound));
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

#ifdef WITH_CONVOLUTION

PyDoc_STRVAR(M_aud_Sound_convolver_doc,
	"convolver()\n\n"
	"Creates a sound that will apply convolution to another sound.\n\n"
	":arg impulseResponse: The filter with which convolve the sound.\n"
	":type impulseResponse: :class:`ImpulseResponse`\n"
	":arg threadPool: A thread pool used to parallelize convolution.\n"
	":type threadPool: :class:`ThreadPool`\n"
	":return: The created :class:`Sound` object.\n"
	":rtype: :class:`Sound`");

static PyObject *
Sound_convolver(Sound* self, PyObject* args)
{
	PyTypeObject* type = Py_TYPE(self);

	PyObject* object1;
	PyObject* object2;

	if(!PyArg_ParseTuple(args, "OO:convolver", &object1, &object2))
		return nullptr;

	ImpulseResponseP* filter = checkImpulseResponse(object1);
	if(!filter)
		return nullptr;

	ThreadPoolP* threadPool = checkThreadPool(object2);
	if(!threadPool)
		return nullptr;

	Sound* parent;
	parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new ConvolverSound(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), *reinterpret_cast<std::shared_ptr<ImpulseResponse>*>(filter->impulseResponse), *reinterpret_cast<std::shared_ptr<ThreadPool>*>(threadPool->threadPool)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

PyDoc_STRVAR(M_aud_Sound_binaural_doc,
	"convolver()\n\n"
	"Creates a binaural sound using another sound as source. The original sound must be mono\n\n"
	":arg hrtfs: An HRTF set.\n"
	":type hrtf: :class:`HRTF`\n"
	":arg source: An object representing the source position of the sound.\n"
	":type source: :class:`Source`\n"
	":arg threadPool: A thread pool used to parallelize convolution.\n"
	":type threadPool: :class:`ThreadPool`\n"
	":return: The created :class:`Sound` object.\n"
	":rtype: :class:`Sound`");

static PyObject *
Sound_binaural(Sound* self, PyObject* args)
{
	PyTypeObject* type = Py_TYPE(self);

	PyObject* object1;
	PyObject* object2;
	PyObject* object3;

	if(!PyArg_ParseTuple(args, "OOO:binaural", &object1, &object2, &object3))
		return nullptr;

	HRTFP* hrtfs = checkHRTF(object1);
	if(!hrtfs)
		return nullptr;

	SourceP* source = checkSource(object2);
	if(!hrtfs)
		return nullptr;

	ThreadPoolP* threadPool = checkThreadPool(object3);
	if(!threadPool)
		return nullptr;

	Sound* parent;
	parent = (Sound*)type->tp_alloc(type, 0);

	if(parent != nullptr)
	{
		try
		{
			parent->sound = new std::shared_ptr<ISound>(new BinauralSound(*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound), *reinterpret_cast<std::shared_ptr<HRTF>*>(hrtfs->hrtf), *reinterpret_cast<std::shared_ptr<Source>*>(source->source), *reinterpret_cast<std::shared_ptr<ThreadPool>*>(threadPool->threadPool)));
		}
		catch(Exception& e)
		{
			Py_DECREF(parent);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)parent;
}

#endif

static PyMethodDef Sound_methods[] = {
	{"data", (PyCFunction)Sound_data, METH_NOARGS,
	 M_aud_Sound_data_doc
	},
	{"write", (PyCFunction)Sound_write, METH_VARARGS | METH_KEYWORDS,
	 M_aud_Sound_write_doc
	},
	{"buffer", (PyCFunction)Sound_buffer, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_buffer_doc
	},
	{"cache", (PyCFunction)Sound_cache, METH_NOARGS,
	 M_aud_Sound_cache_doc
	},
	{"file", (PyCFunction)Sound_file, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_file_doc
	},
	{"sawtooth", (PyCFunction)Sound_sawtooth, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_sawtooth_doc
	},
	{"silence", (PyCFunction)Sound_silence, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_silence_doc
	},
	{"sine", (PyCFunction)Sound_sine, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_sine_doc
	},
	{"square", (PyCFunction)Sound_square, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_square_doc
	},
	{"triangle", (PyCFunction)Sound_triangle, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_triangle_doc
	},
	{"accumulate", (PyCFunction)Sound_accumulate, METH_VARARGS,
	 M_aud_Sound_accumulate_doc
	},
	{"ADSR", (PyCFunction)Sound_ADSR, METH_VARARGS,
	 M_aud_Sound_ADSR_doc
	},
	{"delay", (PyCFunction)Sound_delay, METH_VARARGS,
	 M_aud_Sound_delay_doc
	},
	{"envelope", (PyCFunction)Sound_envelope, METH_VARARGS,
	 M_aud_Sound_envelope_doc
	},
	{"fadein", (PyCFunction)Sound_fadein, METH_VARARGS,
	 M_aud_Sound_fadein_doc
	},
	{"fadeout", (PyCFunction)Sound_fadeout, METH_VARARGS,
	 M_aud_Sound_fadeout_doc
	},
	{"filter", (PyCFunction)Sound_filter, METH_VARARGS,
	 M_aud_Sound_filter_doc
	},
	{"highpass", (PyCFunction)Sound_highpass, METH_VARARGS,
	 M_aud_Sound_highpass_doc
	},
	{"limit", (PyCFunction)Sound_limit, METH_VARARGS,
	 M_aud_Sound_limit_doc
	},
	{"loop", (PyCFunction)Sound_loop, METH_VARARGS,
	 M_aud_Sound_loop_doc
	},
	{"lowpass", (PyCFunction)Sound_lowpass, METH_VARARGS,
	 M_aud_Sound_lowpass_doc
	},
	{"modulate", (PyCFunction)Sound_modulate, METH_O,
	 M_aud_Sound_modulate_doc
	},
	{"pitch", (PyCFunction)Sound_pitch, METH_VARARGS,
	 M_aud_Sound_pitch_doc
	},
	{"rechannel", (PyCFunction)Sound_rechannel, METH_VARARGS,
	 M_aud_Sound_rechannel_doc
	},
	{"resample", (PyCFunction)Sound_resample, METH_VARARGS,
	 M_aud_Sound_resample_doc
	},
	{"reverse", (PyCFunction)Sound_reverse, METH_NOARGS,
	 M_aud_Sound_reverse_doc
	},
	{"sum", (PyCFunction)Sound_sum, METH_NOARGS,
	 M_aud_Sound_sum_doc
	},
	{"threshold", (PyCFunction)Sound_threshold, METH_VARARGS,
	 M_aud_Sound_threshold_doc
	},
	{"volume", (PyCFunction)Sound_volume, METH_VARARGS,
	 M_aud_Sound_volume_doc
	},
	{"join", (PyCFunction)Sound_join, METH_O,
	 M_aud_Sound_join_doc
	},
	{"mix", (PyCFunction)Sound_mix, METH_O,
	 M_aud_Sound_mix_doc
	},
	{ "pingpong", (PyCFunction)Sound_pingpong, METH_NOARGS,
	 M_aud_Sound_pingpong_doc
	},
	{ "list", (PyCFunction)Sound_list, METH_VARARGS | METH_CLASS,
	 M_aud_Sound_list_doc
	},
	{ "mutable", (PyCFunction)Sound_mutable, METH_NOARGS,
	 M_aud_Sound_mutable_doc
	},
	{ "addSound", (PyCFunction)Sound_list_addSound, METH_O,
	M_aud_Sound_list_addSound_doc
	},
#ifdef WITH_CONVOLUTION
	{ "convolver", (PyCFunction)Sound_convolver, METH_VARARGS,
	M_aud_Sound_convolver_doc
	},
	{ "binaural", (PyCFunction)Sound_binaural, METH_VARARGS,
	M_aud_Sound_binaural_doc
	},
#endif
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Sound_specs_doc,
			 "The sample specification of the sound as a tuple with rate and channel count.");

static PyObject *
Sound_get_specs(Sound* self, void* nothing)
{
	try
	{
		Specs specs = (*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound))->createReader()->getSpecs();
		return Py_BuildValue("(di)", specs.rate, specs.channels);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Sound_length_doc,
			 "The sample specification of the sound as a tuple with rate and channel count.");

static PyObject *
Sound_get_length(Sound* self, void* nothing)
{
	try
	{
		int length = (*reinterpret_cast<std::shared_ptr<ISound>*>(self->sound))->createReader()->getLength();
		return Py_BuildValue("i", length);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyGetSetDef Sound_properties[] = {
	{(char*)"specs", (getter)Sound_get_specs, nullptr,
	 M_aud_Sound_specs_doc, nullptr },
	{(char*)"length", (getter)Sound_get_length, nullptr,
	 M_aud_Sound_length_doc, nullptr },
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Sound_doc,
			 "Sound objects are immutable and represent a sound that can be "
			 "played simultaneously multiple times. They are called factories "
			 "because they create reader objects internally that are used for "
			 "playback.");

PyTypeObject SoundType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.Sound",               /* tp_name */
	sizeof(Sound),             /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor)Sound_dealloc, /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_reserved */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash  */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,        /* tp_flags */
	M_aud_Sound_doc,           /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	Sound_methods,             /* tp_methods */
	0,                         /* tp_members */
	Sound_properties,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,                         /* tp_init */
	0,                         /* tp_alloc */
	Sound_new,                 /* tp_new */
};

AUD_API PyObject* Sound_empty()
{
	return SoundType.tp_alloc(&SoundType, 0);
}

AUD_API Sound* checkSound(PyObject* sound)
{
	if(!PyObject_TypeCheck(sound, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Sound!");
		return nullptr;
	}

	return (Sound*)sound;
}


bool initializeSound()
{
	import_array();

	return PyType_Ready(&SoundType) >= 0;
}


void addSoundToModule(PyObject* module)
{
	Py_INCREF(&SoundType);
	PyModule_AddObject(module, "Sound", (PyObject *)&SoundType);
}
