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

/** \file audaspace/intern/AUD_I3DHandle.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_I3DHANDLE_H__
#define __AUD_I3DHANDLE_H__

#include "AUD_Space.h"
#include "AUD_3DMath.h"

/**
 * This class represents a playback handle for 3D sources.
 */
class AUD_I3DHandle
{
public:
	/**
	 * Destroys the handle.
	 */
	virtual ~AUD_I3DHandle() {}

	/**
	 * Retrieves the location of a source.
	 * \return The location.
	 */
	virtual AUD_Vector3 getSourceLocation()=0;

	/**
	 * Sets the location of a source.
	 * \param location The new location.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceLocation(const AUD_Vector3& location)=0;

	/**
	 * Retrieves the velocity of a source.
	 * \return The velocity.
	 */
	virtual AUD_Vector3 getSourceVelocity()=0;

	/**
	 * Sets the velocity of a source.
	 * \param velocity The new velocity.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceVelocity(const AUD_Vector3& velocity)=0;

	/**
	 * Retrieves the orientation of a source.
	 * \return The orientation as quaternion.
	 */
	virtual AUD_Quaternion getSourceOrientation()=0;

	/**
	 * Sets the orientation of a source.
	 * \param orientation The new orientation as quaternion.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceOrientation(const AUD_Quaternion& orientation)=0;


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
	 * Retrieves the outer angle of the cone of a source.
	 * \return The outer angle of the cone.
	 */
	virtual float getConeAngleOuter()=0;

	/**
	 * Sets the outer angle of the cone of a source.
	 * \param angle The new outer angle of the cone.
	 * \return Whether the action succeeded.
	 */
	virtual bool setConeAngleOuter(float angle)=0;

	/**
	 * Retrieves the inner angle of the cone of a source.
	 * \return The inner angle of the cone.
	 */
	virtual float getConeAngleInner()=0;

	/**
	 * Sets the inner angle of the cone of a source.
	 * \param angle The new inner angle of the cone.
	 * \return Whether the action succeeded.
	 */
	virtual bool setConeAngleInner(float angle)=0;

	/**
	 * Retrieves the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \return The outer volume of the cone.
	 */
	virtual float getConeVolumeOuter()=0;

	/**
	 * Sets the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \param volume The new outer volume of the cone.
	 * \return Whether the action succeeded.
	 */
	virtual bool setConeVolumeOuter(float volume)=0;
};

#endif //__AUD_I3DHANDLE_H__
