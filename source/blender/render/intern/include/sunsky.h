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
 * Contributor(s): zaghaghi
 * 
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/sunsky.h
 *  \ingroup render
 */

#ifndef __SUNSKY_H__
#define __SUNSKY_H__

#define SPECTRUM_MAX_COMPONENTS     100
#define SPECTRUM_START              350.0
#define SPECTRUM_END                800.0

typedef struct SunSky
{
	short effect_type, skyblendtype, sky_colorspace;
	float turbidity;
	float theta, phi;

	float toSun[3];

	/*float sunSpectralRaddata[SPECTRUM_MAX_COMPONENTS];*/
	float sunSolidAngle;

	float zenith_Y, zenith_x, zenith_y;

	float perez_Y[5], perez_x[5], perez_y[5];

	/* suggested by glome in 
	 * http://projects.blender.org/tracker/?func=detail&atid=127&aid=8063&group_id=9*/
	float horizon_brightness;
	float spread;
	float sun_brightness;
	float sun_size;
	float backscattered_light;
	float skyblendfac;
	float sky_exposure;
	
	float atm_HGg;

	float atm_SunIntensity;
	float atm_InscatteringMultiplier;
	float atm_ExtinctionMultiplier;
	float atm_BetaRayMultiplier;
	float atm_BetaMieMultiplier;
	float atm_DistanceMultiplier;

	float atm_BetaRay[3];
	float atm_BetaDashRay[3];
	float atm_BetaMie[3];
	float atm_BetaDashMie[3];
	float atm_BetaRM[3];
} SunSky;

void InitSunSky(struct SunSky *sunsky, float turb, float *toSun, float horizon_brightness, 
                float spread,float sun_brightness, float sun_size, float back_scatter,
                float skyblendfac, short skyblendtype, float sky_exposure, float sky_colorspace);

void GetSkyXYZRadiance(struct SunSky *sunsky, float theta, float phi, float color_out[3]);
void GetSkyXYZRadiancef(struct SunSky *sunsky, const float varg[3], float color_out[3]);
void InitAtmosphere(struct SunSky *sunSky, float sun_intens, float mief, float rayf, float inscattf, float extincf, float disf);
void AtmospherePixleShader(struct SunSky *sunSky, float view[3], float s, float rgb[3]);
void ClipColor(float c[3]);

#endif /*__SUNSKY_H__*/
