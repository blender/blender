/*
* SND_Scene.cpp
*
* The scene for sounds.
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

#ifdef WIN32
#pragma warning (disable:4786) // Get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "SND_Scene.h"
#include "SND_DependKludge.h"
#include "SND_IAudioDevice.h"

#include <stdlib.h>
#include <iostream>

//static unsigned int tijd = 0;

SND_Scene::SND_Scene(SND_IAudioDevice* audiodevice)
					 : m_audiodevice(audiodevice)
{
	if (m_audiodevice)
		m_wavecache = m_audiodevice->GetWaveCache();

	if (!m_wavecache || !audiodevice)
	{
		m_audio = false;
	}
	else
	{
		//if so, go ahead!
		m_audio = true;
#ifdef ONTKEVER
		printf("SND_Scene::SND_Scene() m_audio == true\n");
#endif
		m_audiodevice->InitListener();
	}

	IsPlaybackWanted();
}



SND_Scene::~SND_Scene()
{
	StopAllObjects();
}



// check if audioplayback is wanted
bool SND_Scene::IsPlaybackWanted()
{
    /* Removed the functionality for checking if noaudio was provided on */
    /* the commandline. */
	if (m_audiodevice && m_wavecache)
	{
		m_audioplayback = true;
	}
	else
	{
		StopAllObjects();
		m_audioplayback = false;
	}

	return m_audioplayback;
}



int SND_Scene::LoadSample(const STR_String& samplename,
						   void* memlocation,
						   int size)
{
	int result = -1;

	if (m_audiodevice)
	{
		SND_WaveSlot* waveslot = m_audiodevice->LoadSample(samplename, memlocation, size);

		if (waveslot)
			result = waveslot->GetBuffer();
	}

	return result;
}



void SND_Scene::RemoveAllSamples()
{
	if (m_audio && m_audiodevice)
		m_audiodevice->RemoveAllSamples();
}



bool SND_Scene::CheckBuffer(SND_SoundObject* pObject)
{
	bool result = false;

	if (pObject && m_wavecache)
	{
		SND_WaveSlot* waveslot = m_wavecache->GetWaveSlot(pObject->GetSampleName());
		
		if (waveslot)
		{
			pObject->SetBuffer(waveslot->GetBuffer());

			result = true;
		}
	}

	return result;
}



bool SND_Scene::IsSampleLoaded(STR_String& samplename)
{
	bool result = false;

	if (samplename && m_wavecache)
	{
		SND_WaveSlot* waveslot = m_wavecache->GetWaveSlot(samplename);

		if (waveslot && waveslot->IsLoaded())
			result = true;
	}

	return result;
}



void SND_Scene::AddObject(SND_SoundObject* pObject)
{
	if (m_audio)
	{
		STR_String samplename = pObject->GetSampleName();
		SND_WaveSlot* slot = NULL;
		
		// don't add the object if no valid sample is referenced
		if (samplename != "")
		{	
			// check if the sample is already loaded
			slot = m_wavecache->GetWaveSlot(samplename);
		}
		
		if (slot)
		{
			pObject->SetBuffer(slot->GetBuffer());
			
			// needed for expected lifespan of the sample, but ain't necesary anymore i think
			MT_Scalar samplelength = slot->GetNumberOfSamples();
			MT_Scalar samplerate = slot->GetSampleRate();
			MT_Scalar soundlength = samplelength/samplerate;
			pObject->SetLength(soundlength);
			
			// add the object to the list
			m_soundobjects.insert((SND_SoundObject*)pObject);
		}
	}
}



void SND_Scene::SetListenerTransform(const MT_Vector3& pos,
									 const MT_Vector3& vel,
									 const MT_Matrix3x3& ori)
{
	if (m_audio)
	{
		GetListener()->SetPosition(pos);
		GetListener()->SetVelocity(vel);
		GetListener()->SetOrientation(ori);
	}
}



void SND_Scene::UpdateListener()
{
	// process the listener if modified
	if (m_listener.IsModified())
	{
		m_audiodevice->SetListenerGain(m_listener.GetGain());
		
		// fmod doesn't support dopplervelocity, so just use the dopplerfactor instead
#ifdef USE_FMOD
		m_audiodevice->SetDopplerFactor(m_listener.GetDopplerVelocity());
#else
		m_audiodevice->SetDopplerVelocity(m_listener.GetDopplerVelocity());
		m_audiodevice->SetDopplerFactor(m_listener.GetDopplerFactor());
#endif
		m_listener.SetModified(false);
	}
}



void SND_Scene::AddActiveObject(SND_SoundObject* pObject, MT_Scalar curtime)
{
	if (m_audio)
	{
		if (pObject)
		{
#ifdef ONTKEVER
			printf("SND_Scene::AddActiveObject\n");
#endif
			
			// first check if the object is already on the list
			if (pObject->IsActive())
			{
				pObject->SetTimeStamp(curtime);
				pObject->StartSound();
			}
			else
			{	
				pObject->SetTimeStamp(curtime);
				
				// compute the expected lifespan
				pObject->SetLifeSpan();
				
				// lets give the new active-to-be object an id
				if (m_audiodevice->GetNewId(pObject))
				{
					// and add the object
					m_activeobjects.addTail(pObject);
					pObject->StartSound();
					pObject->SetActive(true);
				}
			}
		}
	}
}



void SND_Scene::RemoveActiveObject(SND_SoundObject* pObject)
{
	if (m_audio)
	{
		if (pObject)
		{
#ifdef ONTKEVER
			printf("SND_Scene::RemoveActiveObject\n");
#endif
			// if inactive, remove it from the list
			if (pObject->IsActive())
			{	
				// first make sure it is stopped
				m_audiodevice->ClearId(pObject);
			}
		}
	}
}



void SND_Scene::UpdateActiveObects()
{
//	++tijd;

	SND_SoundObject* pObject;
	// update only the objects that need to be updated
	for (pObject = (SND_SoundObject*)m_activeobjects.getHead();
					!pObject->isTail();
					pObject = (SND_SoundObject*)pObject->getNext())
	{
		int id = pObject->GetId();
		
		if (id >= 0)
		{
#ifdef USE_FMOD
			// fmod wants these set before playing the sample
			if (pObject->IsModified())
			{
				m_audiodevice->SetObjectLoop(id, pObject->GetLoopMode());
				m_audiodevice->SetObjectLoopPoints(id, pObject->GetLoopStart(), pObject->GetLoopEnd());
			}

			// ok, properties Set. now see if it must play
			if (pObject->GetPlaystate() == SND_MUST_PLAY)
			{
				m_audiodevice->PlayObject(id);
				pObject->SetPlaystate(SND_PLAYING);
				pObject->InitRunning();
//				printf("start play: %d\n", tijd);
			}
#endif
			if (pObject->Is3D())
			{
				// Get the global positions and velocity vectors
				// of the listener and soundobject
				MT_Vector3 op = pObject->GetPosition();
				MT_Vector3 lp = m_listener.GetPosition();
				MT_Vector3 position = op - lp;
				
				// Calculate relative velocity in global coordinates
				// of the sound with respect to the listener.
				MT_Vector3 ov = pObject->GetVelocity();
				MT_Vector3 lv = m_listener.GetVelocity();
				MT_Vector3 velocity = ov - lv;
				
				// Now map the object position and velocity into 
				// the local coordinates of the listener.
				MT_Matrix3x3 lo = m_listener.GetOrientation();
				
				MT_Vector3 local_sound_pos = position * lo;
				MT_Vector3 local_sound_vel = velocity * lo;
				
				m_audiodevice->SetObjectTransform(
					id,
					local_sound_pos,
					local_sound_vel,
					pObject->GetOrientation(), // make relative to listener! 
					lp,
					pObject->GetRollOffFactor());
			}
			else
			{
				m_audiodevice->ObjectIs2D(id);
			}
			
			// update the situation
			if (pObject->IsModified())
			{
				m_audiodevice->SetObjectPitch(id, pObject->GetPitch());
				m_audiodevice->SetObjectGain(id, pObject->GetGain());
				m_audiodevice->SetObjectMinGain(id, pObject->GetMinGain());
				m_audiodevice->SetObjectMaxGain(id, pObject->GetMaxGain());
				m_audiodevice->SetObjectReferenceDistance(id, pObject->GetReferenceDistance());
				m_audiodevice->SetObjectRollOffFactor(id, pObject->GetRollOffFactor());
				m_audiodevice->SetObjectLoop(id, pObject->GetLoopMode());
				m_audiodevice->SetObjectLoopPoints(id, pObject->GetLoopStart(), pObject->GetLoopEnd());
				pObject->SetModified(false);
			}

			pObject->AddRunning();

#ifdef ONTKEVER				
			STR_String naam = pObject->GetObjectName();
			STR_String sample = pObject->GetSampleName();
			
			int id = pObject->GetId();
			int buffer = pObject->GetBuffer();
			
			float gain = pObject->GetGain();
			float pitch = pObject->GetPitch();
			float timestamp = pObject->GetTimestamp();
			
			printf("naam: %s, sample: %s \n", naam.Ptr(), sample.Ptr());
			printf("id: %d, buffer: %d \n", id, buffer);
			printf("gain: %f, pitch: %f, ts: %f \n\n", gain, pitch, timestamp);
#endif
#ifdef USE_OPENAL
			// ok, properties Set. now see if it must play
			if (pObject->GetPlaystate() == SND_MUST_PLAY)
			{
				m_audiodevice->PlayObject(id);
				pObject->SetPlaystate(SND_PLAYING);
				//break;
			}
#endif

			// check to see if the sound is still playing
			// if not: release its id
			int playstate = m_audiodevice->GetPlayState(id);
#ifdef ONTKEVER
			if (playstate != 2)
				printf("%d - ",playstate);
#endif

			if ((playstate == SND_STOPPED) && !pObject->GetLoopMode())
			{
				RemoveActiveObject(pObject);
			}
		}
	}
}



void SND_Scene::UpdateCD()
{
	if (m_audiodevice)
	{
		SND_CDObject* pCD = SND_CDObject::Instance();

		if (pCD)
		{
			int playstate = pCD->GetPlaystate();
			
			switch (playstate)
			{
			case SND_MUST_PLAY:
				{
					// initialize the cd only when you need it
					m_audiodevice->SetCDGain(pCD->GetGain());
					m_audiodevice->SetCDPlaymode(pCD->GetPlaymode());
					m_audiodevice->PlayCD(pCD->GetTrack());
					pCD->SetPlaystate(SND_PLAYING);
					pCD->SetUsed();
					break;
				}
			case SND_MUST_PAUSE:
				{
					m_audiodevice->PauseCD(true);
					pCD->SetPlaystate(SND_PAUSED);
					break;
				}
			case SND_MUST_RESUME:
				{
					m_audiodevice->PauseCD(false);
					pCD->SetPlaystate(SND_PLAYING);
					break;
				}
			case SND_MUST_STOP:
				{
					m_audiodevice->StopCD();
					pCD->SetPlaystate(SND_STOPPED);
					break;
				}
			default:
				{
				}
			}
			
			// this one is only for realtime modifying settings
			if (pCD->IsModified())
			{
				m_audiodevice->SetCDGain(pCD->GetGain());
				pCD->SetModified(false);
			}
		}
	}
}



void SND_Scene::Proceed()
{
	if (m_audio && m_audioplayback)
	{
		m_audiodevice->MakeCurrent();

		UpdateListener();
		UpdateActiveObects();
		UpdateCD();

//		m_audiodevice->UpdateDevice();
	}
}


void SND_Scene::DeleteObject(SND_SoundObject* pObject) 
{
#ifdef ONTKEVER
	printf("SND_Scene::DeleteObject\n");
#endif
	
	if (pObject)
	{
		if (m_audiodevice)
			m_audiodevice->ClearId(pObject);
		
		// must remove object from m_activeList
		std::set<SND_SoundObject*>::iterator set_it;
		set_it = m_soundobjects.find(pObject);
		
		if (set_it != m_soundobjects.end())
			m_soundobjects.erase(set_it);
		
		// release the memory
		delete pObject;
		pObject = NULL;
	}
}



void SND_Scene::RemoveAllObjects()
{
#ifdef ONTKEVER
	printf("SND_Scene::RemoveAllObjects\n");
#endif

	StopAllObjects();

	std::set<SND_SoundObject*>::iterator it = m_soundobjects.begin();

	while (it != m_soundobjects.end())
	{
		delete (*it);
		it++;
	}

	m_soundobjects.clear();
}



void SND_Scene::StopAllObjects()
{
	if (m_audio)
	{
#ifdef ONTKEVER
		printf("SND_Scene::StopAllObjects\n");
#endif
		
		SND_SoundObject* pObject;
		
		for (pObject = (SND_SoundObject*)m_activeobjects.getHead();
		!pObject->isTail();
		pObject = (SND_SoundObject*)pObject->getNext())
		{
			m_audiodevice->ClearId(pObject);
		}
	}
}



SND_SoundListener* SND_Scene::GetListener()
{
	return &m_listener;
}
