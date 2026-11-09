/*******************************************************************************
 * Copyright 2026 Mayank
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
 * @file Compressor.h
 * @ingroup fx
 * The Compressor class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

class AUD_API Compressor : public Effect
{
private:
	std::shared_ptr<ISound> m_sidechain; /* The sidechain sound whose amplitude determines the amount of compression (usually the output of a peak or RMS detector) */
	float m_threshold;                   /* Threshold ratio (0.0 to 1.0) over which to start compression */
	float m_ratio;                       /* Compression ratio (e.g., 4.0 for 4:1) */
	float m_attack;                      /* Attack time in seconds */
	float m_release;                     /* Release time in seconds */
	float m_makeupGain;                  /* Output gain ratio to apply after compression (1.0 for no gain) */
	float m_knee;                        /* Knee ratio (1.0 for hard knee, higher for soft knee) */
public:
	/**
	 * Creates a new compressor sound.
	 * \param sound The input sound to be compressed.
	 * \param sidechain The sidechain sound whose amplitude determines the amount of compression (usually the output of a peak or RMS detector).
	 * \param threshold The threshold ratio (0.0 to 1.0) over which to start compression.
	 * \param ratio The compression ratio (e.g., 4.0 for 4:1).
	 * \param attack The attack time in seconds.
	 * \param release The release time in seconds.
	 * \param makeupGain The output gain ratio to apply after compression (1.0 for no gain).
	 * \param knee The knee ratio (1.0 for hard knee, higher for soft knee).
	 */
	Compressor(std::shared_ptr<ISound> sound, std::shared_ptr<ISound> sidechain, float threshold, float ratio, float attack, float release, float makeupGain, float knee);

	float getThreshold() const;

	float getRatio() const;

	float getAttack() const;

	float getRelease() const;

	float getMakeupGain() const;

	float getKnee() const;

	std::shared_ptr<ISound> getSidechain() const;

	virtual std::shared_ptr<IReader> createReader() override;
};

AUD_NAMESPACE_END
