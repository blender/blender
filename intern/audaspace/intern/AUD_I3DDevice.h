/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_I3DDEVICE
#define AUD_I3DDEVICE

#include "AUD_Space.h"
#include "AUD_3DMath.h"

struct AUD_Handle;

/**
 * This class represents an output device for 3D sound.
 */
class AUD_I3DDevice
{
public:
	/**
	 * Retrieves the listener location.
	 * \return The listener location.
	 */
	virtual AUD_Vector3 getListenerLocation() const=0;

	/**
	 * Sets the listener location.
	 * \param location The new location.
	 */
	virtual void setListenerLocation(const AUD_Vector3& location)=0;

	/**
	 * Retrieves the listener velocity.
	 * \return The listener velocity.
	 */
	virtual AUD_Vector3 getListenerVelocity() const=0;

	/**
	 * Sets the listener velocity.
	 * \param velocity The new velocity.
	 */
	virtual void setListenerVelocity(const AUD_Vector3& velocity)=0;

	/**
	 * Retrieves the listener orientation.
	 * \return The listener orientation as quaternion.
	 */
	virtual AUD_Quaternion getListenerOrientation() const=0;

	/**
	 * Sets the listener orientation.
	 * \param orientation The new orientation as quaternion.
	 */
	virtual void setListenerOrientation(const AUD_Quaternion& orientation)=0;


	/**
	 * Retrieves the speed of sound.
	 * This value is needed for doppler effect calculation.
	 * \return The speed of sound.
	 */
	virtual float getSpeedOfSound() const=0;

	/**
	 * Sets the speed of sound.
	 * This value is needed for doppler effect calculation.
	 * \param speed The new speed of sound.
	 */
	virtual void setSpeedOfSound(float speed)=0;

	/**
	 * Retrieves the doppler factor.
	 * This value is a scaling factor for the velocity vectors of sources and
	 * listener which is used while calculating the doppler effect.
	 * \return The doppler factor.
	 */
	virtual float getDopplerFactor() const=0;

	/**
	 * Sets the doppler factor.
	 * This value is a scaling factor for the velocity vectors of sources and
	 * listener which is used while calculating the doppler effect.
	 * \param factor The new doppler factor.
	 */
	virtual void setDopplerFactor(float factor)=0;

	/**
	 * Retrieves the distance model.
	 * \return The distance model.
	 */
	virtual AUD_DistanceModel getDistanceModel() const=0;

	/**
	 * Sets the distance model.
	 * \param model distance model.
	 */
	virtual void setDistanceModel(AUD_DistanceModel model)=0;



	/**
	 * Retrieves the location of a source.
	 * \param handle The handle of the source.
	 * \return The location.
	 */
	virtual AUD_Vector3 getSourceLocation(AUD_Handle* handle)=0;

	/**
	 * Sets the location of a source.
	 * \param handle The handle of the source.
	 * \param location The new location.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceLocation(AUD_Handle* handle, const AUD_Vector3& location)=0;

	/**
	 * Retrieves the velocity of a source.
	 * \param handle The handle of the source.
	 * \return The velocity.
	 */
	virtual AUD_Vector3 getSourceVelocity(AUD_Handle* handle)=0;

	/**
	 * Sets the velocity of a source.
	 * \param handle The handle of the source.
	 * \param velocity The new velocity.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceVelocity(AUD_Handle* handle, const AUD_Vector3& velocity)=0;

	/**
	 * Retrieves the orientation of a source.
	 * \param handle The handle of the source.
	 * \return The orientation as quaternion.
	 */
	virtual AUD_Quaternion getSourceOrientation(AUD_Handle* handle)=0;

	/**
	 * Sets the orientation of a source.
	 * \param handle The handle of the source.
	 * \param orientation The new orientation as quaternion.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceOrientation(AUD_Handle* handle, const AUD_Quaternion& orientation)=0;


	/**
	 * Checks whether the source location, velocity and orientation are relative
	 * to the listener.
	 * \param handle The handle of the source.
	 * \return Whether the source is relative.
	 */
	virtual bool isRelative(AUD_Handle* handle)=0;

	/**
	 * Sets whether the source location, velocity and orientation are relative
	 * to the listener.
	 * \param handle The handle of the source.
	 * \param relative Whether the source is relative.
	 * \return Whether the action succeeded.
	 */
	virtual bool setRelative(AUD_Handle* handle, bool relative)=0;

	/**
	 * Retrieves the maximum volume of a source.
	 * \param handle The handle of the source.
	 * \return The maximum volume.
	 */
	virtual float getVolumeMaximum(AUD_Handle* handle)=0;

	/**
	 * Sets the maximum volume of a source.
	 * \param handle The handle of the source.
	 * \param volume The new maximum volume.
	 * \return Whether the action succeeded.
	 */
	virtual bool setVolumeMaximum(AUD_Handle* handle, float volume)=0;

	/**
	 * Retrieves the minimum volume of a source.
	 * \param handle The handle of the source.
	 * \return The minimum volume.
	 */
	virtual float getVolumeMinimum(AUD_Handle* handle)=0;

	/**
	 * Sets the minimum volume of a source.
	 * \param handle The handle of the source.
	 * \param volume The new minimum volume.
	 * \return Whether the action succeeded.
	 */
	virtual bool setVolumeMinimum(AUD_Handle* handle, float volume)=0;

	/**
	 * Retrieves the maximum distance of a source.
	 * If a source is further away from the reader than this distance, the
	 * volume will automatically be set to 0.
	 * \param handle The handle of the source.
	 * \return The maximum distance.
	 */
	virtual float getDistanceMaximum(AUD_Handle* handle)=0;

	/**
	 * Sets the maximum distance of a source.
	 * If a source is further away from the reader than this distance, the
	 * volume will automatically be set to 0.
	 * \param handle The handle of the source.
	 * \param distance The new maximum distance.
	 * \return Whether the action succeeded.
	 */
	virtual bool setDistanceMaximum(AUD_Handle* handle, float distance)=0;

	/**
	 * Retrieves the reference distance of a source.
	 * \param handle The handle of the source.
	 * \return The reference distance.
	 */
	virtual float getDistanceReference(AUD_Handle* handle)=0;

	/**
	 * Sets the reference distance of a source.
	 * \param handle The handle of the source.
	 * \param distance The new reference distance.
	 * \return Whether the action succeeded.
	 */
	virtual bool setDistanceReference(AUD_Handle* handle, float distance)=0;

	/**
	 * Retrieves the attenuation of a source.
	 * \param handle The handle of the source.
	 * \return The attenuation.
	 */
	virtual float getAttenuation(AUD_Handle* handle)=0;

	/**
	 * Sets the attenuation of a source.
	 * This value is used for distance calculation.
	 * \param handle The handle of the source.
	 * \param factor The new attenuation.
	 * \return Whether the action succeeded.
	 */
	virtual bool setAttenuation(AUD_Handle* handle, float factor)=0;

	/**
	 * Retrieves the outer angle of the cone of a source.
	 * \param handle The handle of the source.
	 * \return The outer angle of the cone.
	 */
	virtual float getConeAngleOuter(AUD_Handle* handle)=0;

	/**
	 * Sets the outer angle of the cone of a source.
	 * \param handle The handle of the source.
	 * \param angle The new outer angle of the cone.
	 * \return Whether the action succeeded.
	 */
	virtual bool setConeAngleOuter(AUD_Handle* handle, float angle)=0;

	/**
	 * Retrieves the inner angle of the cone of a source.
	 * \param handle The handle of the source.
	 * \return The inner angle of the cone.
	 */
	virtual float getConeAngleInner(AUD_Handle* handle)=0;

	/**
	 * Sets the inner angle of the cone of a source.
	 * \param handle The handle of the source.
	 * \param angle The new inner angle of the cone.
	 * \return Whether the action succeeded.
	 */
	virtual bool setConeAngleInner(AUD_Handle* handle, float angle)=0;

	/**
	 * Retrieves the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \param handle The handle of the source.
	 * \return The outer volume of the cone.
	 */
	virtual float getConeVolumeOuter(AUD_Handle* handle)=0;

	/**
	 * Sets the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \param handle The handle of the source.
	 * \param volume The new outer volume of the cone.
	 * \return Whether the action succeeded.
	 */
	virtual bool setConeVolumeOuter(AUD_Handle* handle, float volume)=0;
};

#endif //AUD_I3DDEVICE
