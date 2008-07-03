 /**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): zaghaghi
 * 
 * ***** END GPL LICENSE BLOCK *****
 */
/**
 * This feature comes from Preetham paper on "A Practical Analytic Model for Daylight" 
 * and example code from Brian Smits, another author of that paper in 
 * http://www.cs.utah.edu/vissim/papers/sunsky/code/
 * */
#ifndef SUNSKY_H_
#define SUNSKY_H_

#define SPECTRUM_MAX_COMPONENTS     100
#define SPECTRUM_START              350.0
#define SPECTRUM_END                800.0

typedef struct SunSky
{
    short effect_type;
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
}SunSky;

/**
 * InitSunSky:
 * this function compute some sun,sky parameters according to input parameters and also initiate some other sun, sky parameters
 * parameters:
 * sunSky, is a structure that contains informtion about sun, sky and atmosphere, in this function, most of its values initiated
 * turb, is atmosphere turbidity
 * toSun, contains sun direction
 * horizon_brighness, controls the brightness of the horizon colors
 * spread, controls colors spreed at horizon
 * sun_brightness, controls sun's brightness
 * sun_size, controls sun's size
 * back_scatter, controls back scatter light
 * */
void InitSunSky(struct SunSky *sunsky, float turb, float *toSun, float horizon_brightness, 
				float spread,float sun_brightness, float sun_size, float back_scatter);

/**
 * GetSkyXYZRadiance:
 * this function compute sky radiance according to a view parameters `theta' and `phi'and sunSky values
 * parameters:
 * sunSky, sontains sun and sky parameters
 * theta, is sun's theta
 * phi, is sun's phi
 * color_out, is computed color that shows sky radiance in XYZ color format
 * */
void GetSkyXYZRadiance(struct SunSky* sunsky, float theta, float phi, float color_out[3]);

/**
 * GetSkyXYZRadiancef:
 * this function compute sky radiance according to a view direction `varg' and sunSky values
 * parameters:
 * sunSky, sontains sun and sky parameters
 * varg, shows direction
 * color_out, is computed color that shows sky radiance in XYZ color format
 * */
void GetSkyXYZRadiancef(struct SunSky* sunsky, const float varg[3], float color_out[3]);

/**
 * InitAtmosphere:
 * this function intiate sunSky structure with user input parameters.
 * parameters:
 * sunSky, contains information about sun, and in this function some atmosphere parameters will initiated
 * sun_intens, shows sun intensity value
 * mief, Mie scattering factor this factor currently call with 1.0 
 * rayf, Rayleigh scattering factor, this factor currently call with 1.0
 * inscattf, inscatter light factor that range from 0.0 to 1.0, 0.0 means no inscatter light and 1.0 means full inscatter light
 * extincf, extinction light factor that range from 0.0 to 1.0, 0.0 means no extinction and 1.0 means full extinction
 * disf, is distance factor, multiplyed to pixle's z value to compute each pixle's distance to camera, 
 * */
void InitAtmosphere(struct SunSky *sunSky, float sun_intens, float mief, float rayf, float inscattf, float extincf, float disf);

/**
 * AtmospherePixleShader:
 * this function apply atmosphere effect on a pixle color `rgb' at distance `s'
 * parameters:
 * sunSky, contains information about sun parameters and user values
 * view, is camera view vector
 * s, is distance 
 * rgb, contains rendered color value for a pixle
 * */
void AtmospherePixleShader( struct SunSky* sunSky, float view[3], float s, float rgb[3]);

/**
 * ClipColor:
 * clip a color to range [0,1];
 * */
void ClipColor(float c[3]);

#endif /*SUNSKY_H_*/
