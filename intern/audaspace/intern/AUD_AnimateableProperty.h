/*
 * $Id$
 *
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


#ifndef AUD_ANIMATEABLEPROPERTY
#define AUD_ANIMATEABLEPROPERTY

#include "AUD_Buffer.h"

#include <pthread.h>

/**
 * This class saves animation data for float properties.
 */
class AUD_AnimateableProperty : private AUD_Buffer
{
private:
	/// The count of floats for a single property.
	const int m_count;

	/// Whether the property is animated or not.
	bool m_isAnimated;

	/// The mutex for locking.
	pthread_mutex_t m_mutex;

	/// Whether the property has been changed.
	bool m_changed;

	// hide copy constructor and operator=
	AUD_AnimateableProperty(const AUD_AnimateableProperty&);
	AUD_AnimateableProperty& operator=(const AUD_AnimateableProperty&);

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
	void lock();

	/**
	 * Unlocks the previously locked property.
	 */
	void unlock();

	void write(const float* data);

	void write(const float* data, int position, int count);

	void read(float position, float* out);

	bool isAnimated() const;

	bool hasChanged();
};

#endif //AUD_ANIMATEABLEPROPERTY
