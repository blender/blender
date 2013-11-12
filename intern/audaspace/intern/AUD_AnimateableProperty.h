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

/** \file audaspace/intern/AUD_AnimateableProperty.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_ANIMATEABLEPROPERTY_H__
#define __AUD_ANIMATEABLEPROPERTY_H__

#include "AUD_Buffer.h"
#include "AUD_ILockable.h"

#include <pthread.h>
#include <list>

/**
 * This class saves animation data for float properties.
 */
class AUD_AnimateableProperty : private AUD_Buffer, public AUD_ILockable
{
private:
	struct Unknown {
		int start;
		int end;

		Unknown(int start, int end) :
			start(start), end(end) {}
	};

	/// The count of floats for a single property.
	const int m_count;

	/// Whether the property is animated or not.
	bool m_isAnimated;

	/// The mutex for locking.
	pthread_mutex_t m_mutex;

	/// The list of unknown buffer areas.
	std::list<Unknown> m_unknown;

	// hide copy constructor and operator=
	AUD_AnimateableProperty(const AUD_AnimateableProperty&);
	AUD_AnimateableProperty& operator=(const AUD_AnimateableProperty&);

	void updateUnknownCache(int start, int end);

public:
	/**
	 * Creates a new animateable property.
	 * \param count The count of floats for a single property.
	 */
	AUD_AnimateableProperty(int count = 1);

	/**
	 * Destroys the animateable property.
	 */
	~AUD_AnimateableProperty();

	/**
	 * Locks the property.
	 */
	virtual void lock();

	/**
	 * Unlocks the previously locked property.
	 */
	virtual void unlock();

	/**
	 * Writes the properties value and marks it non-animated.
	 * \param data The new value.
	 */
	void write(const float* data);

	/**
	 * Writes the properties value and marks it animated.
	 * \param data The new value.
	 * \param position The position in the animation in frames.
	 * \param count The count of frames to write.
	 */
	void write(const float* data, int position, int count);

	/**
	 * Reads the properties value.
	 * \param position The position in the animation in frames.
	 * \param[out] out Where to write the value to.
	 */
	void read(float position, float* out);

	/**
	 * Returns whether the property is animated.
	 * \return Whether the property is animated.
	 */
	bool isAnimated() const;
};

#endif //__AUD_ANIMATEABLEPROPERTY_H__
