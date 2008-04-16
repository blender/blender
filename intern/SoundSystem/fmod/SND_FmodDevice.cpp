/*
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
 * SND_FmodDevice derived from SND_IAudioDevice
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "SND_FmodDevice.h"
#include "SoundDefines.h"
#include "SND_Utils.h"

SND_FmodDevice::SND_FmodDevice()
{
    /* Removed the functionality for checking if noaudio was provided on */
    /* the commandline. */
	m_dspunit = NULL;

	m_audio = true;

	// let's check if we can get fmod to initialize...
	if (m_audio)
	{
		signed char MinHardwareChannels = FSOUND_SetMinHardwareChannels(NUM_FMOD_MIN_HW_CHANNELS);
		signed char MaxHardwareChannels = FSOUND_SetMaxHardwareChannels(NUM_FMOD_MAX_HW_CHANNELS);

		if (FSOUND_Init(MIXRATE, NUM_SOURCES, 0))
		{
			m_max_channels = FSOUND_GetMaxChannels();
			m_num_hardware_channels = FSOUND_GetNumHardwareChannels();
			m_num_software_channels = NUM_SOURCES;

			// let's get us a wavecache
			m_wavecache = new SND_WaveCache();
			
			int i;
			for (i = 0; i < NUM_BUFFERS; i++)
				m_buffers[i] = NULL;

			for (i = 0; i < NUM_SOURCES; i++)
			{
				m_sources[i] = NULL;
				m_frequencies[i] = 0;
				m_channels[i] = 0;
			}
		}
		else
		{
			m_audio = false;
		}
	}
	
#ifdef ONTKEVER
	int numdrivers = FSOUND_GetNumDrivers();
	int output = FSOUND_GetOutput();
	int oputputrate = FSOUND_GetOutputRate();
	int mixer = FSOUND_GetMixer();

	printf("maxchannels is: %d\n", m_max_channels);
	printf("num hw channels is: %d\n", m_num_hardware_channels);
	printf("num sw channels is: %d\n", m_num_software_channels);
	printf("numdrivers is: %d\n", numdrivers);
	printf("output is: %d\n", output);
	printf("oputputrate is: %d\n", oputputrate);
	printf("mixer is: %d\n", mixer);
#endif
}



SND_FmodDevice::~SND_FmodDevice()
{
	// let's see if we used the cd. if not, just leave it alone
	SND_CDObject* pCD = SND_CDObject::Instance();
	
	if (pCD)
	{
		this->StopCD();
		SND_CDObject::DisposeSystem();
	}

	StopUsingDSP();

	FSOUND_Close();
}



void SND_FmodDevice::UseCD() const
{
	// only fmod has CD support, so only create it here
	SND_CDObject::CreateSystem();
}



void SND_FmodDevice::MakeCurrent() const
{
	// empty
}



SND_WaveSlot* SND_FmodDevice::LoadSample(const STR_String& name,
										 void* memlocation,
										 int size)
{
	SND_WaveSlot* waveslot = NULL;
	STR_String samplename = name;
	
	if (m_audio)
	{
		/* first check if the sample is supported */
		if (SND_IsSampleValid(name, memlocation))
		{
			/* create the waveslot */
			waveslot = m_wavecache->GetWaveSlot(samplename);
			
			if (waveslot)
			{
				int buffer = waveslot->GetBuffer();
				
				/* load the sample from memory? */
				if (size && memlocation)
				{
					m_buffers[buffer] = FSOUND_Sample_Load(buffer, (char*)memlocation, FSOUND_LOADMEMORY, size);
					
					/* if the loading succeeded, fill the waveslot with info */
					if (m_buffers[buffer])
					{
						int sampleformat = SND_GetSampleFormat(memlocation);
						int numberofchannels = SND_GetNumberOfChannels(memlocation);
						int samplerate = SND_GetSampleRate(memlocation);
						int bitrate = SND_GetBitRate(memlocation);
						int numberofsamples = SND_GetNumberOfSamples(memlocation);
						
						waveslot->SetFileSize(size);
						waveslot->SetData(memlocation);
						waveslot->SetSampleFormat(sampleformat);
						waveslot->SetNumberOfChannels(numberofchannels);
						waveslot->SetSampleRate(samplerate);
						waveslot->SetBitRate(bitrate);
						waveslot->SetNumberOfSamples(numberofsamples);
					}
				}
				/* or from file? */
				else
				{
					m_buffers[buffer] = FSOUND_Sample_Load(buffer, samplename.Ptr(), FSOUND_LOOP_NORMAL, NULL);
				}
				
#ifdef ONTKEVER
				int error = FSOUND_GetError();
				printf("sample load: errornumber is: %d\n", error);
#endif
				
				/* if the loading succeeded, mark the waveslot */
				if (m_buffers[buffer])
				{
					waveslot->SetLoaded(true);
				}
				/* or when it failed, free the waveslot */
				else
				{
					m_wavecache->RemoveSample(waveslot->GetSampleName(), waveslot->GetBuffer());
					waveslot = NULL;
				}
			}
		}
	}
	
	return waveslot;
}




// listener's and general stuff //////////////////////////////////////////////////////



/* sets the global dopplervelocity */
void SND_FmodDevice::SetDopplerVelocity(MT_Scalar dopplervelocity) const
{
	/* not supported by fmod */
	FSOUND_3D_Listener_SetDopplerFactor(dopplervelocity);
}



/* sets the global dopplerfactor */
void SND_FmodDevice::SetDopplerFactor(MT_Scalar dopplerfactor) const
{
	FSOUND_3D_Listener_SetDopplerFactor(dopplerfactor);
}



/* sets the global rolloff factor */
void SND_FmodDevice::SetListenerRollOffFactor(MT_Scalar rollofffactor) const
{
	// not implemented in openal
}



void SND_FmodDevice::NextFrame() const
{
	FSOUND_3D_Update();
}



// set the gain for the listener
void SND_FmodDevice::SetListenerGain(float gain) const
{
	int fmod_gain = (int)(gain * 255);
	FSOUND_SetSFXMasterVolume(fmod_gain);
}



void SND_FmodDevice::InitListener()
{
	// initialize the listener with these values that won't change
	// (as long as we can have only one listener)
	// now we can superimpose all listeners on each other (for they
	// have the same settings)
	float lispos[3] = {0,0,0};
	float lisvel[3] = {0,0,0};

	FSOUND_3D_Listener_SetAttributes(lispos, lisvel, 0, -1, 0, 0, 0, 1);
}



// source playstate stuff ////////////////////////////////////////////////////////////



// check if the sound's still playing
int SND_FmodDevice::GetPlayState(int id)
{
	int result = SND_STOPPED;

	// klopt niet, fixen
	signed char isplaying = FSOUND_IsPlaying(id);
   
	if (isplaying)
	{
		result = SND_PLAYING;
	}

/* hi reevan, just swap // of these 2 lines */
//    return result;
	return 0;
}



/* sets the buffer */
void SND_FmodDevice::SetObjectBuffer(int id, unsigned int buffer)
{
	m_sources[id] = m_buffers[buffer];
}



// make the source play
void SND_FmodDevice::PlayObject(int id)
{
	m_channels[id] = FSOUND_PlaySound(FSOUND_FREE, m_sources[id]);
	m_frequencies[id] = FSOUND_GetFrequency(m_channels[id]);
//	printf("fmod: play \n");
}



// make the source stop
void SND_FmodDevice::StopObject(int id) const
{
	FSOUND_StopSound(m_channels[id]);
//	printf("fmod: stop \n");
}



// stop all sources
void SND_FmodDevice::StopAllObjects()
{
	FSOUND_StopSound(FSOUND_ALL);
}



// pause the source
void SND_FmodDevice::PauseObject(int id) const
{
	FSOUND_StopSound(m_channels[id]);
}



// source properties stuff ////////////////////////////////////////////////////////////



// give openal the object's pitch
void SND_FmodDevice::SetObjectPitch(int id, MT_Scalar pitch) const
{
	pitch = pitch * m_frequencies[id];
	char result = FSOUND_SetFrequency(m_channels[id], (int)pitch);
}



// give openal the object's gain
void SND_FmodDevice::SetObjectGain(int id, MT_Scalar gain) const
{
	int vol = (int)(gain * 255);
	FSOUND_SetVolume(m_channels[id], vol);
}



// give openal the object's looping
void SND_FmodDevice::SetObjectLoop(int id, unsigned int loopmode) const
{
//	printf("loopmode: %d\n", loopmode);
	switch (loopmode)
	{
	case SND_LOOP_OFF:
		{
#ifndef __APPLE__
			char result = FSOUND_Sample_SetLoopMode(m_sources[id], FSOUND_LOOP_OFF);
#else
			char result = FSOUND_SetLoopMode(m_sources[id], FSOUND_LOOP_OFF);
#endif
//			char result = FSOUND_SetLoopMode(m_channels[id], FSOUND_LOOP_OFF);
			break;
		}
	case SND_LOOP_NORMAL:
		{
#ifndef __APPLE__
			char result = FSOUND_Sample_SetLoopMode(m_sources[id], FSOUND_LOOP_NORMAL);
#else
			char result = FSOUND_SetLoopMode(m_sources[id], FSOUND_LOOP_NORMAL);
#endif
//			char result = FSOUND_SetLoopMode(m_channels[id], FSOUND_LOOP_NORMAL);
			break;
		}
	case SND_LOOP_BIDIRECTIONAL:
		{
#ifndef __APPLE__
			char result = FSOUND_Sample_SetLoopMode(m_sources[id], FSOUND_LOOP_BIDI);
#else
			char result = FSOUND_SetLoopMode(m_sources[id], FSOUND_LOOP_BIDI);
#endif
//			char result = FSOUND_SetLoopMode(m_channels[id], FSOUND_LOOP_NORMAL);
			break;
		}
	default:
		break;
	}
}



void SND_FmodDevice::SetObjectLoopPoints(int id, unsigned int loopstart, unsigned int loopend) const
{
	FSOUND_Sample_SetLoopPoints(m_sources[id], loopstart, loopend);
}



void SND_FmodDevice::SetObjectMinGain(int id, MT_Scalar mingain) const
{
	/* not supported by fmod */
}



void SND_FmodDevice::SetObjectMaxGain(int id, MT_Scalar maxgain) const
{
	/* not supported by fmod */
}



void SND_FmodDevice::SetObjectRollOffFactor(int id, MT_Scalar rollofffactor) const
{
	/* not supported by fmod */
}



void SND_FmodDevice::SetObjectReferenceDistance(int id, MT_Scalar referencedistance) const
{
	/* not supported by fmod */
}



// give openal the object's position
void SND_FmodDevice::ObjectIs2D(int id) const
{
	float obpos[3] = {0,0,0};
	float obvel[3] = {0,0,0};
	
	FSOUND_3D_SetAttributes(m_channels[id], obpos, obvel);
}



void SND_FmodDevice::SetObjectTransform(int id,
										  const MT_Vector3& position,
										  const MT_Vector3& velocity,
										  const MT_Matrix3x3& orientation,
										  const MT_Vector3& lisposition,
										  const MT_Scalar& rollofffactor) const 
{
	float obpos[3];
	float obvel[3];

	obpos[0] = (float)position[0] * (float)rollofffactor;	//x (l/r)
	obpos[1] = (float)position[1] * (float)rollofffactor;
	obpos[2] = (float)position[2] * (float)rollofffactor;

	velocity.getValue(obvel);
	FSOUND_3D_SetAttributes(m_channels[id], obpos, obvel);
}



// cd support stuff ////////////////////////////////////////////////////////////


void SND_FmodDevice::PlayCD(int track) const
{
#ifndef __APPLE__
	signed char result = FSOUND_CD_Play(track);
#else
	signed char result = FSOUND_CD_Play(0, track);
#endif

#ifdef ONTKEVER
	printf("SND_FmodDevice::PlayCD(): track=%d, result=%d\n", track, (int)result);
#endif
}



void SND_FmodDevice::PauseCD(bool pause) const
{
#ifndef __APPLE__
	signed char result = FSOUND_CD_SetPaused(pause);
#else
	signed char result = FSOUND_CD_SetPaused(0, pause);
#endif

#ifdef ONTKEVER
	printf("SND_FmodDevice::PauseCD(): pause=%d, result=%d\n", pause, (int)result);
#endif
}



void SND_FmodDevice::StopCD() const
{
	SND_CDObject* pCD = SND_CDObject::Instance();

	if (pCD)
	{
		if (pCD->GetUsed())
		{
#ifndef __APPLE__
			signed char result = FSOUND_CD_Stop();
#else
			signed char result = FSOUND_CD_Stop(0);
#endif

#ifdef ONTKEVER
			printf("SND_FmodDevice::StopCD(): result=%d\n", (int)result);
#endif
		}
	}
}



void SND_FmodDevice::SetCDPlaymode(int playmode) const
{
#ifndef __APPLE__
	FSOUND_CD_SetPlayMode(playmode);
#else
	FSOUND_CD_SetPlayMode(0, playmode);
#endif

#ifdef ONTKEVER
	printf("SND_FmodDevice::SetCDPlaymode(): playmode=%d,\n", playmode);
#endif
}



void SND_FmodDevice::SetCDGain(MT_Scalar gain) const
{
	int volume = gain * 255;
#ifndef __APPLE__
	signed char result = FSOUND_CD_SetVolume(volume);
#else
	signed char result = FSOUND_CD_SetVolume(0, volume);
#endif

#ifdef ONTKEVER
	printf("SND_FmodDevice::SetCDGain(): gain=%f, volume=%d, result=%d\n", gain, volume, (int)result);
#endif
}



void SND_FmodDevice::StartUsingDSP()
{
	m_dspunit = FSOUND_DSP_GetFFTUnit();

	FSOUND_DSP_SetActive(m_dspunit, true);
}



float* SND_FmodDevice::GetSpectrum()
{
	m_spectrum = FSOUND_DSP_GetSpectrum();

	return m_spectrum;
}



void SND_FmodDevice::StopUsingDSP()
{
	if (m_dspunit)
		FSOUND_DSP_SetActive(m_dspunit, false);
}
