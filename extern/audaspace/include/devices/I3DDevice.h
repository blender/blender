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
 * @file I3DDevice.h
 * @ingroup devices
 * Defines the I3DDevice interface as well as the different distance models.
 */

#include "util/Math3D.h"

AUD_NAMESPACE_BEGIN

/**
 * Possible distance models for the 3D device.
 *
 * The distance models supported are the same as documented in the [OpenAL Specification](http://openal.org/).
 */
enum DistanceModel
{
	DISTANCE_MODEL_INVALID = 0,
	DISTANCE_MODEL_INVERSE,
	DISTANCE_MODEL_INVERSE_CLAMPED,
	DISTANCE_MODEL_LINEAR,
	DISTANCE_MODEL_LINEAR_CLAMPED,
	DISTANCE_MODEL_EXPONENT,
	DISTANCE_MODEL_EXPONENT_CLAMPED
};

/**
 * @interface I3DDevice
 * The I3DDevice interface represents an output device for 3D sound.
 *
 * The interface has been modelled after the OpenAL 1.1 API,
 * see the [OpenAL Specification](http://openal.org/) for lots of details.
 */
class AUD_API I3DDevice
{
public:
	/**
	 * Retrieves the listener location.
	 * \return The listener location.
	 */
	virtual Vector3 getListenerLocation() const=0;

	/**
	 * Sets the listener location.
	 * \param location The new location.
	 * \note The location is not updated with the velocity and
	 *       remains constant until the next call of this method.
	 */
	virtual void setListenerLocation(const Vector3& location)=0;

	/**
	 * Retrieves the listener velocity.
	 * \return The listener velocity.
	 */
	virtual Vector3 getListenerVelocity() const=0;

	/**
	 * Sets the listener velocity.
	 * \param velocity The new velocity.
	 * \note This velocity does not change the position of the listener
	 *       over time, it is simply used for the calculation of the doppler effect.
	 */
	virtual void setListenerVelocity(const Vector3& velocity)=0;

	/**
	 * Retrieves the listener orientation.
	 * \return The listener orientation as quaternion.
	 */
	virtual Quaternion getListenerOrientation() const=0;

	/**
	 * Sets the listener orientation.
	 * \param orientation The new orientation as quaternion.
	 * \note The coordinate system used is right handed and the listener
	 * by default is oriented looking in the negative z direction with the
	 * positive y axis as up direction.
	 */
	virtual void setListenerOrientation(const Quaternion& orientation)=0;


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
	virtual DistanceModel getDistanceModel() const=0;

	/**
	 * Sets the distance model.
	 * \param model distance model.
	 */
	virtual void setDistanceModel(DistanceModel model)=0;
};

AUD_NAMESPACE_END
