/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/sunsky.c
 *  \ingroup render
 *
 * This feature comes from Preetham paper on "A Practical Analytic Model for Daylight"
 * and example code from Brian Smits, another author of that paper in
 * http://www.cs.utah.edu/vissim/papers/sunsky/code/
 */

#include "sunsky.h"
#include "BLI_math.h"

/**
 * These macros are defined for vector operations
 * */

/**
 * compute v1 = v2 op v3
 * v1, v2 and v3 are vectors contains 3 float
 * */
#define VEC3OPV(v1, v2, op, v3)                                               \
	{                                                                         \
		v1[0] = (v2[0] op v3[0]);                                             \
		v1[1] = (v2[1] op v3[1]);                                             \
		v1[2] = (v2[2] op v3[2]);                                             \
	} (void)0

/**
 * compute v1 = v2 op f1
 * v1, v2 are vectors contains 3 float
 * and f1 is a float
 * */
#define VEC3OPF(v1, v2, op, f1)                                               \
	{                                                                         \
		v1[0] = (v2[0] op(f1));                                              \
		v1[1] = (v2[1] op(f1));                                              \
		v1[2] = (v2[2] op(f1));                                              \
	} (void)0

/**
 * compute v1 = f1 op v2
 * v1, v2 are vectors contains 3 float
 * and f1 is a float
 * */
#define FOPVEC3(v1, f1, op, v2)                                               \
	{                                                                         \
		v1[0] = ((f1) op v2[0]);                                              \
		v1[1] = ((f1) op v2[1]);                                              \
		v1[2] = ((f1) op v2[2]);                                              \
	} (void)0

/**
 * ClipColor:
 * clip a color to range [0, 1];
 * */
void ClipColor(float c[3])
{
	if (c[0] > 1.0f) c[0] = 1.0f;
	if (c[0] < 0.0f) c[0] = 0.0f;
	if (c[1] > 1.0f) c[1] = 1.0f;
	if (c[1] < 0.0f) c[1] = 0.0f;
	if (c[2] > 1.0f) c[2] = 1.0f;
	if (c[2] < 0.0f) c[2] = 0.0f;
}

/**
 * AngleBetween:
 * compute angle between to direction 
 * all angles are in radians
 * */
static float AngleBetween(float thetav, float phiv, float theta, float phi)
{
	float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);

	if (cospsi > 1.0f)
		return 0;
	if (cospsi < -1.0f)
		return M_PI;

	return acosf(cospsi);
}

/**
 * DirectionToThetaPhi:
 * this function convert a direction to it's theta and phi value
 * parameters:
 * toSun: contains direction information
 * theta, phi, are return values from this conversion
 * */
static void DirectionToThetaPhi(float *toSun, float *theta, float *phi)
{
	*theta = acosf(toSun[2]);
	if (fabsf(*theta) < 1e-5f)
		*phi = 0;
	else
		*phi = atan2f(toSun[1], toSun[0]);
}

/**
 * PerezFunction:
 * compute perez function value based on input parameters
 */
static float PerezFunction(struct SunSky *sunsky, const float *lam, float theta, float gamma, float lvz)
{
	float den, num;
	
	den = ((1 + lam[0] * expf(lam[1])) *
	       (1 + lam[2] * expf(lam[3] * sunsky->theta) + lam[4] * cosf(sunsky->theta) * cosf(sunsky->theta)));
	
	num = ((1 + lam[0] * expf(lam[1] / cosf(theta))) *
	       (1 + lam[2] * expf(lam[3] * gamma) + lam[4] * cosf(gamma) * cosf(gamma)));
	
	return(lvz * num / den);
}

/**
 * InitSunSky:
 * this function compute some sun,sky parameters according to input parameters and also initiate some other sun, sky parameters
 * parameters:
 * sunSky, is a structure that contains information about sun, sky and atmosphere, in this function, most of its values initiated
 * turb, is atmosphere turbidity
 * toSun, contains sun direction
 * horizon_brighness, controls the brightness of the horizon colors
 * spread, controls colors spreed at horizon
 * sun_brightness, controls sun's brightness
 * sun_size, controls sun's size
 * back_scatter, controls back scatter light
 * */
void InitSunSky(struct SunSky *sunsky, float turb, const float toSun[3], float horizon_brightness,
                float spread, float sun_brightness, float sun_size, float back_scatter,
                float skyblendfac, short skyblendtype, float sky_exposure, float sky_colorspace)
{
	float theta2;
	float theta3;
	float T;
	float T2;
	float chi;

	sunsky->turbidity = turb;

	sunsky->horizon_brightness = horizon_brightness;
	sunsky->spread = spread;
	sunsky->sun_brightness = sun_brightness;
	sunsky->sun_size = sun_size;
	sunsky->backscattered_light = back_scatter;
	sunsky->skyblendfac = skyblendfac;
	sunsky->skyblendtype = skyblendtype;
	sunsky->sky_exposure = -sky_exposure;
	sunsky->sky_colorspace = sky_colorspace;
	
	sunsky->toSun[0] = toSun[0];
	sunsky->toSun[1] = toSun[1];
	sunsky->toSun[2] = toSun[2];

	DirectionToThetaPhi(sunsky->toSun, &sunsky->theta, &sunsky->phi);

	sunsky->sunSolidAngle = 0.25 * M_PI * 1.39 * 1.39 / (150 * 150);   /* = 6.7443e-05 */

	theta2 = sunsky->theta * sunsky->theta;
	theta3 = theta2 * sunsky->theta;
	T = turb;
	T2 = turb * turb;

	chi = (4.0f / 9.0f - T / 120.0f) * ((float)M_PI - 2.0f * sunsky->theta);
	sunsky->zenith_Y = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
	sunsky->zenith_Y *= 1000;   /* conversion from kcd/m^2 to cd/m^2 */

	if (sunsky->zenith_Y <= 0)
		sunsky->zenith_Y = 1e-6;
	
	sunsky->zenith_x =
	    (+0.00165f * theta3 - 0.00374f * theta2 + 0.00208f * sunsky->theta + 0.0f) * T2 +
	    (-0.02902f * theta3 + 0.06377f * theta2 - 0.03202f * sunsky->theta + 0.00394f) * T +
	    (+0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * sunsky->theta + 0.25885f);

	sunsky->zenith_y =
	    (+0.00275f * theta3 - 0.00610f * theta2 + 0.00316f * sunsky->theta + 0.0f) * T2 +
	    (-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * sunsky->theta + 0.00515f) * T +
	    (+0.15346f * theta3 - 0.26756f * theta2 + 0.06669f * sunsky->theta + 0.26688f);

	
	sunsky->perez_Y[0] = 0.17872f * T - 1.46303f;
	sunsky->perez_Y[1] = -0.35540f * T + 0.42749f;
	sunsky->perez_Y[2] = -0.02266f * T + 5.32505f;
	sunsky->perez_Y[3] = 0.12064f * T - 2.57705f;
	sunsky->perez_Y[4] = -0.06696f * T + 0.37027f;

	sunsky->perez_x[0] = -0.01925f * T - 0.25922f;
	sunsky->perez_x[1] = -0.06651f * T + 0.00081f;
	sunsky->perez_x[2] = -0.00041f * T + 0.21247f;
	sunsky->perez_x[3] = -0.06409f * T - 0.89887f;
	sunsky->perez_x[4] = -0.00325f * T + 0.04517f;

	sunsky->perez_y[0] = -0.01669f * T - 0.26078f;
	sunsky->perez_y[1] = -0.09495f * T + 0.00921f;
	sunsky->perez_y[2] = -0.00792f * T + 0.21023f;
	sunsky->perez_y[3] = -0.04405f * T - 1.65369f;
	sunsky->perez_y[4] = -0.01092f * T + 0.05291f;
	
	/* suggested by glome in patch [#8063] */
	sunsky->perez_Y[0] *= sunsky->horizon_brightness;
	sunsky->perez_x[0] *= sunsky->horizon_brightness;
	sunsky->perez_y[0] *= sunsky->horizon_brightness;
	
	sunsky->perez_Y[1] *= sunsky->spread;
	sunsky->perez_x[1] *= sunsky->spread;
	sunsky->perez_y[1] *= sunsky->spread;

	sunsky->perez_Y[2] *= sunsky->sun_brightness;
	sunsky->perez_x[2] *= sunsky->sun_brightness;
	sunsky->perez_y[2] *= sunsky->sun_brightness;
	
	sunsky->perez_Y[3] *= sunsky->sun_size;
	sunsky->perez_x[3] *= sunsky->sun_size;
	sunsky->perez_y[3] *= sunsky->sun_size;
	
	sunsky->perez_Y[4] *= sunsky->backscattered_light;
	sunsky->perez_x[4] *= sunsky->backscattered_light;
	sunsky->perez_y[4] *= sunsky->backscattered_light;
}

/**
 * GetSkyXYZRadiance:
 * this function compute sky radiance according to a view parameters `theta' and `phi'and sunSky values
 * parameters:
 * sunSky, sontains sun and sky parameters
 * theta, is sun's theta
 * phi, is sun's phi
 * color_out, is computed color that shows sky radiance in XYZ color format
 * */
void GetSkyXYZRadiance(struct SunSky *sunsky, float theta, float phi, float color_out[3])
{
	float gamma;
	float x, y, Y, X, Z;
	float hfade = 1, nfade = 1;


	if (theta > (float)M_PI_2) {
		hfade = 1.0f - (theta * (float)M_1_PI - 0.5f) * 2.0f;
		hfade = hfade * hfade * (3.0f - 2.0f * hfade);
		theta = M_PI_2;
	}

	if (sunsky->theta > (float)M_PI_2) {
		if (theta <= (float)M_PI_2) {
			nfade = 1.0f - (0.5f - theta * (float)M_1_PI) * 2.0f;
			nfade *= 1.0f - (sunsky->theta * (float)M_1_PI - 0.5f) * 2.0f;
			nfade = nfade * nfade * (3.0f - 2.0f * nfade);
		}
	}

	gamma = AngleBetween(theta, phi, sunsky->theta, sunsky->phi);
	
	/* Compute xyY values */
	x = PerezFunction(sunsky, sunsky->perez_x, theta, gamma, sunsky->zenith_x);
	y = PerezFunction(sunsky, sunsky->perez_y, theta, gamma, sunsky->zenith_y);
	Y = 6.666666667e-5f * nfade * hfade * PerezFunction(sunsky, sunsky->perez_Y, theta, gamma, sunsky->zenith_Y);

	if (sunsky->sky_exposure != 0.0f)
		Y = 1.0 - exp(Y * sunsky->sky_exposure);
	
	X = (x / y) * Y;
	Z = ((1 - x - y) / y) * Y;

	color_out[0] = X;
	color_out[1] = Y;
	color_out[2] = Z;
}

/**
 * GetSkyXYZRadiancef:
 * this function compute sky radiance according to a view direction `varg' and sunSky values
 * parameters:
 * sunSky, sontains sun and sky parameters
 * varg, shows direction
 * color_out, is computed color that shows sky radiance in XYZ color format
 * */
void GetSkyXYZRadiancef(struct SunSky *sunsky, const float varg[3], float color_out[3])
{
	float theta, phi;
	float v[3];

	normalize_v3_v3(v, varg);

	if (v[2] < 0.001f) {
		v[2] = 0.001f;
		normalize_v3(v);
	}

	DirectionToThetaPhi(v, &theta, &phi);
	GetSkyXYZRadiance(sunsky, theta, phi, color_out);
}

/**
 * ComputeAttenuatedSunlight:
 * this function compute attenuated sun light based on sun's theta and atmosphere turbidity
 * parameters:
 * theta, is sun's theta
 * turbidity: is atmosphere turbidity
 * fTau: contains computed attenuated sun light
 * */
static void ComputeAttenuatedSunlight(float theta, int turbidity, float fTau[3])
{
	float fBeta;
	float fTauR, fTauA;
	float m;
	float fAlpha;

	int i;
	float fLambda[3]; 
	fLambda[0] = 0.65f;
	fLambda[1] = 0.57f;
	fLambda[2] = 0.475f;

	fAlpha = 1.3f;
	fBeta = 0.04608365822050f * turbidity - 0.04586025928522f;
	
	m =  1.0f / (cosf(theta) + 0.15f * powf(93.885f - theta / (float)M_PI * 180.0f, -1.253f));

	for (i = 0; i < 3; i++) {
		/* Rayleigh Scattering */
		fTauR = expf(-m * 0.008735f * powf(fLambda[i], (float)(-4.08f)));

		/* Aerosal (water + dust) attenuation */
		fTauA = exp(-m * fBeta * powf(fLambda[i], -fAlpha));

		fTau[i] = fTauR * fTauA;
	}
}

/**
 * InitAtmosphere:
 * this function initiate sunSky structure with user input parameters.
 * parameters:
 * sunSky, contains information about sun, and in this function some atmosphere parameters will initiated
 * sun_intens, shows sun intensity value
 * mief, Mie scattering factor this factor currently call with 1.0 
 * rayf, Rayleigh scattering factor, this factor currently call with 1.0
 * inscattf, inscatter light factor that range from 0.0 to 1.0, 0.0 means no inscatter light and 1.0 means full inscatter light
 * extincf, extinction light factor that range from 0.0 to 1.0, 0.0 means no extinction and 1.0 means full extinction
 * disf, is distance factor, multiplied to pixle's z value to compute each pixle's distance to camera,
 * */
void InitAtmosphere(struct SunSky *sunSky, float sun_intens, float mief, float rayf,
                    float inscattf, float extincf, float disf)
{
	const float pi = M_PI;
	const float n = 1.003f;  /* refractive index */
	const float N = 2.545e25;
	const float pn = 0.035f;
	const float T = 2.0f;
	float fTemp, fTemp2, fTemp3, fBeta, fBetaDash;
	float c = (6.544f * T - 6.51f) * 1e-17f;
	float K[3] = {0.685f, 0.679f, 0.670f};
	float vBetaMieTemp[3];
	
	float fLambda[3], fLambda2[3], fLambda4[3];
	float vLambda2[3];
	float vLambda4[3];
	
	int i;

	sunSky->atm_SunIntensity = sun_intens;
	sunSky->atm_BetaMieMultiplier  = mief;
	sunSky->atm_BetaRayMultiplier = rayf;
	sunSky->atm_InscatteringMultiplier = inscattf;
	sunSky->atm_ExtinctionMultiplier = extincf;
	sunSky->atm_DistanceMultiplier = disf;
		
	sunSky->atm_HGg = 0.8;

	fLambda[0]  = 1 / 650e-9f;
	fLambda[1]  = 1 / 570e-9f;
	fLambda[2]  = 1 / 475e-9f;
	for (i = 0; i < 3; i++) {
		fLambda2[i] = fLambda[i] * fLambda[i];
		fLambda4[i] = fLambda2[i] * fLambda2[i];
	}

	vLambda2[0] = fLambda2[0];
	vLambda2[1] = fLambda2[1];
	vLambda2[2] = fLambda2[2];

	vLambda4[0] = fLambda4[0];
	vLambda4[1] = fLambda4[1];
	vLambda4[2] = fLambda4[2];

	/* Rayleigh scattering constants. */
	fTemp = pi * pi * (n * n - 1) * (n * n - 1) * (6 + 3 * pn) / (6 - 7 * pn) / N;
	fBeta = 8 * fTemp * pi / 3;

	VEC3OPF(sunSky->atm_BetaRay, vLambda4, *, fBeta);
	fBetaDash = fTemp / 2;
	VEC3OPF(sunSky->atm_BetaDashRay, vLambda4, *, fBetaDash);


	/* Mie scattering constants. */
	fTemp2 = 0.434f * c * (2 * pi) * (2 * pi) * 0.5f;
	VEC3OPF(sunSky->atm_BetaDashMie, vLambda2, *, fTemp2);
	
	fTemp3 = 0.434f * c * pi * (2 * pi) * (2 * pi);
	
	VEC3OPV(vBetaMieTemp, K, *, fLambda);
	VEC3OPF(sunSky->atm_BetaMie, vBetaMieTemp, *, fTemp3);
	
}

/**
 * AtmospherePixleShader:
 * this function apply atmosphere effect on a pixle color `rgb' at distance `s'
 * parameters:
 * sunSky, contains information about sun parameters and user values
 * view, is camera view vector
 * s, is distance 
 * rgb, contains rendered color value for a pixle
 * */
void AtmospherePixleShader(struct SunSky *sunSky, float view[3], float s, float rgb[3])
{
	float costheta;
	float Phase_1;
	float Phase_2;
	float sunColor[3];
	
	float E[3];
	float E1[3];
	
	
	float I[3];
	float fTemp;
	float vTemp1[3], vTemp2[3];

	float sunDirection[3];
	
	s *= sunSky->atm_DistanceMultiplier;
	
	sunDirection[0] = sunSky->toSun[0];
	sunDirection[1] = sunSky->toSun[1];
	sunDirection[2] = sunSky->toSun[2];

	costheta = dot_v3v3(view, sunDirection); /* cos(theta) */
	Phase_1 = 1 + (costheta * costheta); /* Phase_1 */

	VEC3OPF(sunSky->atm_BetaRay, sunSky->atm_BetaRay, *, sunSky->atm_BetaRayMultiplier);
	VEC3OPF(sunSky->atm_BetaMie, sunSky->atm_BetaMie, *, sunSky->atm_BetaMieMultiplier);
	VEC3OPV(sunSky->atm_BetaRM, sunSky->atm_BetaRay, +, sunSky->atm_BetaMie);

	/* e^(-(beta_1 + beta_2) * s) = E1 */
	VEC3OPF(E1, sunSky->atm_BetaRM, *, -s / (float)M_LN2);
	E1[0] = exp(E1[0]);
	E1[1] = exp(E1[1]);
	E1[2] = exp(E1[2]);

	copy_v3_v3(E, E1);

	/* Phase2(theta) = (1-g^2)/(1+g-2g*cos(theta))^(3/2) */
	fTemp = 1 + sunSky->atm_HGg - 2 * sunSky->atm_HGg * costheta;
	fTemp = fTemp * sqrtf(fTemp);
	Phase_2 = (1 - sunSky->atm_HGg * sunSky->atm_HGg) / fTemp;
	
	VEC3OPF(vTemp1, sunSky->atm_BetaDashRay, *, Phase_1);
	VEC3OPF(vTemp2, sunSky->atm_BetaDashMie, *, Phase_2);

	VEC3OPV(vTemp1, vTemp1, +, vTemp2);
	FOPVEC3(vTemp2, 1.0f, -, E1);
	VEC3OPV(vTemp1, vTemp1, *, vTemp2);

	FOPVEC3(vTemp2, 1.0f, /, sunSky->atm_BetaRM);

	VEC3OPV(I, vTemp1, *, vTemp2);
		
	VEC3OPF(I, I, *, sunSky->atm_InscatteringMultiplier);
	VEC3OPF(E, E, *, sunSky->atm_ExtinctionMultiplier);

	/* scale to color sun */
	ComputeAttenuatedSunlight(sunSky->theta, sunSky->turbidity, sunColor);
	VEC3OPV(E, E, *, sunColor);

	VEC3OPF(I, I, *, sunSky->atm_SunIntensity);

	VEC3OPV(rgb, rgb, *, E);
	VEC3OPV(rgb, rgb, +, I);
}

#undef VEC3OPV
#undef VEC3OPF
#undef FOPVEC3

/* EOF */
