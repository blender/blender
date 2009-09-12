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

/**
 * This class represents an output device for 3D sound.
 * Whether a normal device supports this or not can be checked with the
 * AUD_CAPS_3D_DEVICE capability.
 */
class AUD_I3DDevice
{
public:
	/**
	 * Plays a 3D sound source.
	 * \param factory The factory to create the reader for the sound source.
	 * \param keep When keep is true the sound source will not be deleted but
	 *             set to paused when its end has been reached.
	 * \return Returns a handle with which the playback can be controlled.
	 *         This is NULL if the sound couldn't be played back.
	 * \exception AUD_Exception Thrown if there's an unexpected (from the
	 *            device side) error during creation of the reader.
	 * \note The factory must provide a mono (single channel) source otherwise
	 *       the sound is played back normally.
	 */
	virtual AUD_Handle* play3D(AUD_IFactory* factory, bool keep = false)=0;

	/**
	 * Updates a listeners 3D data.
	 * \param data The 3D data.
	 * \return Whether the action succeeded.
	 */
	virtual bool updateListener(AUD_3DData &data)=0;

	/**
	 * Sets a 3D device setting.
	 * \param setting The setting type.
	 * \param value The new setting value.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSetting(AUD_3DSetting setting, float value)=0;

	/**
	 * Retrieves a 3D device setting.
	 * \param setting The setting type.
	 * \return The setting value.
	 */
	virtual float getSetting(AUD_3DSetting setting)=0;

	/**
	 * Updates a listeners 3D data.
	 * \param handle The source handle.
	 * \param data The 3D data.
	 * \return Whether the action succeeded.
	 */
	virtual bool updateSource(AUD_Handle* handle, AUD_3DData &data)=0;

	/**
	 * Sets a 3D source setting.
	 * \param handle The source handle.
	 * \param setting The setting type.
	 * \param value The new setting value.
	 * \return Whether the action succeeded.
	 */
	virtual bool setSourceSetting(AUD_Handle* handle,
								  AUD_3DSourceSetting setting, float value)=0;

	/**
	 * Retrieves a 3D source setting.
	 * \param handle The source handle.
	 * \param setting The setting type.
	 * \return The setting value.
	 */
	virtual float getSourceSetting(AUD_Handle* handle,
								   AUD_3DSourceSetting setting)=0;
};

#endif //AUD_I3DDEVICE
