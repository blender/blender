// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence of Vision Raytracer Scene Description File
// File: diffuse_back.pov
// Author: Christoph Lipka
// Description: Demonstrates diffuse backside illumination
//
// -w640 -h480
// -w800 -h600 +a0.3
// 
// Warning: this will take time!

#version 3.7;
  
#declare Photons=on;
#declare Radiosity=on;

global_settings {
  max_trace_level 25
	assumed_gamma 2.2
  #if (Photons)
    photons {
      count 100000
    }
  #end
  #if (Radiosity)
    radiosity {
      pretrace_start 0.04
      pretrace_end   0.005
      count 1000
      nearest_count 10
      error_bound 0.5
      recursion_limit 2
      low_error_factor .25
      gray_threshold 0.0
      minimum_reuse 0.002
      brightness 1
      adc_bailout 0.01/2
      always_sample off
    }
  #end
}

#if (Radiosity)
  default {
    finish { ambient 0 }
  }
#else
  default {
    finish { ambient 0.2 }
  }
#end

// ----------------------------------------

#declare OverallBrightness      = 8;
#declare OverallScale           = 100;

camera {
  right     x*image_width/image_height // keep propotions with any aspect ratio
  location  < 1,1.6,-2.5>*OverallScale
  look_at   <-2.0,1.2,0>*OverallScale
}

light_source {
  vnormalize(<-500,200,-250>)*1000*OverallScale
  color rgb 1.3 * OverallBrightness
  area_light x*10*OverallScale,y*10*OverallScale, 9,9 adaptive 1 jitter circular orient
  photons {
    refraction on
    reflection on
  }
}

sky_sphere {
  pigment {
    gradient y
    color_map {
      [0.0 rgb <0.6,0.7,1.0>*OverallBrightness*0.5]
      [0.7 rgb <0.0,0.1,0.8>*OverallBrightness*0.5]
    }
  }
}


// ----------------------------------------

plane { y, -10
  texture {
    pigment { color rgb <1.0, 0.8, 0.6> }
    finish { diffuse 0.5 }
  }  
}

#declare M_SolidWhite= material {
  texture {
    pigment { rgb 1 }
    finish { ambient 0 diffuse 0.8 specular 0.2 reflection { 0.2 } }
  }
}

// Room

difference {
  box { <-3.1,-1,-4>, <3.1,3.5,4> }     // solid block
  box { <-3,-0.2,-3>, <3,2.5,3> }       // main room cutout
  box { <-3.2,0.3,-2>, <2.9,2,2> }      // window cutout
  texture { 
    pigment { color rgb <0.9, 0.9, 0.9> }
    finish { diffuse 1.0 }
  }
  scale OverallScale
}

// Window Bars

union {
  cylinder { <-3.05,0, 1>, <-3.05,2, 1>, 0.05 }
  cylinder { <-3.05,0,-1>, <-3.05,2,-1>, 0.05 }
  material { M_SolidWhite }
  scale OverallScale 
}

// Baseboards

#declare Baseboard = union {
  cylinder { <-3,0.1,0>, <3,0.1,0>, 0.025 }
  box { <-3,0,0>, <3,0.1,-0.025> }
  material { M_SolidWhite }
  translate z*3
}
                                        
union {                                        
  object { Baseboard }
  object { Baseboard rotate y*90 }
  object { Baseboard rotate y*180 }
  object { Baseboard rotate y*270 }
  scale OverallScale 
}


box { <-3,0,-3>, <3,-0.1,3>
  pigment { color rgb <1.0, 0.8, 0.6> }
  scale OverallScale
}


// Curtains

#declare M_Curtains= material {
  texture {
    pigment { rgb <1.0,0.8,0.6> }
    finish {
      ambient 0
      diffuse 0.7,0.2
    }
  }
}

#declare Curtain= union {
  polygon{ 5, <0,0.1,2.0>, <0,0.1,0.1>, <0,2.45,0.1>, <0,2.45,2.0>, <0,0.1,2.0> material { M_Curtains } }          
  cylinder { <0,0.1,2.025>, <0,0.1,0.075>, 0.025 material { M_SolidWhite } }
  cylinder { <0,2.45,2.025>, <0,2.45,0.075>, 0.025 material { M_SolidWhite } }
  translate <-2.8,0,0>
  material { M_Curtains }
}

union {
  object { Curtain }
  object { Curtain scale <1,1,-1> }
  scale OverallScale 
}

// Screen

#declare M_Screen= material {
  texture {
    pigment { rgbt <1,1,1, 0.01> }
    finish {
      ambient 0
      diffuse 0.55,0.45
      specular 0.2
      reflection { 0.2 }
    }
  }
}

#declare Screen = cylinder { <0,0,0>, <0,1.0,0>, 0.5
  open
  clipped_by { plane { x, 0.1 } }
  material { M_Screen }
}

union {
  object { Screen rotate y*45 translate <-2.25,0,2> }
  object { Screen rotate y*0  translate <-2.25,0,-1.0> }
  scale OverallScale 
}

// Glass Objects

#declare M_Glass= material {
  texture {
    pigment {rgbt 1}
    finish {
      ambient 0.0
      diffuse 0.05
      specular 0.6
      roughness 0.005
      reflection {
        0.1, 1.0
        fresnel on
      }
      conserve_energy
    }
  }
  interior {
    ior 1.5
    fade_power 1001
    fade_distance 0.9 * 10
    fade_color <0.5,0.8,0.6>
  }
}

sphere {
  <0,1,0>, 1
  scale 0.2
  translate <-1.8,0,0.5>
  material { M_Glass }
  photons {  // photon block for an object
    target 1.0
    refraction on
    reflection on
  }
  scale OverallScale
}

cylinder {
  <0,0.01,0>, <0,2.5,0>, 1
  scale 0.2
  translate <-3.05,0.3,0.4>
  material { M_Glass }
  photons {  // photon block for an object
    target 1.0
    refraction on
    reflection on
  }
  scale OverallScale
}
