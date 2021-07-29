// This work is licensed under the Creative Commons Attribution 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by/3.0/
// or send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View,
// California, 94041, USA.

// Persistence of Vision Ray Tracer Scene Description File
// File: subsurface.pov
// Vers: 3.7
// Desc: Subsurface Scattering Demo - Candle on a Checkered Plane
// Date: 2011-02-25
// Auth: Christoph Lipka
//
// Recommended settings:
//  +W640 +H480 +A0.3
// Rendering time:
//  ~4 min on a 2.3GHz AMD Phenom X4 9650 QuadCore

#version 3.7;

#include "colors.inc"

global_settings {
  assumed_gamma 1.0
  mm_per_unit 40
  subsurface { samples 400, 40 }
  ambient_light 0.3
}

// ----------------------------------------

camera {
  location  <0.0, 2.5, -4.0>
  angle 50 // direction 1.5*z
  right     x*image_width/image_height
  look_at   <0.5, 1.0,  0.0>
}

sky_sphere {
  pigment {
    gradient y
    color_map {
      [0.0 rgb <0.6,0.7,1.0>]
      [0.7 rgb <0.0,0.1,0.8>]
    }
  }
}

light_source {
  <-30, 30, -30>
  color rgb <1,1,1>
}

// ----------------------------------------

// a checkered white/"black" marble plane
plane {
  y, -0.01
  texture {
    checker
    texture {
      // marble parameters derived from Jensen et al. "A Practical Model for Subsurface Light Transport", Siggraph 2001
      pigment {
      crackle
      turbulence 0.7
        color_map {
          [0.5 color rgb <0.83,0.79,0.75>*1.0]
          [0.9 color rgb <0.83,0.79,0.75>*0.8]
          [1.0 color rgb <1.00,0.75,0.70>*0.5]
        }
        scale 0.3
      }
      normal { 
	    agate 0.085 
	    turbulence 2
	  }      
      finish{
        diffuse 0.8
        specular 0.6
        reflection { 0.2 fresnel }
        conserve_energy
        subsurface { translucency <0.4562, 0.3811,0.3325> }
      }
    }
    texture {
    pigment{ crackle turbulence 0.25
             form <-1,1,0.05>
             color_map { [0.00 color rgb<1,1,1>]
                         [0.025 color rgb<0.252,0.482,0.372>]
                         [0.05 color rgb<0.082,0.092,0.072>]
                         [0.15 color rgb<0.05,0.09,0.06>]
                         [0.52 color rgb<0.008,0.019,0.012>]  
                         [0.65 color rgb<0.0025,0.0029,0.0014>]                     
                         [0.75 color rgb<0.0060,0.0084,0.0065>]
                         [1.00 color rgb<0.008,0.012,0.012>]
                       }
    	}
      normal { 
	    agate 0.085 
	    turbulence 2
	  }        
      finish{
        diffuse 0.8
        specular 0.6
        reflection { 0.2 fresnel }
        conserve_energy
        subsurface { translucency <0.4562, 0.3811,0.3325> }
      }
    }
    scale 4
    translate <0.7,0,1>
  }
  interior { ior 1.5 }
}

// the classic chrome sphere
sphere { <1.5,0.7,1>, 0.7
  pigment { color rgb 1 }
  finish {
    ambient 0 diffuse 0
    specular 0.7  roughness 0.01
    conserve_energy
    reflection { 0.7 metallic }
  }
}

// a candle...
blob {
  threshold 0.5
  cylinder { <0.0, 0.0,  0.0>,
             <0.0, 2.0,  0.0>,  1.0,   1.0 } // candle "body"
  sphere   { <0.0, 2.5,  0.0>,  0.8,  -2.0 } // (used to shape the candle top)
  sphere   { <0.0,-0.52, 0.0>,  0.8,  -2.0 } // (used to shape the candle bottom)
  sphere   { <0.0, 2.0, -0.5>,  0.1,  -0.2 } // the "notch" where wax runs over
  cylinder { <0.0, 1.88,-0.52>,
             <0.0, 1.5, -0.52>, 0.05,  0.2 } // a streak of wax running over
  sphere   { <0.0, 1.5, -0.55>, 0.07,  0.2 } // a drop of of wax running over
  texture {
    // bees' wax
    pigment { color rgb <0.8,0.50,0.01> }
    finish{
      diffuse 0.6 specular 0.6 roughness 0.1
      subsurface { translucency <5,3,1>*0.5 }
    }
  }
  interior { ior 1.45 }
  rotate -y*45
}

// ... and the wick
intersection {
  box { <-1,-1,-1>, <0,1,1> }
  torus { 0.15, 0.03 }
  rotate x*90
  translate <0.15, 1.95, 0.0>
  pigment { color rgb 0 }
  finish { ambient 0 diffuse 1 specular 0 }
  no_shadow
}

// a classic-textured slab for comparison
superellipsoid {
  <0.1,0.1>
  texture {
    pigment { color rgb <0.9,0.6,0.6> }
    finish{
      diffuse 1.0
      specular 0.6
      reflection { 0.2 fresnel }
      conserve_energy
    }
  }
  interior { ior 1.45 }
  scale <0.25,0.05,0.25>
  rotate y*30
  translate <1.2,0.05,0.25>
}
