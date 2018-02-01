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

#include "PyAPI.h"
#include "PySound.h"
#include "PyHandle.h"
#include "PyDevice.h"
#include "PySequenceEntry.h"
#include "PySequence.h"
#include "PyPlaybackManager.h"
#include "PyDynamicMusic.h"
#include "PyThreadPool.h"
#include "PySource.h"

#ifdef WITH_CONVOLUTION
#include "PyImpulseResponse.h"
#include "PyHRTF.h"
#endif

#include "respec/Specification.h"
#include "devices/IHandle.h"
#include "devices/I3DDevice.h"
#include "file/IWriter.h"
#include "plugin/PluginManager.h"
#include "sequence/AnimateableProperty.h"
#include "ISound.h"

#include <memory>

#include <structmember.h>

using namespace aud;

// ====================================================================

#define PY_MODULE_ADD_CONSTANT(module, name) PyModule_AddIntConstant(module, #name, name)

// ====================================================================

extern PyObject* AUDError;
PyObject* AUDError = nullptr;

// ====================================================================

PyDoc_STRVAR(M_aud_doc,
			 "Audaspace (pronounced \"outer space\") is a high level audio library.");

static struct PyModuleDef audmodule = {
	PyModuleDef_HEAD_INIT,
	"aud",     /* name of module */
	M_aud_doc, /* module documentation */
	-1,        /* size of per-interpreter state of the module,
				  or -1 if the module keeps state in global variables. */
   nullptr, nullptr, nullptr, nullptr, nullptr
};

PyMODINIT_FUNC
PyInit_aud()
{
	PyObject* module;

	PluginManager::loadPlugins();

	if(!initializeSound())
		return nullptr;

	if(!initializeDevice())
		return nullptr;

	if(!initializeHandle())
		return nullptr;

	if(!initializeSequenceEntry())
		return nullptr;

	if(!initializeSequence())
		return nullptr;

	if(!initializeDynamicMusic())
		return nullptr;

	if(!initializePlaybackManager())
		return nullptr;

	if(!initializeThreadPool())
		return nullptr;

	if(!initializeSource())
		return nullptr;

#ifdef WITH_CONVOLUTION
	if(!initializeImpulseResponse())
		return nullptr;

	if(!initializeHRTF())
		return nullptr;
#endif

	module = PyModule_Create(&audmodule);
	if(module == nullptr)
		return nullptr;

	addSoundToModule(module);
	addHandleToModule(module);
	addDeviceToModule(module);
	addSequenceEntryToModule(module);
	addSequenceToModule(module);
	addDynamicMusicToModule(module);
	addPlaybackManagerToModule(module);
	addThreadPoolToModule(module);
	addSourceToModule(module);

#ifdef WITH_CONVOLUTION
	addImpulseResponseToModule(module);
	addHRTFToModule(module);
#endif

	AUDError = PyErr_NewException("aud.error", nullptr, nullptr);
	Py_INCREF(AUDError);
	PyModule_AddObject(module, "error", AUDError);

	// animatable property type constants
	PY_MODULE_ADD_CONSTANT(module, AP_VOLUME);
	PY_MODULE_ADD_CONSTANT(module, AP_PANNING);
	PY_MODULE_ADD_CONSTANT(module, AP_PITCH);
	PY_MODULE_ADD_CONSTANT(module, AP_LOCATION);
	PY_MODULE_ADD_CONSTANT(module, AP_ORIENTATION);
	// channels constants
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_INVALID);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_MONO);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_STEREO);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_STEREO_LFE);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_SURROUND4);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_SURROUND5);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_SURROUND51);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_SURROUND61);
	PY_MODULE_ADD_CONSTANT(module, CHANNELS_SURROUND71);
	// codec constants
	PY_MODULE_ADD_CONSTANT(module, CODEC_INVALID);
	PY_MODULE_ADD_CONSTANT(module, CODEC_AAC);
	PY_MODULE_ADD_CONSTANT(module, CODEC_AC3);
	PY_MODULE_ADD_CONSTANT(module, CODEC_FLAC);
	PY_MODULE_ADD_CONSTANT(module, CODEC_MP2);
	PY_MODULE_ADD_CONSTANT(module, CODEC_MP3);
	PY_MODULE_ADD_CONSTANT(module, CODEC_PCM);
	PY_MODULE_ADD_CONSTANT(module, CODEC_VORBIS);
	PY_MODULE_ADD_CONSTANT(module, CODEC_OPUS);
	// container constants
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_INVALID);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_AC3);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_FLAC);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_MATROSKA);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_MP2);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_MP3);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_OGG);
	PY_MODULE_ADD_CONSTANT(module, CONTAINER_WAV);
	// distance model constants
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_EXPONENT);
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_EXPONENT_CLAMPED);
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_INVERSE);
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_INVERSE_CLAMPED);
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_LINEAR);
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_LINEAR_CLAMPED);
	PY_MODULE_ADD_CONSTANT(module, DISTANCE_MODEL_INVALID);
	// format constants
	PY_MODULE_ADD_CONSTANT(module, FORMAT_INVALID);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_FLOAT32);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_FLOAT64);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_INVALID);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_S16);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_S24);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_S32);
	PY_MODULE_ADD_CONSTANT(module, FORMAT_U8);
	// rate constants
	PY_MODULE_ADD_CONSTANT(module, RATE_INVALID);
	PY_MODULE_ADD_CONSTANT(module, RATE_8000);
	PY_MODULE_ADD_CONSTANT(module, RATE_16000);
	PY_MODULE_ADD_CONSTANT(module, RATE_11025);
	PY_MODULE_ADD_CONSTANT(module, RATE_22050);
	PY_MODULE_ADD_CONSTANT(module, RATE_32000);
	PY_MODULE_ADD_CONSTANT(module, RATE_44100);
	PY_MODULE_ADD_CONSTANT(module, RATE_48000);
	PY_MODULE_ADD_CONSTANT(module, RATE_88200);
	PY_MODULE_ADD_CONSTANT(module, RATE_96000);
	PY_MODULE_ADD_CONSTANT(module, RATE_192000);
	// status constants
	PY_MODULE_ADD_CONSTANT(module, STATUS_INVALID);
	PY_MODULE_ADD_CONSTANT(module, STATUS_PAUSED);
	PY_MODULE_ADD_CONSTANT(module, STATUS_PLAYING);
	PY_MODULE_ADD_CONSTANT(module, STATUS_STOPPED);

	return module;
}

AUD_API PyObject* AUD_getPythonSound(void* sound)
{
	if(sound)
	{
		Sound* object = (Sound*) Sound_empty();
		if(object)
		{
			object->sound = new std::shared_ptr<ISound>(*reinterpret_cast<std::shared_ptr<ISound>*>(sound));
			return (PyObject *) object;
		}
	}

	return nullptr;
}

AUD_API void* AUD_getSoundFromPython(PyObject* object)
{
	Sound* sound = checkSound(object);

	if(!sound)
		return nullptr;

	return new std::shared_ptr<ISound>(*reinterpret_cast<std::shared_ptr<ISound>*>(sound->sound));
}
