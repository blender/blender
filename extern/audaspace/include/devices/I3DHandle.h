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

/**
 * @file I3DHandle.h
 * @ingroup devices
 * The I3DHandle interface.
 */

#include "util/Math3D.h"

AUD_NAMESPACE_BEGIN

/**
 * @interface I3DHandle
 * The I3DHandle interface represents a playback handle for 3D sources.
 * If the playback IDevice class also implements the I3DDevice interface
 * then all playback IHandle instances also implement this interface.
 *
 * The interface has been modelled after the OpenAL 1.1 API,
 * see the [OpenAL Specification](http://openal.org/) for lots of details.
 */
class AUD_API I3DHandle
{
public:
	/**
	 * Destroys the handle.
	 */
	virtual ~I3DHandle() {}

	/**
	 * Retrieves the location of the source.
	 * \return The location.
	 */
	virtual Vector3 getLocation()=0;

	/**
	 * Sets the location of the source.
	 * \param location The new location.
	 * \return Whether the action succeeded.
	 * \note The location is not updated with the velocity and
	 *       remains constant until the next call of this method.
	 */
	virtual bool setLocation(const Vector3& location)=0;

	/**
	 * Retrieves the velocity of the source.
	 * \return The velocity.
	 */
	virtual Vector3 getVelocity()=0;

	/**
	 * Sets the velocity of the source.
	 * \param velocity The new velocity.
	 * \return Whether the action succeeded.
	 * \note This velocity does not change the position of the listener
	 *       over time, it is simply used for the calculation of the doppler effect.
	 */
	virtual bool setVelocity(const Vector3& velocity)=0;

	/**
	 * Retrieves the orientation of the source.
	 * \return The orientation as quaternion.
	 */
	virtual Quaternion getOrientation()=0;

	/**
	 * Sets the orientation of the source.
	 * \param orientation The new orientation as quaternion.
	 * \return Whether the action succeeded.
	 * \note The coordinate system used is right handed and the source
	 * by default is oriented looking in the negative z direction with the
	 * positive y axis as up direction.
	 * \note This setting currently only affects sounds with non-default cone settings.
	 */
	virtual bool setOrientation(const Quaternion& orientation)=0;


	/**
	 * Checks whether the source location, velocity and orientation are relative
	 * to the listener.
	 * \return Whether the source is relative.
	 */
	virtual bool isRelative()=0;

	/**
	 * Sets whether the source location, velocity and orientation are relative
	 * to the listener.
	 * \param relative Whether the source is relative.
	 * \return Whether the action succeeded.
	 * \note The default value is true as this setting is used to play sounds ordinarily without 3D.
	 */
	virtual bool setRelative(bool relative)=0;

	/**
	 * Retrieves the maximum volume of a source.
	 * \return The maximum volume.
	 */
	virtual float getVolumeMaximum()=0;

	/**
	 * Sets the maximum volume of a source.
	 * \param volume The new maximum volume.
	 * \return Whether the action succeeded.
	 */
	virtual bool setVolumeMaximum(float volume)=0;

	/**
	 * Retrieves the minimum volume of a source.
	 * \return The minimum volume.
	 */
	virtual float getVolumeMinimum()=0;

	/**
	 * Sets the minimum volume of a source.
	 * \param volume The new minimum volume.
	 * \return Whether the action succeeded.
	 */
	virtual bool setVolumeMinimum(float volume)=0;

	/**
	 * Retrieves the maximum distance of a source.
	 * If a source is further away from the reader than this distance, the
	 * volume will automatically be set to 0.
	 * \return The maximum distance.
	 */
	virtual float getDistanceMaximum()=0;

	/**
	 * Sets the maximum distance of a source.
	 * If a source is further away from the reader than this distance, the
	 * volume will automatically be set to 0.
	 * \param distance The new maximum distance.
	 * \return Whether the action succeeded.
	 */
	virtual bool setDistanceMaximum(float distance)=0;

	/**
	 * Retrieves the reference distance of a source.
	 * \return The reference distance.
	 */
	virtual float getDistanceReference()=0;

	/**
	 * Sets the reference distance of a source.
	 * \param distance The new reference distance.
	 * \return Whether the action succeeded.
	 */
	virtual bool setDistanceReference(float distance)=0;

	/**
	 * Retrieves the attenuation of a source.
	 * \return The attenuation.
	 */
	virtual float getAttenuation()=0;

	/**
	 * Sets the attenuation of a source.
	 * This value is used for distance calculation.
	 * \param factor The new attenuation.
	 * \return Whether the action succeeded.
	 */
	virtual bool setAttenuation(float factor)=0;

	/**
	 * Retrieves the outer opening angle of the cone of a source.
	 * \return The outer angle of the cone.
	 * \note This angle is defined in degrees.
	 */
	virtual float getConeAngleOuter()=0;

	/**
	 * Sets the outer opening angle of the cone of a source.
	 * \param angle The new outer angle of the cone.
	 * \return Whether the action succeeded.
	 * \note This angle is defined in degrees.
	 */
	virtual bool setConeAngleOuter(float angle)=0;

	/**
	 * Retrieves the inner opening angle of the cone of a source.
	 * The volume inside this cone is unaltered.
	 * \return The inner angle of the cone.
	 * \note This angle is defined in degrees.
	 */
	virtual float getConeAngleInner()=0;

	/**
	 * Sets the inner opening angle of the cone of a source.
	 * The volume inside this cone is unaltered.
	 * \param angle The new inner angle of the cone.
	 * \return Whether the action succeeded.
	 * \note This angle is defined in degrees.
	 */
	virtual bool setConeAngleInner(float angle)=0;

	/**
	 * Retrieves the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \return The outer volume of the cone.
	 * \note The general volume of the handle still applies on top of this.
	 */
	virtual float getConeVolumeOuter()=0;

	/**
	 * Sets the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \param volume The new outer volume of the cone.
	 * \return Whether the action succeeded.
	 * \note The general volume of the handle still applies on top of this.
	 */
	virtual bool setConeVolumeOuter(float volume)=0;
};

AUD_NAMESPACE_END
