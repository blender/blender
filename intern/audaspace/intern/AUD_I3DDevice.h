/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_I3DDevice.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_I3DDEVICE_H__
#define __AUD_I3DDEVICE_H__

#include "AUD_Space.h"
#include "AUD_3DMath.h"

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
};

#endif //__AUD_I3DDEVICE_H__
