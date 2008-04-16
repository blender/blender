/**
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
#ifndef SND_OPENALDEVICE
#define SND_OPENALDEVICE

#include "SND_AudioDevice.h"
#include "SoundDefines.h"

typedef struct SDL_CD;

class SND_OpenALDevice : public SND_AudioDevice
{
public:
	SND_OpenALDevice();
	virtual ~SND_OpenALDevice();

	SND_WaveSlot* LoadSample(const STR_String& samplename,
							 void* memlocation,
							 int size);

	void InitListener();
	void SetListenerGain(float gain) const;
	void SetDopplerVelocity(MT_Scalar dopplervelocity) const;
	void SetDopplerFactor(MT_Scalar dopplerfactor) const;
	void SetListenerRollOffFactor(MT_Scalar rollofffactor) const;

	void MakeCurrent() const;

	void NextFrame() const;
	void UseCD() const;

	void SetObjectBuffer(int id, unsigned int buffer); 

	int GetPlayState(int id);
	void PlayObject(int id);
	void StopObject(int id) const;
	void StopAllObjects();
	void PauseObject(int id) const;

	void SetObjectLoop(int id, unsigned int loopmode) const;
	void SetObjectLoopPoints(int id, unsigned int loopstart, unsigned int loopend) const;
	void SetObjectPitch(int id, MT_Scalar pitch) const;
	void SetObjectGain(int id, MT_Scalar gain) const;
	void SetObjectMinGain(int id, MT_Scalar mingain) const;
	void SetObjectMaxGain(int id, MT_Scalar maxgain) const;
	void SetObjectRollOffFactor(int id, MT_Scalar rolloff) const;
	void SetObjectReferenceDistance(int id, MT_Scalar distance) const;

	void SetObjectTransform(int id,
							const MT_Vector3& position,
							const MT_Vector3& velocity,
							const MT_Matrix3x3& orientation,	
							const MT_Vector3& lisposition,
							const MT_Scalar& rollofffactor) const;
	void ObjectIs2D(int id) const;

	void PlayCD(int track) const;
	void PauseCD(bool pause) const;
	void StopCD() const;
	void SetCDPlaymode(int playmode) const;
	void SetCDGain(MT_Scalar gain) const;
	
	void StartUsingDSP()	{};
	float* GetSpectrum()	{ return NULL; }
	void StopUsingDSP()		{};

private:
	void*			m_context;
	void*			m_device;

	unsigned int	m_buffers[NUM_BUFFERS];
	unsigned int	m_sources[NUM_SOURCES];
	bool			m_buffersinitialized;
	bool			m_sourcesinitialized;
#ifndef __APPLE__
	class SND_SDLCDDevice*	m_cdrom;
#endif
};

#endif //SND_OPENALDEVICE

