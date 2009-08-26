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

#ifndef AUD_MIXERFACTORY
#define AUD_MIXERFACTORY

#include "AUD_IFactory.h"

/**
 * This factory is a base class for all mixer factories.
 */
class AUD_MixerFactory : public AUD_IFactory
{
protected:
	/**
	 * The reader that should be mixed later.
	 */
	AUD_IReader* m_reader;

	/**
	 * If there is no reader it is created out of this factory.
	 */
	AUD_IFactory* m_factory;

	/**
	 * The target specification for resampling.
	 */
	AUD_Specs m_specs;

	/**
	 * Returns the reader created out of the factory or taken from m_reader.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader to mix, or NULL if there is no reader or factory.
	 */
	AUD_IReader* getReader();

public:
	/**
	 * Creates a new factory.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 */
	AUD_MixerFactory(AUD_IReader* reader, AUD_Specs specs);

	/**
	 * Creates a new factory.
	 * \param factory The factory to create the readers to mix out of.
	 * \param specs The target specification.
	 */
	AUD_MixerFactory(AUD_IFactory* factory, AUD_Specs specs);

	/**
	 * Creates a new factory.
	 * \param specs The target specification.
	 */
	AUD_MixerFactory(AUD_Specs specs);

	/**
	 * Destroys the resampling factory.
	 */
	virtual ~AUD_MixerFactory();

	/**
	 * Returns the target specification for resampling.
	 */
	AUD_Specs getSpecs();

	/**
	 * Sets the target specification for resampling.
	 * \param specs The specification.
	 */
	void setSpecs(AUD_Specs specs);

	/**
	 * Sets the reader for resampling.
	 * If there has already been a reader, it will be deleted.
	 * \param reader The reader that should be used as source for resampling.
	 */
	void setReader(AUD_IReader* reader);

	/**
	 * Sets the factory for resampling.
	 * \param factory The factory that should be used as source for resampling.
	 */
	void setFactory(AUD_IFactory* factory);

	/**
	 * Returns the saved factory.
	 * \return The factory or NULL if there has no factory been saved.
	 */
	AUD_IFactory* getFactory();
};

#endif //AUD_MIXERFACTORY
