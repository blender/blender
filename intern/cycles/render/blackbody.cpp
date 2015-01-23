/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2013, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "blackbody.h"
#include "util_color.h"
#include "util_math.h"

#include "kernel_types.h"

CCL_NAMESPACE_BEGIN

vector<float> blackbody_table_build()
{
	/* quoted from OSLs opcolor.cpp
	In order to speed up the blackbody computation, we have a table
	storing the precomputed BB values for a range of temperatures.  Less
	than BB_DRAPER always returns 0.  Greater than BB_MAX_TABLE_RANGE
	does the full computation, we think it'll be rare to inquire higher
	temperatures.

	Since the bb function is so nonlinear, we actually space the table
	entries nonlinearly, with the relationship between the table index i
	and the temperature T as follows:
	i = ((T-Draper)/spacing)^(1/xpower)
	T = pow(i, xpower) * spacing + Draper
	And furthermore, we store in the table the true value raised ^(1/5).
	I tuned this a bit, and with the current values we can have all
	blackbody results accurate to within 0.1% with a table size of 317
	(about 5 KB of data).
	*/

	const float cie_colour_match[81][3] = {
		{0.0014f,0.0000f,0.0065f}, {0.0022f,0.0001f,0.0105f}, {0.0042f,0.0001f,0.0201f},
		{0.0076f,0.0002f,0.0362f}, {0.0143f,0.0004f,0.0679f}, {0.0232f,0.0006f,0.1102f},
		{0.0435f,0.0012f,0.2074f}, {0.0776f,0.0022f,0.3713f}, {0.1344f,0.0040f,0.6456f},
		{0.2148f,0.0073f,1.0391f}, {0.2839f,0.0116f,1.3856f}, {0.3285f,0.0168f,1.6230f},
		{0.3483f,0.0230f,1.7471f}, {0.3481f,0.0298f,1.7826f}, {0.3362f,0.0380f,1.7721f},
		{0.3187f,0.0480f,1.7441f}, {0.2908f,0.0600f,1.6692f}, {0.2511f,0.0739f,1.5281f},
		{0.1954f,0.0910f,1.2876f}, {0.1421f,0.1126f,1.0419f}, {0.0956f,0.1390f,0.8130f},
		{0.0580f,0.1693f,0.6162f}, {0.0320f,0.2080f,0.4652f}, {0.0147f,0.2586f,0.3533f},
		{0.0049f,0.3230f,0.2720f}, {0.0024f,0.4073f,0.2123f}, {0.0093f,0.5030f,0.1582f},
		{0.0291f,0.6082f,0.1117f}, {0.0633f,0.7100f,0.0782f}, {0.1096f,0.7932f,0.0573f},
		{0.1655f,0.8620f,0.0422f}, {0.2257f,0.9149f,0.0298f}, {0.2904f,0.9540f,0.0203f},
		{0.3597f,0.9803f,0.0134f}, {0.4334f,0.9950f,0.0087f}, {0.5121f,1.0000f,0.0057f},
		{0.5945f,0.9950f,0.0039f}, {0.6784f,0.9786f,0.0027f}, {0.7621f,0.9520f,0.0021f},
		{0.8425f,0.9154f,0.0018f}, {0.9163f,0.8700f,0.0017f}, {0.9786f,0.8163f,0.0014f},
		{1.0263f,0.7570f,0.0011f}, {1.0567f,0.6949f,0.0010f}, {1.0622f,0.6310f,0.0008f},
		{1.0456f,0.5668f,0.0006f}, {1.0026f,0.5030f,0.0003f}, {0.9384f,0.4412f,0.0002f},
		{0.8544f,0.3810f,0.0002f}, {0.7514f,0.3210f,0.0001f}, {0.6424f,0.2650f,0.0000f},
		{0.5419f,0.2170f,0.0000f}, {0.4479f,0.1750f,0.0000f}, {0.3608f,0.1382f,0.0000f},
		{0.2835f,0.1070f,0.0000f}, {0.2187f,0.0816f,0.0000f}, {0.1649f,0.0610f,0.0000f},
		{0.1212f,0.0446f,0.0000f}, {0.0874f,0.0320f,0.0000f}, {0.0636f,0.0232f,0.0000f},
		{0.0468f,0.0170f,0.0000f}, {0.0329f,0.0119f,0.0000f}, {0.0227f,0.0082f,0.0000f},
		{0.0158f,0.0057f,0.0000f}, {0.0114f,0.0041f,0.0000f}, {0.0081f,0.0029f,0.0000f},
		{0.0058f,0.0021f,0.0000f}, {0.0041f,0.0015f,0.0000f}, {0.0029f,0.0010f,0.0000f},
		{0.0020f,0.0007f,0.0000f}, {0.0014f,0.0005f,0.0000f}, {0.0010f,0.0004f,0.0000f},
		{0.0007f,0.0002f,0.0000f}, {0.0005f,0.0002f,0.0000f}, {0.0003f,0.0001f,0.0000f},
		{0.0002f,0.0001f,0.0000f}, {0.0002f,0.0001f,0.0000f}, {0.0001f,0.0000f,0.0000f},
		{0.0001f,0.0000f,0.0000f}, {0.0001f,0.0000f,0.0000f}, {0.0000f,0.0000f,0.0000f}
	};

	const double c1 = 3.74183e-16; // 2*pi*h*c^2, W*m^2
	const double c2 = 1.4388e-2;   // h*c/k, m*K
								   // h is Planck's const, k is Boltzmann's
	const float dlambda = 5.0f * 1e-9f;  // in meters

	/* Blackbody table from 800 to 12k Kelvin (319 entries (317+2 offset) * 3) */
	vector<float> blackbody_table(956);

	float X, Y, Z;

	/* ToDo: bring this back to what OSL does with the lastTemperature limit ? */
	for (int i = 0;  i <= 317;  ++i) {
		double Temperature = pow((double)i, (double)BB_TABLE_XPOWER) * (double)BB_TABLE_SPACING + (double)BB_DRAPER;
		X = 0;
		Y = 0;
		Z = 0;

		/* from OSL "spectrum_to_XYZ" */
		for (int n = 0; n < 81; ++n) {
			float lambda = 380.0f + 5.0f * n;
			double wlm = lambda * 1e-9f;   // Wavelength in meters
			// N.B. spec_intens returns result in W/m^2 but it's a differential,
			// needs to be scaled by dlambda!
			float spec_intens = float((c1 * pow(wlm, -5.0)) / (exp(c2 / (wlm * Temperature)) -1.0));
			float Me = spec_intens * dlambda;

			X += Me * cie_colour_match[n][0];
			Y += Me * cie_colour_match[n][1];
			Z += Me * cie_colour_match[n][2];
		}
		
		/* Convert from xyz color space */
		float3 col = xyz_to_rgb(X, Y, Z);

		/* Clamp to zero if values are smaller */
		col = max(col, make_float3(0.0f, 0.0f, 0.0f));

		col.x = powf(col.x, 1.0f / BB_TABLE_YPOWER);
		col.y = powf(col.y, 1.0f / BB_TABLE_YPOWER);
		col.z = powf(col.z, 1.0f / BB_TABLE_YPOWER);

		/* Store in table in RRRGGGBBB format */
		blackbody_table[i] = col.x;
		blackbody_table[i+319*1] = col.y;
		blackbody_table[i+319*2] = col.z;	
	}

	return blackbody_table;
}
CCL_NAMESPACE_END
