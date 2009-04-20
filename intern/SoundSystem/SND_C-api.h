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
#ifndef	SND_BLENDER_H
#define SND_BLENDER_H

#ifdef __cplusplus
extern "C" { 
#endif

#include "SoundDefines.h"

#define SND_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name

SND_DECLARE_HANDLE(SND_AudioDeviceInterfaceHandle);
SND_DECLARE_HANDLE(SND_SceneHandle);
SND_DECLARE_HANDLE(SND_ObjectHandle);
SND_DECLARE_HANDLE(SND_ListenerHandle);

/**
 * set the specified type 
 */
extern void SND_SetDeviceType(int device_type);

/**
 * get an audiodevice
 */
extern SND_AudioDeviceInterfaceHandle SND_GetAudioDevice(void);

/**
 * and let go of it
 */
extern void SND_ReleaseDevice(void);

/**
 * check if playback is desired
 */
extern int SND_IsPlaybackWanted(SND_SceneHandle scene);

/**
 *	add memlocation to cache
 */
extern int SND_AddSample(SND_SceneHandle scene,
						 const char* filename,
						 void* memlocation,
						 int size);

/**
 *	remove all samples
 */
extern void	SND_RemoveAllSamples(SND_SceneHandle scene);

/**
 *  forces the object to check its buffer, and fix it if it's wrong
 */
extern int SND_CheckBuffer(SND_SceneHandle scene, SND_ObjectHandle object);

/**
  * Creates a scene, initializes it and returns a handle to that scene.
  *
  *	@param audiodevice: handle to the audiodevice.
  */
extern SND_SceneHandle SND_CreateScene(SND_AudioDeviceInterfaceHandle audiodevice);

/**
  * Stops all sounds, suspends the scene (so all resources will be freed) and deletes the scene.
  *
  *	@param scene: handle to the soundscene.
  */
extern void	SND_DeleteScene(SND_SceneHandle scene);	

/**
  * Adds a soundobject to the scene, gets the buffer the sample is loaded into.
  *
  *	@param scene: handle to the soundscene.
  * @param object: handle to soundobject.
  */
extern void	SND_AddSound(SND_SceneHandle scene, SND_ObjectHandle object);

/**
  * Removes a soundobject from the scene.
  *
  *	@param scene: handle to the soundscene.
  * @param object: handle to soundobject.
  */
extern void	SND_RemoveSound(SND_SceneHandle scene, SND_ObjectHandle object);

/**
  * Removes all soundobjects from the scene.
  *
  *	@param scene: handle to the soundscene.
  */
extern void	SND_RemoveAllSounds(SND_SceneHandle scene);

/**
  * Stopss all soundobjects in the scene.
  *
  *	@param scene: handle to the soundscene.
  */
extern void SND_StopAllSounds(SND_SceneHandle scene);

/**
  * Updates the listener, checks the status of all soundobjects, builds a list of all active
  * objects, updates the active objects.
  *
  *	@param audiodevice: handle to the audiodevice.
  *	@param scene: handle to the soundscene.
  */
extern void	SND_Proceed(SND_AudioDeviceInterfaceHandle audiodevice, SND_SceneHandle scene);

/**
  * Returns a handle to the listener.
  *
  *	@param scene: handle to the soundscene.
  */
extern SND_ListenerHandle SND_GetListener(SND_SceneHandle scene);

/**
  * Sets the gain of the listener.
  *
  *	@param scene: handle to the soundscene.
  * @param gain: factor the gain gets multiplied with.
  */
extern void SND_SetListenerGain(SND_SceneHandle scene, double gain);

/**
  * Sets a scaling to exaggerate or deemphasize the Doppler (pitch) shift resulting from the
  * calculation.
  * @attention $f' = dopplerfactor * f * frac{dopplervelocity - listener_velocity}{dopplervelocity + object_velocity}$
  *	@attention f:  frequency in sample (soundobject)
  * @attention f': effective Doppler shifted frequency
  *
  *	@param object: handle to soundobject.
  * @param dopplerfactor: the dopplerfactor.
  */
extern void	SND_SetDopplerFactor(SND_SceneHandle scene, double dopplerfactor);

/**
  * Sets the value of the propagation speed relative to which the source velocities are interpreted.
  * @attention $f' = dopplerfactor * f * frac{dopplervelocity - listener_velocity}{dopplervelocity + object_velocity}$
  *	@attention f:  frequency in sample (soundobject)
  * @attention f': effective Doppler shifted frequency
  *
  *	@param object: handle to soundobject.
  * @param dopplervelocity: the dopplervelocity.
  */
extern void	SND_SetDopplerVelocity(SND_SceneHandle scene, double dopplervelocity);

/**
  * Creates a new soundobject and returns a handle to it.
  */
extern SND_ObjectHandle	SND_CreateSound(void);

/**
  * Deletes a soundobject.
  *
  *	@param object: handle to soundobject.
  */
extern void	SND_DeleteSound(SND_ObjectHandle object);

/**
  * Sets a soundobject to SND_MUST_PLAY, so with the next proceed it will be updated and played.
  *
  *	@param object: handle to soundobject.
  */
extern void	SND_StartSound(SND_SceneHandle scene, SND_ObjectHandle object);

/**
  * Sets a soundobject to SND_MUST_STOP, so with the next proceed it will be stopped.
  *
  *	@param object: handle to soundobject.
  */
extern void	SND_StopSound(SND_SceneHandle scene, SND_ObjectHandle object);

/**
  * Sets a soundobject to SND_MUST_PAUSE, so with the next proceed it will be paused.
  *
  *	@param object: handle to soundobject.
  */
extern void	SND_PauseSound(SND_SceneHandle scene, SND_ObjectHandle object);

/**
  * Sets the name of the sample to reference the soundobject to it.
  *
  *	@param object: handle to soundobject.
  * @param samplename: the name of the sample
  */
extern void	SND_SetSampleName(SND_ObjectHandle object, char* samplename);

/**
  * Sets the gain of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param gain: factor the gain gets multiplied with.
  */
extern void	SND_SetGain(SND_ObjectHandle object, double gain);

/**
  * Sets the minimum gain of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param minimumgain: lower threshold for the gain.
  */
extern void	SND_SetMinimumGain(SND_ObjectHandle object, double minimumgain);

/**
  * Sets the maximum gain of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param maximumgain: upper threshold for the gain.
  */
extern void	SND_SetMaximumGain(SND_ObjectHandle object, double maximumgain);

/**
  * Sets the rollofffactor. The	rollofffactor is a per-Source parameter the application 
  * can use to increase or decrease	the range of a source by decreasing or increasing the
  * attenuation, respectively. The default value is 1. The implementation is free to optimize
  * for a rollofffactor value of 0, which indicates that the application does not wish any
  * distance attenuation on the respective Source.
  *
  *	@param object: handle to soundobject.
  * @param rollofffactor: the rollofffactor.
  */
extern void	SND_SetRollOffFactor(SND_ObjectHandle object, double rollofffactor);

/**
  * Sets the referencedistance at which the listener will experience gain.
  * @attention G_dB = gain - 20 * log10(1 + rollofffactor * (dist - referencedistance)/referencedistance);
  *
  *	@param object: handle to soundobject.
  * @param distance: the reference distance.
  */
extern void	SND_SetReferenceDistance(SND_ObjectHandle object, double referencedistance);

/**
  * Sets the pitch of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param pitch: pitchingfactor: 2.0 for doubling the frequency, 0.5 for half the frequency.
  */
extern void	SND_SetPitch(SND_ObjectHandle object, double pitch);

/**
  * Sets the position a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param position: position[3].
  */
extern void SND_SetPosition(SND_ObjectHandle object, double* position);

/**
  * Sets the velocity of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param velocity: velocity[3].
  */
extern void SND_SetVelocity(SND_ObjectHandle object, double* velocity);

/**
  * Sets the orientation of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param orientation: orientation[9].
  */
extern void SND_SetOrientation(SND_ObjectHandle object, double* orientation);

/**
  * Sets the loopmode of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param loopmode	type of the loop (SND_LOOP_OFF, SND_LOOP_NORMAL, SND_LOOP_BIDIRECTIONAL);
 */
extern void SND_SetLoopMode(SND_ObjectHandle object, int loopmode);

/**
  * Sets the looppoints of a soundobject.
  *
  *	@param object: handle to soundobject.
  * @param loopstart	startpoint of the loop
  * @param loopend		endpoint of the loop
 */
extern void SND_SetLoopPoints(SND_ObjectHandle object, unsigned int loopstart, unsigned int loopend);

/**
  * Gets the gain of a soundobject.
  *
  *	@param object: handle to soundobject.
  */
extern float SND_GetGain(SND_ObjectHandle object);

/**
  * Gets the pitch of a soundobject.
  *
  *	@param object: handle to soundobject.
  */
extern float SND_GetPitch(SND_ObjectHandle object);

/**
  * Gets the looping of a soundobject.
  * 0: SND_LOOP_OFF
  * 1: SND_LOOP_NORMAL
  * 2: SND_LOOP_BIDIRECTIONAL
  *
  *	@param object: handle to soundobject.
  */
extern int SND_GetLoopMode(SND_ObjectHandle object);

/**
  * Gets the playstate of a soundobject.
  *	SND_UNKNOWN = -1
  *	SND_INITIAL
  *	SND_MUST_PLAY
  *	SND_PLAYING
  *	SND_MUST_STOP
  *	SND_STOPPED
  *	SND_MUST_PAUSE
  *	SND_PAUSED
  *	SND_MUST_BE_DELETED
  *
  *	@param object: handle to soundobject.
  */
extern int SND_GetPlaystate(SND_ObjectHandle object);

#ifdef __cplusplus
}
#endif

#endif

