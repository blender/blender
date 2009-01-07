/*
 * SND_C-Api.cpp
 *
 * C Api for soundmodule
 *
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SND_C-api.h"
#include "SND_DeviceManager.h"
#include "SND_Scene.h"

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32



void SND_SetDeviceType(int device_type)
{
	SND_DeviceManager::SetDeviceType(device_type);
}



SND_AudioDeviceInterfaceHandle SND_GetAudioDevice()
{
	SND_IAudioDevice* audiodevice = NULL;

	SND_DeviceManager::Subscribe();
	audiodevice = SND_DeviceManager::Instance();

	if (!audiodevice->IsInitialized())
	{
		SND_DeviceManager::SetDeviceType(snd_e_dummydevice);
		audiodevice = SND_DeviceManager::Instance();
	}

	return (SND_AudioDeviceInterfaceHandle)audiodevice;
}



void SND_ReleaseDevice()
{
	SND_DeviceManager::Unsubscribe();
}



int SND_IsPlaybackWanted(SND_SceneHandle scene)
{
	assert(scene);
	bool result = ((SND_Scene*)scene)->IsPlaybackWanted();

	return (int)result;
}



// create a scene
SND_SceneHandle SND_CreateScene(SND_AudioDeviceInterfaceHandle audiodevice)
{
	// initialize sound scene and object
	SND_Scene* scene = new SND_Scene((SND_IAudioDevice*)audiodevice);

	return (SND_SceneHandle)scene; 
}



void SND_DeleteScene(SND_SceneHandle scene)
{
    assert(scene);
    delete (SND_Scene*)scene;
}



int SND_AddSample(SND_SceneHandle scene,
				  const char* filename,
				  void* memlocation,
				  int size)
{
	assert(scene);
	assert(memlocation);
	int buffer = ((SND_Scene*)scene)->LoadSample(filename, memlocation, size);
	
	return buffer;
}



void SND_RemoveAllSamples(SND_SceneHandle scene)
{
	assert(scene);
	((SND_Scene*)scene)->RemoveAllSamples();
}



int SND_CheckBuffer(SND_SceneHandle scene, SND_ObjectHandle object)
{
	assert(scene);
	assert(object);
	int result = (int)((SND_Scene*)scene)->CheckBuffer((SND_SoundObject*)object);

	return result;
}



void SND_AddSound(SND_SceneHandle scene, SND_ObjectHandle object)
{
    assert(scene);
    assert(object);
    ((SND_Scene*)scene)->AddObject((SND_SoundObject *)object);
}



void SND_RemoveSound(SND_SceneHandle scene, SND_ObjectHandle object)
{
    assert(scene);
    assert(object);
    ((SND_Scene*)scene)->DeleteObject((SND_SoundObject *)object);
}



void SND_RemoveAllSounds(SND_SceneHandle scene)
{
	assert(scene);
    ((SND_Scene*)scene)->RemoveAllObjects();	
}



void SND_StopAllSounds(SND_SceneHandle scene)
{
	assert(scene);
	((SND_Scene*)scene)->StopAllObjects();
}



void SND_Proceed(SND_AudioDeviceInterfaceHandle audiodevice, SND_SceneHandle scene)
{
	assert(scene);
	((SND_Scene*)scene)->Proceed();
	((SND_IAudioDevice*)audiodevice)->NextFrame();
}



SND_ListenerHandle SND_GetListener(SND_SceneHandle scene)
{
	assert(scene);
	return (SND_ListenerHandle)((SND_Scene*)scene)->GetListener();
}



void SND_SetListenerGain(SND_SceneHandle scene, double gain)
{
	assert(scene);
	SND_SoundListener* listener = ((SND_Scene*)scene)->GetListener();
	listener->SetGain((MT_Scalar)gain);
}



void SND_SetDopplerFactor(SND_SceneHandle scene, double dopplerfactor)
{
	assert(scene);
	SND_SoundListener* listener = ((SND_Scene*)scene)->GetListener();
	listener->SetDopplerFactor(dopplerfactor);
}



void SND_SetDopplerVelocity(SND_SceneHandle scene, double dopplervelocity)
{
	assert(scene);
	SND_SoundListener* listener = ((SND_Scene*)scene)->GetListener();
	listener->SetDopplerVelocity(dopplervelocity);
}



// Object instantiation
SND_ObjectHandle SND_CreateSound()
{
	return (SND_ObjectHandle)new SND_SoundObject();
}



void SND_DeleteSound(SND_ObjectHandle object)
{
	assert(object);
    delete (SND_SoundObject*)object;
}



// Object control
void SND_StartSound(SND_SceneHandle scene, SND_ObjectHandle object)
{
	assert(scene);
	assert(object);
	((SND_Scene*)scene)->AddActiveObject((SND_SoundObject*)object, 0);
}



void SND_StopSound(SND_SceneHandle scene, SND_ObjectHandle object)
{
	assert(scene);
	assert(object);
	((SND_Scene*)scene)->RemoveActiveObject((SND_SoundObject*)object);
}



void SND_PauseSound(SND_SceneHandle scene, SND_ObjectHandle object)
{
	assert(scene);
	assert(object);
	((SND_Scene*)scene)->RemoveActiveObject((SND_SoundObject*)object);
}



void SND_SetSampleName(SND_ObjectHandle object, char* samplename)
{
	assert(object);
	STR_String name = samplename;
	((SND_SoundObject*)object)->SetSampleName(name);
}



void SND_SetGain(SND_ObjectHandle object, double gain)
{
	assert(object);
	((SND_SoundObject*)object)->SetGain(gain);
}



void SND_SetMinimumGain(SND_ObjectHandle object, double minimumgain)
{
	assert(object);
	((SND_SoundObject*)object)->SetMinGain(minimumgain);
}



void SND_SetMaximumGain(SND_ObjectHandle object, double maximumgain)
{
	assert(object);
	((SND_SoundObject*)object)->SetMaxGain(maximumgain);
}



void SND_SetRollOffFactor(SND_ObjectHandle object, double rollofffactor)
{
	assert(object);
	((SND_SoundObject*)object)->SetRollOffFactor(rollofffactor);
}



void SND_SetReferenceDistance(SND_ObjectHandle object, double referencedistance)
{
	assert(object);
	((SND_SoundObject*)object)->SetReferenceDistance(referencedistance);
}



void SND_SetPitch(SND_ObjectHandle object, double pitch)
{
	assert(object);
	((SND_SoundObject*)object)->SetPitch(pitch);
}



void SND_SetPosition(SND_ObjectHandle object, double* position) 
{
	assert(object);
	((SND_SoundObject*)object)->SetPosition(position);
}



void SND_SetVelocity(SND_ObjectHandle object, double* velocity)
{
	assert(object);
	((SND_SoundObject*)object)->SetVelocity(velocity);
}



void SND_SetOrientation(SND_ObjectHandle object, double* orientation)
{
	assert(object);
	((SND_SoundObject*)object)->SetOrientation(orientation);
}



void SND_SetLoopMode(SND_ObjectHandle object, int loopmode)
{
	assert(object);
	((SND_SoundObject*)object)->SetLoopMode(loopmode);
}



void SND_SetLoopPoints(SND_ObjectHandle object, unsigned int loopstart, unsigned int loopend)
{
	assert(object);
	((SND_SoundObject*)object)->SetLoopStart(loopstart);
	((SND_SoundObject*)object)->SetLoopEnd(loopend);
}



float SND_GetGain(SND_ObjectHandle object)
{
	assert(object);
	MT_Scalar gain = ((SND_SoundObject*)object)->GetGain();
	return (float) gain;
}



float SND_GetPitch(SND_ObjectHandle object) 
{
	assert(object);
	MT_Scalar pitch = ((SND_SoundObject*)object)->GetPitch();
	return (float) pitch;
}



int SND_GetLoopMode(SND_ObjectHandle object) 
{
	assert(object);
	return ((SND_SoundObject*)object)->GetLoopMode();
}



int SND_GetPlaystate(SND_ObjectHandle object) 
{
	assert(object);
	return ((SND_SoundObject*)object)->GetPlaystate();
}
