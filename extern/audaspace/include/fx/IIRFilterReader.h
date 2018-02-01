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
 * @file IIRFilterReader.h
 * @ingroup fx
 * The IIRFilterReader class.
 */

#include "fx/BaseIIRFilterReader.h"

#include <vector>

AUD_NAMESPACE_BEGIN

/**
 * This class is for infinite impulse response filters with simple coefficients.
 */
class AUD_API IIRFilterReader : public BaseIIRFilterReader
{
private:
	/**
	 * Output filter coefficients.
	 */
	std::vector<float> m_a;

	/**
	 * Input filter coefficients.
	 */
	std::vector<float> m_b;

	// delete copy constructor and operator=
	IIRFilterReader(const IIRFilterReader&) = delete;
	IIRFilterReader& operator=(const IIRFilterReader&) = delete;

public:
	/**
	 * Creates a new IIR filter reader.
	 * \param reader The reader to read from.
	 * \param b The input filter coefficients.
	 * \param a The output filter coefficients.
	 */
	IIRFilterReader(std::shared_ptr<IReader> reader, const std::vector<float>& b, const std::vector<float>& a);

	virtual sample_t filter();

	/**
	 * Sets new filter coefficients.
	 * @param b The input filter coefficients.
	 * @param a The output filter coefficients.
	 */
	void setCoefficients(const std::vector<float>& b, const std::vector<float>& a);
};

AUD_NAMESPACE_END
