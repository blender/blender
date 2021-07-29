// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence Of Vision Raytracer sample file.
// BSP test scene by Lance Birch - thezone.firewave.com.au
// Render with +BM2 to enable BSP tree bounding (POV-Ray v3.7 beta 12 or later).

/***************************************************************
 * $File: //depot/povray/smp/distribution/scenes/bsp/Tango.pov $
 * $Revision: #1 $
 * $Change: 5418 $
 * $DateTime: 2011/03/06 09:25:00 $
 * $Author: jholsenback $
 **************************************************************/

#version 3.7;

global_settings {assumed_gamma 1.0}
#default {texture {finish {ambient 0.03}} pigment {rgb 1}}
background {rgb 1}

camera {ultra_wide_angle location <0,0,-55> look_at <0,0,0> angle 100 rotate <0,32,45> translate <-25,-25,0>}
light_source {0*x color rgb 2.9 area_light <8,0,0> <0,0,8> 8, 8 adaptive 2 jitter circular orient translate <100,0,-100>}

#declare StrandCorner = difference {
	cylinder {<0,0,0> <0,0,.1> 1}
	cylinder {<0,0,-0.1> <0,0,.2> .25}
	box {<-1.1,1.1,-0.1> <1.1,0,.2>}
	box {<-1.1,1.1,-0.1> <0,-1.1,.2>}
};

#declare S = seed(12);
#declare CStrand = 1;
#declare CDir = 1;

#while (CStrand <= 300)
	#declare StartOffset = (rand(S)*30)-15;
	#declare CHeight = <70-StartOffset,70+StartOffset,-CStrand/10>;

	#while ((CHeight.y > -45) & (CHeight.x > -45))
		#declare CDir = -CDir;
		#declare StrandSegLength = floor(rand(S)*12)+1;
		#if (CDir = 1)
			box {CHeight+<-0.375,0,0> CHeight+<0.375,-StrandSegLength,.1>}
			#declare CHeight = CHeight + <-0.625,-(StrandSegLength+0.625),0>;
			object {StrandCorner translate CHeight+<0,0.625,0>}
		#else
			box {CHeight+<0,-0.375,0> CHeight+<-StrandSegLength,0.375,.1>}
			#declare CHeight = CHeight + <-(StrandSegLength+0.625),-0.625,0>;
			object {StrandCorner rotate <0,0,180> translate CHeight+<0.625,0,0>}
		#end
	#end
	#if (CDir = 1)
		object {StrandCorner rotate <0,0,270> translate CHeight+<0,0.625,0>}
	#else
		object {StrandCorner rotate <0,0,270> translate CHeight+<0.625,0,0>}
	#end

	#declare CStrand = CStrand + 1;
#end
