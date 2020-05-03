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

#pragma once

#include "AUD_Device.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Possible animatable properties for Sequence Factories and Entries.
typedef enum
{
	AUD_AP_VOLUME,
	AUD_AP_PANNING,
	AUD_AP_PITCH,
	AUD_AP_LOCATION,
	AUD_AP_ORIENTATION
} AUD_AnimateablePropertyType;

/**
 * Creates a new sequenced sound scene.
 * \param fps The FPS of the scene.
 * \param muted Whether the scene is muted.
 * \return The new sound scene.
 */
extern AUD_API AUD_Sound* AUD_Sequence_create(float fps, int muted);

/**
 * Deletes a sound scene.
 * \param sequence The sound scene.
 */
extern AUD_API void AUD_Sequence_free(AUD_Sound* sequence);

/**
 * Adds a new entry to the scene.
 * \param sequence The sound scene.
 * \param sound The sound this entry should play.
 * \param begin The start time.
 * \param end The end time or a negative value if determined by the sound.
 * \param skip How much seconds should be skipped at the beginning.
 * \return The entry added.
 */
extern AUD_API AUD_SequenceEntry* AUD_Sequence_add(AUD_Sound* sequence, AUD_Sound* sound, double begin, double end, double skip);

/**
 * Removes an entry from the scene.
 * \param sequence The sound scene.
 * \param entry The entry to remove.
 */
extern AUD_API void AUD_Sequence_remove(AUD_Sound* sequence, AUD_SequenceEntry* entry);

/**
 * Writes animation data to a sequence.
 * \param sequence The sound scene.
 * \param type The type of animation data.
 * \param frame The frame this data is for.
 * \param data The data to write.
 * \param animated Whether the attribute is animated.
 */
extern AUD_API void AUD_Sequence_setAnimationData(AUD_Sound* sequence, AUD_AnimateablePropertyType type, int frame, float* data, char animated);

/**
 * Retrieves the distance model of a sequence.
 * param sequence The sequence to get the distance model from.
 * return The distance model of the sequence.
 */
extern AUD_API AUD_DistanceModel AUD_Sequence_getDistanceModel(AUD_Sound* sequence);

/**
 * Sets the distance model of a sequence.
 * param sequence The sequence to set the distance model from.
 * param value The new distance model to set.
 */
extern AUD_API void AUD_Sequence_setDistanceModel(AUD_Sound* sequence, AUD_DistanceModel value);

/**
 * Retrieves the doppler factor of a sequence.
 * param sequence The sequence to get the doppler factor from.
 * return The doppler factor of the sequence.
 */
extern AUD_API float AUD_Sequence_getDopplerFactor(AUD_Sound* sequence);

/**
 * Sets the doppler factor of a sequence.
 * param sequence The sequence to set the doppler factor from.
 * param value The new doppler factor to set.
 */
extern AUD_API void AUD_Sequence_setDopplerFactor(AUD_Sound* sequence, float value);

/**
 * Retrieves the fps of a sequence.
 * param sequence The sequence to get the fps from.
 * return The fps of the sequence.
 */
extern AUD_API float AUD_Sequence_getFPS(AUD_Sound* sequence);

/**
 * Sets the fps of a sequence.
 * param sequence The sequence to set the fps from.
 * param value The new fps to set.
 */
extern AUD_API void AUD_Sequence_setFPS(AUD_Sound* sequence, float value);

/**
 * Retrieves the muted of a sequence.
 * param sequence The sequence to get the muted from.
 * return The muted of the sequence.
 */
extern AUD_API int AUD_Sequence_isMuted(AUD_Sound* sequence);

/**
 * Sets the muted of a sequence.
 * param sequence The sequence to set the muted from.
 * param value The new muted to set.
 */
extern AUD_API void AUD_Sequence_setMuted(AUD_Sound* sequence, int value);

/**
 * Retrieves the specs of a sequence.
 * param sequence The sequence to get the specs from.
 * return The specs of the sequence.
 */
extern AUD_API AUD_Specs AUD_Sequence_getSpecs(AUD_Sound* sequence);

/**
 * Sets the specs of a sequence.
 * param sequence The sequence to set the specs from.
 * param value The new specs to set.
 */
extern AUD_API void AUD_Sequence_setSpecs(AUD_Sound* sequence, AUD_Specs value);

/**
 * Retrieves the speed of sound of a sequence.
 * param sequence The sequence to get the speed of sound from.
 * return The speed of sound of the sequence.
 */
extern AUD_API float AUD_Sequence_getSpeedOfSound(AUD_Sound* sequence);

/**
 * Sets the speed of sound of a sequence.
 * param sequence The sequence to set the speed of sound from.
 * param value The new speed of sound to set.
 */
extern AUD_API void AUD_Sequence_setSpeedOfSound(AUD_Sound* sequence, float value);



/**
 * Moves the entry.
 * \param entry The sequenced entry.
 * \param begin The new start time.
 * \param end The new end time or a negative value if unknown.
 * \param skip How many seconds to skip at the beginning.
 */
extern AUD_API void AUD_SequenceEntry_move(AUD_SequenceEntry* entry, double begin, double end, double skip);

/**
 * Writes animation data to a sequenced entry.
 * \param entry The sequenced entry.
 * \param type The type of animation data.
 * \param frame The frame this data is for.
 * \param data The data to write.
 * \param animated Whether the attribute is animated.
 */
extern AUD_API void AUD_SequenceEntry_setAnimationData(AUD_SequenceEntry* entry, AUD_AnimateablePropertyType type, int frame, float* data, char animated);

/**
 * Retrieves the attenuation of a sequence_entry.
 * param sequence_entry The sequence_entry to get the attenuation from.
 * return The attenuation of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getAttenuation(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the attenuation of a sequence_entry.
 * param sequence_entry The sequence_entry to set the attenuation from.
 * param value The new attenuation to set.
 */
extern AUD_API void AUD_SequenceEntry_setAttenuation(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the cone angle inner of a sequence_entry.
 * param sequence_entry The sequence_entry to get the cone angle inner from.
 * return The cone angle inner of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getConeAngleInner(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the cone angle inner of a sequence_entry.
 * param sequence_entry The sequence_entry to set the cone angle inner from.
 * param value The new cone angle inner to set.
 */
extern AUD_API void AUD_SequenceEntry_setConeAngleInner(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the cone angle outer of a sequence_entry.
 * param sequence_entry The sequence_entry to get the cone angle outer from.
 * return The cone angle outer of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getConeAngleOuter(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the cone angle outer of a sequence_entry.
 * param sequence_entry The sequence_entry to set the cone angle outer from.
 * param value The new cone angle outer to set.
 */
extern AUD_API void AUD_SequenceEntry_setConeAngleOuter(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the cone volume outer of a sequence_entry.
 * param sequence_entry The sequence_entry to get the cone volume outer from.
 * return The cone volume outer of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getConeVolumeOuter(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the cone volume outer of a sequence_entry.
 * param sequence_entry The sequence_entry to set the cone volume outer from.
 * param value The new cone volume outer to set.
 */
extern AUD_API void AUD_SequenceEntry_setConeVolumeOuter(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the distance maximum of a sequence_entry.
 * param sequence_entry The sequence_entry to get the distance maximum from.
 * return The distance maximum of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getDistanceMaximum(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the distance maximum of a sequence_entry.
 * param sequence_entry The sequence_entry to set the distance maximum from.
 * param value The new distance maximum to set.
 */
extern AUD_API void AUD_SequenceEntry_setDistanceMaximum(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the distance reference of a sequence_entry.
 * param sequence_entry The sequence_entry to get the distance reference from.
 * return The distance reference of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getDistanceReference(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the distance reference of a sequence_entry.
 * param sequence_entry The sequence_entry to set the distance reference from.
 * param value The new distance reference to set.
 */
extern AUD_API void AUD_SequenceEntry_setDistanceReference(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the muted of a sequence_entry.
 * param sequence_entry The sequence_entry to get the muted from.
 * return The muted of the sequence_entry.
 */
extern AUD_API int AUD_SequenceEntry_isMuted(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the muted of a sequence_entry.
 * param sequence_entry The sequence_entry to set the muted from.
 * param value The new muted to set.
 */
extern AUD_API void AUD_SequenceEntry_setMuted(AUD_SequenceEntry* sequence_entry, int value);

/**
 * Retrieves the relative of a sequence_entry.
 * param sequence_entry The sequence_entry to get the relative from.
 * return The relative of the sequence_entry.
 */
extern AUD_API int AUD_SequenceEntry_isRelative(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the relative of a sequence_entry.
 * param sequence_entry The sequence_entry to set the relative from.
 * param value The new relative to set.
 */
extern AUD_API void AUD_SequenceEntry_setRelative(AUD_SequenceEntry* sequence_entry, int value);

/**
 * Retrieves the sound of a sequence_entry.
 * param sequence_entry The sequence_entry to get the sound from.
 * return The sound of the sequence_entry.
 */
extern AUD_API AUD_Sound* AUD_SequenceEntry_getSound(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the sound of a sequence_entry.
 * param sequence_entry The sequence_entry to set the sound from.
 * param value The new sound to set.
 */
extern AUD_API void AUD_SequenceEntry_setSound(AUD_SequenceEntry* sequence_entry, AUD_Sound* value);

/**
 * Retrieves the volume maximum of a sequence_entry.
 * param sequence_entry The sequence_entry to get the volume maximum from.
 * return The volume maximum of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getVolumeMaximum(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the volume maximum of a sequence_entry.
 * param sequence_entry The sequence_entry to set the volume maximum from.
 * param value The new volume maximum to set.
 */
extern AUD_API void AUD_SequenceEntry_setVolumeMaximum(AUD_SequenceEntry* sequence_entry, float value);

/**
 * Retrieves the volume minimum of a sequence_entry.
 * param sequence_entry The sequence_entry to get the volume minimum from.
 * return The volume minimum of the sequence_entry.
 */
extern AUD_API float AUD_SequenceEntry_getVolumeMinimum(AUD_SequenceEntry* sequence_entry);

/**
 * Sets the volume minimum of a sequence_entry.
 * param sequence_entry The sequence_entry to set the volume minimum from.
 * param value The new volume minimum to set.
 */
extern AUD_API void AUD_SequenceEntry_setVolumeMinimum(AUD_SequenceEntry* sequence_entry, float value);

#ifdef __cplusplus
}
#endif
