/*
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * SND_OpenALDevice derived from SND_IAudioDevice
 */

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "SND_OpenALDevice.h"
#ifndef __APPLE__
#include "SND_SDLCDDevice.h"
#endif
#include "SoundDefines.h"

#include "SND_Utils.h"

#ifdef APPLE_FRAMEWORK_FIX
#include <al.h>
#include <alc.h>
#include <alut.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#if defined(WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#include <signal.h>

/* untill openal gets unified we need this hack for non-windows systems */
#if !defined(WIN32) && !defined(ALC_MAJOR_VERSION)

#include <malloc.h>

ALvoid alutLoadWAVMemory(ALbyte *memory,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq,ALboolean *loop);
ALvoid alutUnloadWAV(ALenum format,ALvoid *data,ALsizei size,ALsizei freq);

typedef struct                                  /* WAV File-header */
{
  ALubyte  Id[4];
  ALsizei  Size;
  ALubyte  Type[4];
} WAVFileHdr_Struct;

typedef struct                                  /* WAV Fmt-header */
{
  ALushort Format;                              
  ALushort Channels;
  ALuint   SamplesPerSec;
  ALuint   BytesPerSec;
  ALushort BlockAlign;
  ALushort BitsPerSample;
} WAVFmtHdr_Struct;

typedef struct									/* WAV FmtEx-header */
{
  ALushort Size;
  ALushort SamplesPerBlock;
} WAVFmtExHdr_Struct;

typedef struct                                  /* WAV Smpl-header */
{
  ALuint   Manufacturer;
  ALuint   Product;
  ALuint   SamplePeriod;                          
  ALuint   Note;                                  
  ALuint   FineTune;                              
  ALuint   SMPTEFormat;
  ALuint   SMPTEOffest;
  ALuint   Loops;
  ALuint   SamplerData;
  struct
  {
    ALuint Identifier;
    ALuint Type;
    ALuint Start;
    ALuint End;
    ALuint Fraction;
    ALuint Count;
  }      Loop[1];
} WAVSmplHdr_Struct;

typedef struct                                  /* WAV Chunk-header */
{
  ALubyte  Id[4];
  ALuint   Size;
} WAVChunkHdr_Struct;

ALvoid alutLoadWAVMemory(ALbyte *memory,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq,ALboolean *loop)
{
	WAVChunkHdr_Struct ChunkHdr;
	WAVFmtExHdr_Struct FmtExHdr;
	WAVFileHdr_Struct FileHdr;
	WAVSmplHdr_Struct SmplHdr;
	WAVFmtHdr_Struct FmtHdr;
	ALbyte *Stream;
	
	*format=AL_FORMAT_MONO16;
	*data=NULL;
	*size=0;
	*freq=22050;
	*loop=AL_FALSE;
	if (memory)
	{
		Stream=memory;
		if (Stream)
		{
			memcpy(&FileHdr,Stream,sizeof(WAVFileHdr_Struct));
			Stream+=sizeof(WAVFileHdr_Struct);
			FileHdr.Size=((FileHdr.Size+1)&~1)-4;
			while ((FileHdr.Size!=0)&&(memcpy(&ChunkHdr,Stream,sizeof(WAVChunkHdr_Struct))))
			{
				Stream+=sizeof(WAVChunkHdr_Struct);
				if (!memcmp(ChunkHdr.Id,"fmt ",4))
				{
					memcpy(&FmtHdr,Stream,sizeof(WAVFmtHdr_Struct));
					if (FmtHdr.Format==0x0001)
					{
						*format=(FmtHdr.Channels==1?
								(FmtHdr.BitsPerSample==8?AL_FORMAT_MONO8:AL_FORMAT_MONO16):
								(FmtHdr.BitsPerSample==8?AL_FORMAT_STEREO8:AL_FORMAT_STEREO16));
						*freq=FmtHdr.SamplesPerSec;
						Stream+=ChunkHdr.Size;
					} 
					else
					{
						memcpy(&FmtExHdr,Stream,sizeof(WAVFmtExHdr_Struct));
						Stream+=ChunkHdr.Size;
					}
				}
				else if (!memcmp(ChunkHdr.Id,"data",4))
				{
					if (FmtHdr.Format==0x0001)
					{
						*size=ChunkHdr.Size;
						*data=malloc(ChunkHdr.Size+31);
						if (*data) memcpy(*data,Stream,ChunkHdr.Size);
						memset(((char *)*data)+ChunkHdr.Size,0,31);
						Stream+=ChunkHdr.Size;
					}
					else if (FmtHdr.Format==0x0011)
					{
						//IMA ADPCM
					}
					else if (FmtHdr.Format==0x0055)
					{
						//MP3 WAVE
					}
				}
				else if (!memcmp(ChunkHdr.Id,"smpl",4))
				{
					memcpy(&SmplHdr,Stream,sizeof(WAVSmplHdr_Struct));
					*loop = (SmplHdr.Loops ? AL_TRUE : AL_FALSE);
					Stream+=ChunkHdr.Size;
				}
				else Stream+=ChunkHdr.Size;
				Stream+=ChunkHdr.Size&1;
				FileHdr.Size-=(((ChunkHdr.Size+1)&~1)+8);
			}
		}
	}
}

ALvoid alutUnloadWAV(ALenum format,ALvoid *data,ALsizei size,ALsizei freq)
{
	if (data)
		free(data);
}

#endif /* WIN32 */

#ifdef __APPLE__
#define OUDE_OPENAL 1
#endif


SND_OpenALDevice::SND_OpenALDevice()
	: SND_AudioDevice(),
	  m_context(NULL),
	  m_device(NULL)
{
    /* Removed the functionality for checking if noaudio was provided on */
    /* the commandline. */
	m_audio = true;
	m_context = NULL;
	m_buffersinitialized = false;
	m_sourcesinitialized = false;

	// let's check if we can get openal to initialize...
	if (m_audio)
	{
#ifdef OUDE_OPENAL
		m_audio = true;			// openal_2.12
		alutInit(NULL, NULL);	// openal_2.12
#else
		m_audio = false;

		ALCdevice *dev = alcOpenDevice(NULL);
		if (dev) {
			m_context = alcCreateContext(dev, NULL);

			if (m_context) {
#ifdef AL_VERSION_1_1
		alcMakeContextCurrent((ALCcontext*)m_context);
#else
			alcMakeContextCurrent(m_context);
#endif
			m_audio = true;
				m_device = dev;
#ifdef __linux__
				/*
				*   SIGHUP Hack:
				*
				*   On Linux, alcDestroyContext generates a SIGHUP (Hangup) when killing the OpenAL
				*   mixer thread, which kills Blender.
				*  
				*   So we set the signal to ignore....
				*
				*   TODO: check if this applies to other platforms.
				*
				*/
				signal(SIGHUP, SIG_IGN);
#endif
			}
		}

#endif
	}

	// then try to generate some buffers
	if (m_audio)
	{
		// let openal generate its buffers
		alGenBuffers(NUM_BUFFERS, m_buffers);
		m_buffersinitialized = true;
		
		for (int i = 0; i < NUM_BUFFERS; i++)
		{
			if (!alIsBuffer(m_buffers[i]))
			{
				//printf("\n\n  WARNING: OpenAL returned with an error. Continuing without audio.\n\n");
				m_audio = false;
				break;
			}
		}
	}

	// next: the sources
	if (m_audio)
	{
#ifdef OUDE_OPENAL
		ALenum alc_error = ALC_NO_ERROR;	// openal_2.12
#elif defined(_WIN32)
		// alcGetError has no arguments on windows
		ALenum alc_error = alcGetError();       // openal_2.14+
#else
		ALenum alc_error = alcGetError(NULL);	// openal_2.14+
#endif

		// let openal generate its sources
		if (alc_error == ALC_NO_ERROR)
		{
			alGenSources(NUM_SOURCES, m_sources);
			m_sourcesinitialized = true;
		}
	}

	// let's get us a wavecache
	if (m_audio)
	{
		m_wavecache = new SND_WaveCache();
	}
#ifndef __APPLE__	
	m_cdrom = new SND_SDLCDDevice();
#endif
}

void SND_OpenALDevice::UseCD(void) const
{
	// only fmod has CD support, so only create it here
	SND_CDObject::CreateSystem();

}

void SND_OpenALDevice::MakeCurrent() const
{
#ifdef WIN32
	alcMakeContextCurrent(m_context);
#endif
}



SND_OpenALDevice::~SND_OpenALDevice()
{
	MakeCurrent();
	
	if (m_sourcesinitialized)
	{
		for (int i = 0; i < NUM_SOURCES; i++)
			alSourceStop(m_sources[i]);
		
		alDeleteSources(NUM_SOURCES, m_sources);
		m_sourcesinitialized = false;
	}
	
	if (m_buffersinitialized)
	{
		alDeleteBuffers(NUM_BUFFERS, m_buffers);
		m_buffersinitialized = false;
	}
	
	if (m_context) {
		MakeCurrent();
#ifdef AL_VERSION_1_1
		alcDestroyContext((ALCcontext*)m_context);
#else
		alcDestroyContext(m_context);
#endif
		m_context = NULL;
	}
	
#ifdef __linux__
	// restore the signal state above.
	signal(SIGHUP, SIG_DFL);
#endif	
	// let's see if we used the cd. if not, just leave it alone
	SND_CDObject* pCD = SND_CDObject::Instance();
	
	if (pCD)
	{
		this->StopCD();
		SND_CDObject::DisposeSystem();
	}
#ifndef __APPLE__
	if (m_cdrom)
		delete m_cdrom;
#endif
#ifdef OUDE_OPENAL
	if (m_audio)
		alutExit();
#else
	if (m_device)
		alcCloseDevice((ALCdevice*) m_device);
#endif
}



SND_WaveSlot* SND_OpenALDevice::LoadSample(const STR_String& name,
										  void* memlocation,
										  int size)
{
	SND_WaveSlot* waveslot = NULL;
	STR_String samplename = name;
	
	if (m_audio)
	{
		/* create the waveslot */
		waveslot = m_wavecache->GetWaveSlot(samplename);

		/* do we support this sample? */
		if (SND_IsSampleValid(name, memlocation))
		{
			if (waveslot)
			{
				int buffer = waveslot->GetBuffer();
				void* data = NULL;
#ifndef __APPLE__
				char loop = 'a';
#endif
				int sampleformat, bitrate, numberofchannels;
				ALenum al_error = alGetError();
				
#ifdef OUDE_OPENAL
				ALsizei samplerate, numberofsamples;		// openal_2.12
#else
				int samplerate, numberofsamples;  // openal_2.14+
#endif
				
				/* Give them some safe defaults just incase */
				bitrate = numberofchannels = 0;

				/* load the sample from memory? */
				if (size && memlocation)
				{
					waveslot->SetFileSize(size);
					
					/* what was (our) buffer? */
					int buffer = waveslot->GetBuffer();
					
					/* get some info out of the sample */
					SND_GetSampleInfo((signed char*)memlocation, waveslot);
					numberofchannels = SND_GetNumberOfChannels(memlocation);
					bitrate = SND_GetBitRate(memlocation);
					
					/* load the sample into openal */
#if defined(OUDE_OPENAL) || defined (__APPLE__)
					alutLoadWAVMemory((char*)memlocation, &sampleformat, &data, &numberofsamples, &samplerate);				//	openal_2.12
#else
#ifdef AL_VERSION_1_1					
					alutLoadWAVMemory((ALbyte*)memlocation, &sampleformat, &data, &numberofsamples, &samplerate, &loop);//	openal_2.14+
#else
					 alutLoadWAVMemory((signed char*)memlocation, &sampleformat, &data, &numberofsamples, &samplerate, &loop);//  openal_2.14+
					 
#endif 
#endif
					/* put it in the buffer */
					alBufferData(m_buffers[buffer], sampleformat, data, numberofsamples, samplerate);
				}
				/* or from file? */
				else
				{
#ifdef __APPLE__
					alutLoadWAVFile((ALbyte *)samplename.Ptr(), &sampleformat, &data, &numberofsamples, &samplerate);
#else
					alutLoadWAVFile((ALbyte *)samplename.Ptr(), &sampleformat, &data, &numberofsamples, &samplerate, &loop);
#endif
					/* put it in the buffer */
					alBufferData(m_buffers[buffer], sampleformat, data, numberofsamples, samplerate);
				}
				
				/* fill the waveslot with info */
				al_error = alGetError();
				if (al_error == AL_NO_ERROR && m_buffers[buffer])
				{
					waveslot->SetData(data);
					waveslot->SetSampleFormat(sampleformat);
					waveslot->SetNumberOfChannels(numberofchannels);
					waveslot->SetSampleRate(samplerate);
					waveslot->SetBitRate(bitrate);
					waveslot->SetNumberOfSamples(numberofsamples);
					
					/* if the loading succeeded, mark the waveslot */
					waveslot->SetLoaded(true);
				}
				else
				{
					/* or when it failed, free the waveslot */
					m_wavecache->RemoveSample(waveslot->GetSampleName(), waveslot->GetBuffer());
					waveslot = NULL;
				}
				
				/* and free the original stuff (copy was made in openal) */
				alutUnloadWAV(sampleformat, data, numberofsamples, samplerate);
			}
		}
		else
		{
			/* sample not supported, remove waveslot */
			m_wavecache->RemoveSample(waveslot->GetSampleName(), waveslot->GetBuffer());
			waveslot = NULL;
		}
	}
	return waveslot;
}



// listener's and general stuff //////////////////////////////////////////////////////



/* sets the global dopplervelocity */
void SND_OpenALDevice::SetDopplerVelocity(MT_Scalar dopplervelocity) const
{
	alDopplerVelocity ((float)dopplervelocity);
}



/* sets the global dopplerfactor */
void SND_OpenALDevice::SetDopplerFactor(MT_Scalar dopplerfactor) const
{
	alDopplerFactor ((float)dopplerfactor);
}



/* sets the global rolloff factor */
void SND_OpenALDevice::SetListenerRollOffFactor(MT_Scalar rollofffactor) const
{
	// not implemented in openal
}



void SND_OpenALDevice::NextFrame() const
{
	// CD
#ifndef __APPLE__
	m_cdrom->NextFrame();
#endif
	// not needed by openal
}



// set the gain for the listener
void SND_OpenALDevice::SetListenerGain(float gain) const
{
	alListenerf (AL_GAIN, gain);
}



void SND_OpenALDevice::InitListener()
{
	// initialize the listener with these values that won't change
	// (as long as we can have only one listener)
	// now we can superimpose all listeners on each other (for they
	// have the same settings)
	float lispos[3] = {0,0,0};
	float lisvel[3] = {0,0,0};
#ifdef WIN32
	float lisori[6] = {0,1,0,0,0,1};
#else
	float lisori[6] = {0,0,1,0,-1,0};
#endif

	alListenerfv(AL_POSITION, lispos);
	alListenerfv(AL_VELOCITY, lisvel);
	alListenerfv(AL_ORIENTATION, lisori);
}



// source playstate stuff ////////////////////////////////////////////////////////////



/* sets the buffer */
void SND_OpenALDevice::SetObjectBuffer(int id, unsigned int buffer)
{
	alSourcei (m_sources[id], AL_BUFFER, m_buffers[buffer]);
}



// check if the sound's still playing
int SND_OpenALDevice::GetPlayState(int id)
{
    int alstate = 0;
	int result = 0;

#ifdef __APPLE__
	alGetSourcei(m_sources[id], AL_SOURCE_STATE, &alstate);
#else
    alGetSourceiv(m_sources[id], AL_SOURCE_STATE, &alstate);
#endif
	
	switch(alstate)
	{
	case AL_INITIAL:
		{
			result = SND_INITIAL;
			break;
		}
	case AL_PLAYING:
		{
			result = SND_PLAYING;
			break;
		}
	case AL_PAUSED:
		{
			result = SND_PAUSED;
			break;
		}
	case AL_STOPPED:
		{
			result = SND_STOPPED;
			break;
		}
	default:
		result = SND_UNKNOWN;
	}

    return result;
}



// make the source play
void SND_OpenALDevice::PlayObject(int id)
{
	alSourcePlay(m_sources[id]);
}



// make the source stop
void SND_OpenALDevice::StopObject(int id) const
{
	float obpos[3] = {0,0,0};
	float obvel[3] = {0,0,0};

	alSourcefv(m_sources[id], AL_POSITION, obpos);
	alSourcefv(m_sources[id], AL_VELOCITY, obvel);

	alSourcef(m_sources[id], AL_GAIN, 1.0);
	alSourcef(m_sources[id], AL_PITCH, 1.0);
	alSourcei(m_sources[id], AL_LOOPING, AL_FALSE);
	alSourceStop(m_sources[id]);
}



// stop all sources
void SND_OpenALDevice::StopAllObjects()
{
	alSourceStopv(NUM_SOURCES, m_sources);
}



// pause the source
void SND_OpenALDevice::PauseObject(int id) const
{
	alSourcePause(m_sources[id]);
}



// source properties stuff ////////////////////////////////////////////////////////////



// give openal the object's pitch
void SND_OpenALDevice::SetObjectPitch(int id, MT_Scalar pitch) const
{
	alSourcef (m_sources[id], AL_PITCH, (float)pitch);
}



// give openal the object's gain
void SND_OpenALDevice::SetObjectGain(int id, MT_Scalar gain) const
{
	alSourcef (m_sources[id], AL_GAIN, (float)gain);
}



// give openal the object's looping
void SND_OpenALDevice::SetObjectLoop(int id, unsigned int loopmode) const
{
	if (loopmode == SND_LOOP_OFF)
	{
		//printf("%d - ", id);
		alSourcei (m_sources[id], AL_LOOPING, AL_FALSE);
	}
	else
		alSourcei (m_sources[id], AL_LOOPING, AL_TRUE);
}

void SND_OpenALDevice::SetObjectLoopPoints(int id, unsigned int loopstart, unsigned int loopend) const
{


}


void SND_OpenALDevice::SetObjectMinGain(int id, MT_Scalar mingain) const
{
	alSourcef (m_sources[id], AL_MIN_GAIN, (float)mingain);
}



void SND_OpenALDevice::SetObjectMaxGain(int id, MT_Scalar maxgain) const
{
	alSourcef (m_sources[id], AL_MAX_GAIN, (float)maxgain);
}



void SND_OpenALDevice::SetObjectRollOffFactor(int id, MT_Scalar rollofffactor) const
{
	alSourcef (m_sources[id], AL_ROLLOFF_FACTOR, (float)rollofffactor);
}



void SND_OpenALDevice::SetObjectReferenceDistance(int id, MT_Scalar referencedistance) const
{
	alSourcef (m_sources[id], AL_REFERENCE_DISTANCE, (float)referencedistance);
}



// give openal the object's position
void SND_OpenALDevice::ObjectIs2D(int id) const
{
	float obpos[3] = {0,0,0};
	float obvel[3] = {0,0,0};
	
	alSourcefv(m_sources[id], AL_POSITION, obpos);
	alSourcefv(m_sources[id], AL_VELOCITY, obvel);
}



void SND_OpenALDevice::SetObjectTransform(int id,
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

	alSourcefv(m_sources[id], AL_POSITION, obpos);

	velocity.getValue(obvel);
	alSourcefv(m_sources[id], AL_VELOCITY, obvel);
}

void SND_OpenALDevice::PlayCD(int track) const
{
#ifndef __APPLE__
	m_cdrom->PlayCD(track);
#endif
}


void SND_OpenALDevice::PauseCD(bool pause) const
{
#ifndef __APPLE__
	m_cdrom->PauseCD(pause);
#endif
}

void SND_OpenALDevice::StopCD() const
{
#ifndef __APPLE__
	SND_CDObject* pCD = SND_CDObject::Instance();

	if (pCD && pCD->GetUsed())
	{
		m_cdrom->StopCD();
	}
#endif
}

void SND_OpenALDevice::SetCDPlaymode(int playmode) const
{
#ifndef __APPLE__
	m_cdrom->SetCDPlaymode(playmode);
#endif
}

void SND_OpenALDevice::SetCDGain(MT_Scalar gain) const
{
#ifndef __APPLE__
	m_cdrom->SetCDGain(gain);
#endif
}
