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
#ifndef SND_IAUDIODEVICE
#define SND_IAUDIODEVICE

#include "SND_SoundObject.h"
#include "SND_CDObject.h"
#include "SND_WaveCache.h"
#include "SND_WaveSlot.h"
#include "MT_Matrix3x3.h"

class SND_IAudioDevice
{
public:

	/**
	 * constructor
	 */
	SND_IAudioDevice() {};

	/**
	 * destructor
	 */
	virtual ~SND_IAudioDevice() {};

	/**
	 * check to see if initialization was successfull
	 *
	 * @return indication of succes
	 */
	virtual bool IsInitialized()=0;

	/**
	 * get the wavecache (which does sample (un)loading)
	 *
	 * @return pointer to the wavecache
	 */
	virtual SND_WaveCache* GetWaveCache() const =0;

	/**
	 * loads a sample into the device
	 *
	 * @param samplename	the name of the sample
	 * @param memlocation	pointer where the sample is stored
	 * @param size			size of the sample in memory
	 *
	 * @return pointer to the slot with sample data
	 */
	virtual SND_WaveSlot* LoadSample(const STR_String& samplename,
									 void* memlocation,
									 int size)=0;

	/**
	 * remove a sample from the wavecache
	 *
	 * @param filename	pointer to filename
	 */
//	virtual	void RemoveSample(const char* filename)=0;
	
	/**
	 * remove all samples from the wavecache
	 */
	virtual	void RemoveAllSamples()=0;

	/**
	 * get a new id from the device
	 *
	 * @param pObject	pointer to soundobject
	 *
	 * @return indication of success
	 */
	virtual bool GetNewId(SND_SoundObject* pObject)=0;
	
	/**
	 * clear an id
	 *
	 * @param pObject	pointer to soundobject
	 */
	virtual void ClearId(SND_SoundObject* pObject)=0;
	
	/**
	 * initialize the listener
	 */
	virtual void InitListener()=0;
	
	/**
	 * set the value of the propagation speed relative to which the 
	 * source velocities are interpreted.
	 * f' = DOPPLER_FACTOR * f * (DOPPLER_VELOCITY - Vl) / (DOPPLER_VELOCITY + Vo)
	 * f:  frequency in sample (soundobject)
     * f': effective Doppler shifted frequency
	 * Vl: velocity listener
	 * Vo: velocity soundobject
	 *
	 * @param dopplervelocity	scaling factor for doppler effect
	 */
	virtual void SetDopplerVelocity(MT_Scalar dopplervelocity) const =0;
	
	/**
	 * set a scaling to exaggerate or deemphasize the Doppler (pitch) 
	 * shift resulting from the calculation.
	 * f' = DOPPLER_FACTOR * f * (DOPPLER_VELOCITY - Listener_velocity )/(DOPPLER_VELOCITY + object_velocity )
	 *
	 * @param dopplerfactor	scaling factor for doppler effect
	 */
	virtual void SetDopplerFactor(MT_Scalar dopplerfactor) const =0;
	
	/**
	 * set the roll-off factor
	 *
	 * @param rollofffactor	a global volume scaling factor
	 */
	virtual	void SetListenerRollOffFactor(MT_Scalar rollofffactor) const =0;

	/**
	 * make the context the current one
	 */
	virtual void MakeCurrent() const =0;

	/**
	 * update the device
	 */
	virtual void NextFrame() const =0;

	/**
	 * set the volume of the listener.
	 *
	 * @param gain	the mastergain
	 */
	virtual void SetListenerGain(float gain) const =0;

	/**
	 * connect the buffer with the source
	 *
	 * @param id		the id of the object
	 * @param buffer	the buffer the sample is stored in
	 */
	virtual void SetObjectBuffer(int id, unsigned int buffer)=0; 

	/**
	 * pause playback of the cd
	 * @param id	the id of the object
	 *
	 * @return the state the object is in
	 */
	virtual int GetPlayState(int id) =0;

	/**
	 * play a sound belonging to an object.
	 *
	 * @param id	the id of the object
	 */
	virtual void PlayObject(int id) =0;
	
	/**
	 * stop a sound belonging to an object.
	 *
	 * @param id	the id of the object
	 */
	virtual void StopObject(int id) const =0;
	
	/**
	 * stop all sounds.
	 */
	virtual void StopAllObjects()=0;
	
	/**
	 * pause the sound belonging to an object.
	 *
	 * @param id	the id of the object
	 */
	virtual void PauseObject(int id) const =0;

	/**
	 * set the sound to looping or non-looping.
	 *
	 * @param id	the id of the object
	 * @param loopmode	type of looping (no loop, normal, bidirectional)
	 */
	virtual void SetObjectLoop(int id, unsigned int loopmode) const =0;
	
	/**
	 * set the looppoints of a sound
	 *
	 * @param id	the id of the object
	 * @param loopstart	the startpoint of the loop (in samples)
	 * @param loopend	the endpoint of the loop (in samples)
	 */
	virtual void SetObjectLoopPoints(int id, unsigned int loopstart, unsigned int loopend) const =0;

	/**
	 * set the pitch of the sound.
	 *
	 * @param id	the id of the object
	 * @param pitch	the pitch
	 */
	virtual void SetObjectPitch(int id, MT_Scalar pitch) const =0;
	
	/**
	 * set the gain of the sound.
	 *
	 * @param id	the id of the object
	 * @param gain	the gain
	 */
	virtual void SetObjectGain(int id, MT_Scalar gain) const =0;
	
	/**
	 * ROLLOFF_FACTOR is per-Source parameter the application can use to increase or decrease
	 * the range of a source by decreasing or increasing the attenuation, respectively. The 
	 * default value is 1. The implementation is free to optimize for a ROLLOFF_FACTOR value 
	 * of 0, which indicates that the application does not wish any distance attenuation on 
	 * the respective Source. 
	 *
	 * @param id		the id of the object
	 * @param rolloff	a per-source volume scaling factor
	 */
	virtual void SetObjectRollOffFactor(int id, MT_Scalar rolloff) const =0;

	/**
	 * min_gain indicates the minimal gain which is always guaranteed for this sound
	 *
	 * @param id		the id of the object
	 * @param mingain	the minimum gain of the object
	 */
	virtual void SetObjectMinGain(int id, MT_Scalar mingain) const =0;

	/**
	 * max_gain indicates the maximal gain which is always guaranteed for this sound
	 *
	 * @param id		the id of the object
	 * @param maxgain	the maximum gain of the object
	 */
	virtual void SetObjectMaxGain(int id, MT_Scalar maxgain) const =0;
	/**
	 * set the distance at which the Listener will experience gain.
     * G_dB = GAIN - 20*log10(1 + ROLLOFF_FACTOR*(dist-REFERENCE_DISTANCE)/REFERENCE_DISTANCE );
	 *
	 * @param id				the id of the object
	 * @param referencedistance	the distance at which the listener will start hearing
	 */
	virtual void SetObjectReferenceDistance(int id, MT_Scalar referencedistance) const =0;

	/**
	 * set the position, velocity and orientation of a sound.
	 *
	 * @param id			the id of the object
	 * @param position		the position of the object
	 * @param velocity		the velocity of the object
	 * @param orientation	the orientation of the object
	 * @param lisposition	the position of the listener
	 * @param rollofffactor	the rollofffactor of the object
	 */
	virtual void SetObjectTransform(int id,
									const MT_Vector3& position,
									const MT_Vector3& velocity,
									const MT_Matrix3x3& orientation,	
									const MT_Vector3& lisposition,
									const MT_Scalar& rollofffactor) const =0;

	/**
	 * make a sound 2D
	 *
	 * @param id	the id of the object
	 */
	virtual void ObjectIs2D(int id) const =0;

	/**
	 * tell the device we want cd suppport
	 */
	virtual void UseCD() const =0;
	
	/**
	 * start playback of the cd
	 *
	 * @param track	the tracknumber to start playback from
	 */
	virtual void PlayCD(int track) const =0;

	/**
	 * pause playback of the cd (true == pause, false == resume)
	 */
	virtual void PauseCD(bool pause) const =0;

	/**
	 * stop playback of the cd
	 */
	virtual void StopCD() const =0;

	/**
	 * set the playbackmode of the cd
	 * SND_CD_ALL		play all tracks
	 * SND_CD_TRACK		play one track
	 * SND_CD_TRACKLOOP	play one track looped
	 * SND_CD_RANDOM	play all tracks in random order
	 *
	 * @param playmode	playmode
	 */
	virtual void SetCDPlaymode(int playmode) const =0;
	
	/**
	 * set the volume playback of the cd
	 *
	 * @param gain	the gain
	 */
	virtual void SetCDGain(MT_Scalar gain) const =0;

	virtual void StartUsingDSP() =0;
	virtual float* GetSpectrum() =0;
	virtual void StopUsingDSP() =0;

protected:

	virtual void RevokeSoundObject(SND_SoundObject* pObject)=0;
};

#endif //SND_IAUDIODEVICE

