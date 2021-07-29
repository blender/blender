// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence of Vision Ray Tracer Scene Description File
// File: gamma_showcase.pov
// Vers: 3.7
// Desc: Gamma Handling Test Scene - An arrangement of spheres on a marble plane
// Date: 2010-12-21
// Auth: Christoph Lipka
// MORE INFORMATION: http://wiki.povray.org/content/Documentation:Tutorial_Section_3.3#Gamma_Handling
//

// +w640 +h480 +a0.3 +am1 +fN -d File_Gamma=sRGB Output_File_Name=gamma_showcase.png
// +w640 +h480 +a0.3 +am1 +fN -d File_Gamma=1.0  Output_File_Name=gamma_showcase_linear.png
// +w320 +h240 +a0.3 +am1 +fN -d File_Gamma=sRGB Output_File_Name=gamma_showcase_ref0.png Declare=Stripes=off
// +w320 +h240 +a0.3 +am1 +fN -d File_Gamma=sRGB Output_File_Name=gamma_showcase_ref1.png Declare=Stripes=off Declare=Gamma=1.2
// +w320 +h240 +a0.3 +am1 +fN -d File_Gamma=sRGB Output_File_Name=gamma_showcase_ref2.png Declare=Stripes=off Declare=Gamma=0.8
// +w640 +h480 +a0.3 +am1 -f +d

#version 3.7;

#include "colors.inc"
#include "stones.inc"

#ifndef (Stripes)
  #declare Stripes = on;
#end
#ifndef (Gamma)
  #declare Gamma   = 1.0;
#end

global_settings {
  max_trace_level 5
  assumed_gamma 1.0
  radiosity {
    pretrace_start 0.08
    pretrace_end   0.01
    count 35
    nearest_count 5
    error_bound 1.8
    recursion_limit 2
    low_error_factor .5
    gray_threshold 0.0
    minimum_reuse 0.015
    brightness 1
    adc_bailout 0.01/2
  }
}

#default {
  texture {
    pigment {rgb 1}
    finish {
      ambient 0.0
      diffuse 0.6
      specular 0.6 roughness 0.001
      reflection { 0.0 1.0 fresnel on }
      conserve_energy
    }
  }
}

// ----------------------------------------

#local TestRed   = rgb <0.5,0.1,0.1>;
#local TestGreen = rgb <0.1,0.5,0.1>;
#local TestBlue  = rgb <0.1,0.1,0.5>;

#local CameraFocus = <0,0.5,0>;
#local CameraDist  = 8;
#local CameraDepth = 1.8;
#local CameraTilt  = 20;

camera {
  location  <0,0,0>
  direction z*CameraDepth
  right     x*image_width/image_height
  up        y
  translate <0,0,-CameraDist>
  rotate    x*CameraTilt
  translate CameraFocus
}

#macro LightSource(Pos,Color)
  light_source {
    Pos
    color Color
    spotlight
    point_at <0,0,0>
    radius  175/vlength(Pos)
    falloff 200/vlength(Pos)
    area_light x*vlength(Pos)/10, y*vlength(Pos)/10, 9,9 adaptive 1 jitter circular orient
  }
  
#end

LightSource(<-500,500,-500>,TestRed   + <0.2,0.2,0.2>)
LightSource(<   0,500,-500>,TestGreen + <0.2,0.2,0.2>)
LightSource(< 500,500,-500>,TestBlue  + <0.2,0.2,0.2>)

// ----------------------------------------

#macro DarkStripeBW(TargetBrightness)
  #if (TargetBrightness < 0.5)
    (0.0)
  #else
    (TargetBrightness*2 - 1.0)
  #end
#end

#macro BrightStripeBW(TargetBrightness)
  #if (TargetBrightness < 0.5)
    (TargetBrightness*2)
  #else
    (1.0)
  #end
#end

#macro DarkStripeRGB(TargetColor)
  <DarkStripeBW(TargetColor.red),DarkStripeBW(TargetColor.green),DarkStripeBW(TargetColor.blue)>
#end

#macro BrightStripeRGB(TargetColor)
  <BrightStripeBW(TargetColor.red),BrightStripeBW(TargetColor.green),BrightStripeBW(TargetColor.blue)>
#end

#macro StripedPigment(TargetColor)
  #if (Stripes)
    function { abs(mod(abs(image_height*CameraDepth*y/z+0.5),2.0)-1.0) }
    color_map {
      [0.5 color rgb DarkStripeRGB(TargetColor) ]
      [0.5 color rgb BrightStripeRGB(TargetColor) ]
    }
    translate <0,0,-CameraDist>
    rotate x*CameraTilt
    translate CameraFocus
  #else
    color TargetColor
  #end
#end


plane {
  y, 0
  texture { T_Stone11 }
  interior { ior 1.5 }
}

#macro GammaAdjust(C,G)
  #local C2 = color rgbft <pow(C.red,G),pow(C.green,G),pow(C.blue,G),pow(C.filter,G),pow(C.transmit,G)>;
  (C2)
#end

#macro TestSphere(Pos,Radius,TargetColor,Split)
  sphere {
    Pos + y*Radius, Radius
    texture { pigment { color GammaAdjust(TargetColor,Gamma) } }
    interior { ior 1.5 }
  }
  #if (Split)
    sphere {
      Pos + y*Radius + x*0.001, Radius
      texture { pigment { StripedPigment(TargetColor) } }
      interior { ior 1.5 }
    }
  #end
#end

TestSphere(<-2,0,1>, 1, TestRed,   true)
TestSphere(< 0,0,1>, 1, TestGreen, true)
TestSphere(< 2,0,1>, 1, TestBlue,  true)

#local Steps = 6;
#for(I,0,1,1/Steps)
  #if (I < 0.5)
    #local Color2 = TestRed;
  #else
    #local Color2 = TestBlue;
  #end
  #local P = abs(I-0.5)*2;
  TestSphere(<I*4-2,0,-0.5>, 2/Steps, (1-P)*TestGreen + P*Color2, true)
#end

#local Steps = 8;
#for(I,0,1,1/Steps)
  TestSphere(<I*4-2,0,-1.5>, 2/Steps, rgb I, true)
  TestSphere(<I*4-2,0,-2.0>, 2/Steps, GammaAdjust(rgb I, 2.2*Gamma), false)
#end
