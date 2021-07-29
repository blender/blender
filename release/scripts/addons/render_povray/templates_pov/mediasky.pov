// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence of Vision Raytracer Scene Description File
// File: mediasky.pov
// Author: Chris Huff
// Description: This file demonstrates the use of scattering media
// to create a sky with clouds. It attempts to simulate an actual
// atmosphere: there is an outer shell of media that scatters blue
// light, and an inner cloud shell that scatters white. The scattered
// light from the outer shell makes the sky appear blue, and the light
// that passes through is tinted orange by its passage, giving the
// clouds an orange color.
//
//  Updated: 2013/02/15 for 3.7
//
// -w320 -h180
// +w640 +h360 +a0.3
// use 16:9 aspect ratio
//
//*******************************************

#version 3.7;

#include "colors.inc"

global_settings {
  assumed_gamma 1.0
  max_trace_level 5
}

#declare CamPos = <-5, 1,-25>;

camera {
	location CamPos
	up y 
        right     x*image_width/image_height // keep propotions with any aspect ratio
	look_at < 0, 7.5, 0>
	angle 90
}

light_source {CamPos, color Gray30 media_interaction off}
//light_source {vrotate(z, <-1, 8, 0>)*500000, color rgb < 1, 0.8, 0.65>}

#declare SunPos = vrotate(z, <-12, 8, 0>)*1000000;
light_source {SunPos, color White*2}
sphere {SunPos, 75000
	texture {
		pigment {color White}
		finish {ambient 10 diffuse 0}
	}
	no_shadow
}


#declare PlanetSize = 50000;

//the ocean
sphere {< 0, 0, 0>, 1
	scale PlanetSize
	translate -y*PlanetSize
	hollow
	texture {
//		pigment {color rgb < 1, 1, 1>}
		pigment {color rgbf < 1, 1, 1, 1>}
		finish {
			ambient 0 diffuse 0.7
			reflection {0.5, 1
				fresnel//use the fresnel form of angle-dependant reflection
				metallic//use metallic reflection
			}
			conserve_energy
			metallic//use metallic highlights
		}
		normal {bumps bump_size 0.075 scale < 4, 1, 1>*0.025}
	}
	interior {
		ior 1.33//required for fresnel reflection
		media {
			method 3
			samples 2 intervals 1
			absorption color rgb < 0.75, 0.5, 0.25>*0.005
		}
	}
}
//the ocean floor
sphere {< 0, 0, 0>, 1
	scale PlanetSize - 100
	translate -y*PlanetSize
	texture {
		pigment {color rgb 1}
	}
}

#macro SkyShell(minAlt, maxAlt, Int)
    difference {
    	sphere {< 0, 0, 0>, 1 scale (PlanetSize + maxAlt)}
    	sphere {< 0, 0, 0>, 1 scale (PlanetSize + minAlt)}
    	hollow
	    texture {pigment {color rgbf 1}}
    	translate -y*PlanetSize
    	interior {Int}
    }
#end

//A much more realistic sky could be done using multiple layers
//of clouds to simulate clouds of different densities and with
//different altitudes. Of course, this would render a lot slower...

//the "cloud shell", creates clouds.
SkyShell(1000, 1300,
	interior {
		media {
			method 3 aa_threshold 0.1 aa_level 3
			samples 4 intervals 1
			scattering {2, color White*0.0075 extinction 1}
			density {wrinkles
				scale < 5, 2, 2>*200
				warp {turbulence 2}
				color_map {
					[0 color rgb 1]
					[0.5 color rgb 0.85]
					[0.55 color rgb 0.035]
					[1 color rgb 0.035]
				}
			}
		}
/*		media {
			method 3
			samples 2 intervals 1
			scattering {2, color White*0.0075*0.015 extinction 1}
		}*/
	}
)

//the "atmosphere shell", creates the blue sky and orange light.
SkyShell(1001, 2200,
	interior {
		media {
			method 3
			samples 2 intervals 1
			scattering {4, color rgb < 0.25, 0.6, 0.9>*0.00075 extinction 1}
		}
	}
)

