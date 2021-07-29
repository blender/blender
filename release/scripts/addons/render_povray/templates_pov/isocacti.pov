// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence Of Vision raytracer sample file.
// Updated: Feb-2013 for 3.7
//
// -w320 -h240
// -w800 -h600 +a0.3

#version 3.7;
global_settings { assumed_gamma 1.3 }
 
#include "stdinc.inc"
#include "arrays.inc"



sky_sphere {
	pigment {gradient y
		color_map {
			[0 color Blue*0.6]
			[1 color White]
		}
	}
}

#default {finish {ambient 0}}
#declare RS = seed(464786);
//----------------------------------------
#declare CamLoc = < 5, 10,-10>;
camera {
	location CamLoc
        right x*image_width/image_height // keep propotions with any aspect ratio
	angle 45
	look_at <0, 0, 0>
}

light_source {<-20, 30, -30>*3 color White*1.5}
light_source {CamLoc color rgb 0.3}
//----------------------------------------

#declare Ground =
isosurface {
	function {y - f_snoise3d(x/7, 0, z/2)*0.5}
	threshold 0
	max_gradient 1.1
	contained_by {box {<-100,-3,-100>, < 100, 1, 100>}}

/*	texture {
		pigment {color rgb < 1, 0.9, 0.65>}
		normal {granite bump_size 0.1 scale 0.01}
	}*/
  texture{
    pigment{
      color rgb <.518, .339, .138>
    }
    normal{
      bumps 5
      scale 0.05
    }
    finish{
      specular .3
      roughness .8
    }
  }
 
  texture{
    pigment{
      wrinkles
      scale 0.05
      color_map{
	[0.0 color rgbt <1, .847, .644, 0>]
	[0.2 color rgbt <.658, .456, .270, 1>]
	[0.4 color rgbt <.270, .191, .067, .25>]
	[0.6 color rgbt <.947, .723, .468, 0>]
	[0.8 color rgbt <.356, .250, .047, 1>]
	[1.0 color rgbt <.171, .136, .1, 1>]
      }
    }

	}
}
object {Ground}

#declare RockColors = array[5]
{
	color rgb < 0.5, 0.4, 0.35>,
	color rgb < 0.4, 0.5, 0.4>,
	color rgb < 0.8, 0.75, 0.65>,
	color rgb 0.8,
	color rgb 0.5
}

#declare CtrlPtrn = function {pattern {bozo scale < 7, 1, 2>}}
#declare L = 0;
#while(L < 750)
	#declare Pt = trace(Ground, < rand(RS)*25 - 15, 10, rand(RS)*25 - 10>, -y);
	#if(rand(RS) > CtrlPtrn(Pt.x, Pt.y, Pt.z))
//		sphere {o, 0.03 + pow(rand(RS), 2)*0.15
		isosurface {
			function {f_r(x, y, z) - 1 + f_noise3d(x, y, z)*0.5}
			threshold 0
			contained_by {sphere {o, 1}}
			#if(rand(RS) < 0.5) scale VRand_In_Box(< 1, 0.9, 1>, < 2, 1, 3>, RS) #end
			rotate y*rand(RS)*360
			translate -y*0.35
			scale 0.03 + pow(rand(RS),2)*0.35
			texture {
				pigment {Rand_Array_Item(RockColors, RS)*RRand(0.1, 1, RS)}
				normal {granite bump_size 0.5 scale 0.01}
			}
			translate Pt
		}
		#declare L = L + 1;
	#end
#end

#macro MakeSpineBunch(Pt, Dir, Jitter, Len, BaseRad, Num)
	#local L = 0;
	#while(L < Num)
		#local NewDir = vnormalize(Dir + Jitter*(< rand(RS), rand(RS), rand(RS)>*2 - 1));
		cone {Pt, BaseRad, Pt + NewDir*Len, 0}
		#local L = L + 1;
	#end
#end
#macro MakeSpineRows(Body, Stretch, AltJitter, Ridges, Bunches, Spines, SpineJitter, SpineLen, SpineRad)
	#declare J = 0;
	#local AltDelta = 180/Bunches;
	union {
		#while(J < Ridges)
			#declare K = 0;
			#while(K < Bunches)
				#declare Orig = vrotate(-y*50, x*(K + rand(RS)*AltJitter)*AltDelta);
				#declare Orig = vrotate(Orig, y*(360*J/Ridges + 360/(Ridges*4)))*Stretch;
				#declare PtNorm = y;
				#declare Pt = trace(Body, Orig,-Orig, PtNorm);
				MakeSpineBunch(Pt, PtNorm, SpineJitter, SpineLen, SpineRad, Spines)
				#declare K = K + 1;
			#end
			#declare J = J + 1;
		#end
	}
#end

#declare sinw = function (x) {(sin(x) + 1)/2}

#declare Ridges = 40;
#declare RidgeDepth = 0.075;

#declare cactus1Body =
isosurface {
	function {sqrt(x*x + pow(y - sqrt((x*x/4) + (z*z/4))*1.5, 2) + z*z) - 1 -
		(sin(atan2(x, z)*Ridges)*0.5*RidgeDepth)
	}
	threshold 0
        max_gradient 5
        
	contained_by {sphere {< 0, 0, 0>, 3.1}}
	texture {
		pigment {radial
			color_map {
				[0.00 color rgb < 0.3, 0.65, 0.4>*0.8]
				[0.65 color rgb < 0.3, 0.65, 0.4>*0.8]
				[1.00 color rgb < 0.3, 0.65, 0.4>*0.2]
			}
			frequency Ridges sine_wave
		}
		normal {dents 0.1 poly_wave 2 scale < 1, 0.15, 1>}
	}
}
#declare Cactus1 =
union {
	object {cactus1Body}
	object {MakeSpineRows(cactus1Body, 1, 0.2, Ridges, 24, 3, 0.5, 1, 0.01)
		texture {pigment {color rgb < 0.98, 0.98, 0.5>}}
	}
	scale < 1, 0.75, 1>
	translate y*0.35
}


#declare Ridges = 32;
#declare RidgeDepth = 0.1;
#declare cactus2Body =

isosurface {
	function {
		f_r(x, y*0.35, z) - 1 - sqrt(x*x + z*z)*0.2
		- (sinw(atan2(x, z)*Ridges)*RidgeDepth)
	}
	threshold 0
	max_gradient 5
	contained_by {sphere {< 0, 0, 0>, 3.1}}
	texture {
		pigment {color rgb < 0.3, 0.65, 0.4>}
		normal {bozo 0.1 scale < 1, 0.15, 1>}
	}
}

#declare Cactus2 =
union {
	object {cactus2Body}
	object {MakeSpineRows(cactus2Body, < 1, 3, 1>, 1, Ridges, 64, 3, 1, 0.5, 0.01)
		texture {pigment {color rgb < 0.98, 0.98, 0.5>}}
	}
	translate y*2
}



#declare Ridges = 75;
#declare RidgeDepth = 0.05;
#declare cactus3Body =

isosurface {
	function {
		sqrt(x*x + pow((y/1.5),2) + z*z) - 1 - sqrt(x*x + z*z)*0.2
		- (sinw(atan2(x, z)*Ridges)*RidgeDepth)
	}
	threshold 0
	max_gradient 5
	contained_by {sphere {< 0, 0, 0>, 3.1}}
	texture {
		pigment {color rgb < 0.1, 0.5, 0.25>}
		normal {bozo 0.1 scale 0.15}
	}
}
#declare Cactus3 =
union {
	object {cactus3Body}
	object {MakeSpineRows(cactus3Body, < 1, 1.5, 1>, 1, Ridges, 24, 5, 1, 0.35, 0.01)
		texture {pigment {color rgb < 0.98, 0.98, 0.85>}}
	}
	translate y*1.25
}

object {Cactus1 translate < 3, 0,-3>}
object {Cactus2}
object {Cactus3 translate <-2, 0,-3.5>}

//----------------------------------------
