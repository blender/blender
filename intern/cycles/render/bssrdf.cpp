/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "bssrdf.h"

#include "util_algorithm.h"
#include "util_math.h"
#include "util_types.h"

#include "kernel_types.h"
#include "kernel_montecarlo.h"

#include "closure/bsdf_diffuse.h"
#include "closure/bssrdf.h"

CCL_NAMESPACE_BEGIN

/* Cumulative density function utilities */

static float cdf_lookup_inverse(const vector<float>& table, float2 range, float x)
{
	int index = upper_bound(table.begin(), table.end(), x) - table.begin();

	if(index == 0)
		return range[0];
	else
		index--;
	
	float t = (x - table[index])/(table[index+1] - table[index]);
	float y = ((index + t)/(table.size() - 1));

	return y*(range[1] - range[0]) + range[0];
}

static void cdf_invert(vector<float>& to, float2 to_range, const vector<float>& from, float2 from_range)
{
	float step = 1.0f/(float)(to.size() - 1);

	for(int i = 0; i < to.size(); i++) {
		float x = (i*step)*(from_range[1] - from_range[0]) + from_range[0];
		to[i] = cdf_lookup_inverse(from, to_range, x);
	}
}

/* BSSRDF */

static float bssrdf_lookup_table_max_radius(const BSSRDFParams *ss)
{
	/* todo: adjust when we use the real BSSRDF */
	return ss->ld;
}

static void bssrdf_lookup_table_create(const BSSRDFParams *ss, vector<float>& sample_table, vector<float>& pdf_table)
{
	const int size = BSSRDF_RADIUS_TABLE_SIZE;
	vector<float> cdf(size);
	vector<float> pdf(size);
	float step = 1.0f/(float)(size - 1);
	float max_radius = bssrdf_lookup_table_max_radius(ss);
	float pdf_sum = 0.0f;

	/* compute the probability density function */
	for(int i = 0; i < pdf.size(); i++) {
		float x = (i*step)*max_radius;
		pdf[i] = bssrdf_cubic(ss->ld, x);
		pdf_sum += pdf[i];
	}

	/* adjust for area covered by each distance */
	for(int i = 0; i < pdf.size(); i++) {
		float x = (i*step)*max_radius;
		pdf[i] *= 2*M_PI_F*x;
	}

	/* normalize pdf, we multiply in reflectance later */
	if(pdf_sum > 0.0f)
		for(int i = 0; i < pdf.size(); i++)
			pdf[i] /= pdf_sum;

	/* sum to account for sampling which uses overlapping sphere */
	for(int i = pdf.size() - 2; i >= 0; i--)
		pdf[i] = pdf[i] + pdf[i+1];

	/* compute the cumulative density function */
	cdf[0] = 0.0f;

	for(int i = 1; i < size; i++)
		cdf[i] = cdf[i-1] + 0.5f*(pdf[i-1] + pdf[i])*step*max_radius;
	
	/* invert cumulative density function for importance sampling */
	float2 cdf_range = make_float2(0.0f, cdf[size - 1]);
	float2 table_range = make_float2(0.0f, max_radius);

	cdf_invert(sample_table, table_range, cdf, cdf_range);

	/* copy pdf table */
	for(int i = 0; i < pdf.size(); i++)
		pdf_table[i] = pdf[i];
}

void bssrdf_table_build(vector<float>& table)
{
	vector<float> sample_table(BSSRDF_RADIUS_TABLE_SIZE);
	vector<float> pdf_table(BSSRDF_RADIUS_TABLE_SIZE);

	table.resize(BSSRDF_LOOKUP_TABLE_SIZE);

	/* create a 2D lookup table, for reflection x sample radius */
	for(int i = 0; i < BSSRDF_REFL_TABLE_SIZE; i++) {
		float refl = (float)i/(float)(BSSRDF_REFL_TABLE_SIZE-1);
		float ior = 1.3f;
		float radius = 1.0f;

		BSSRDFParams ss;
		bssrdf_setup_params(&ss, refl, radius, ior);
		bssrdf_lookup_table_create(&ss, sample_table, pdf_table);

		memcpy(&table[i*BSSRDF_RADIUS_TABLE_SIZE], &sample_table[0], BSSRDF_RADIUS_TABLE_SIZE*sizeof(float));
		memcpy(&table[BSSRDF_PDF_TABLE_OFFSET + i*BSSRDF_RADIUS_TABLE_SIZE], &pdf_table[0], BSSRDF_RADIUS_TABLE_SIZE*sizeof(float));
	}
}

CCL_NAMESPACE_END

