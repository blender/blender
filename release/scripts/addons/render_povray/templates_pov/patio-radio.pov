// This work is licensed under the Creative Commons Attribution 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by/3.0/
// or send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View,
// California, 94041, USA.

// Persistence Of Vision raytracer sample file.
//
// -w320 -h240
// -w800 -h600 +a0.3

//===================== RENAISSANCE PATIO =====================================

//===================== RADIANCE AND ENVIRONMENT SETTINGS =====================
#version 3.7;
#declare Rad_Quality = 2;

global_settings {
  assumed_gamma 1.0

#switch (Rad_Quality)
 #case (1)
  radiosity {             // --- Settings 1 (fast) ---
    pretrace_start 0.08
    pretrace_end   0.02
    count 50
    error_bound 0.5
    recursion_limit 1
  }
 #break
 #case (2)
  radiosity {             // --- Settings 2 (medium quality) ---
    pretrace_start 0.08
    pretrace_end   0.01
    count 120
    error_bound 0.25
    recursion_limit 1
  }
 #break
 #case (3)
  radiosity {             // --- Settings 3 (high quality) ---
    pretrace_start 0.08
    pretrace_end   0.005
    count 400
    error_bound 0.1
    recursion_limit 1
  }
 #break
 #case (4)
  radiosity {             // --- Settings 4 (medium quality, recursion_limit 2) ---
    pretrace_start 0.08
    pretrace_end   0.005
    count 350
    error_bound 0.15
    recursion_limit 2
  }
 #break
 #end

}

fog {
  fog_type 2
  fog_alt 1.3
  fog_offset 0
  color rgb <0.7, 0.8, 0.9>
  distance 800
}

light_source {<1000, 10000, -15000> color rgb <1.0, 0.9, 0.78>*2.3}

sphere {                  // --- Sky ---
  <0, 0, 0>, 1
  texture {
   pigment {
     gradient y
     color_map {
       [0.0 color rgb < 1.0, 1.0, 1.0 >]
       [0.3 color rgb < 0.5, 0.6, 1.0 >]
     }
   }
   finish { diffuse 0 #if (version < 3.7) ambient 1 #else emission 1 #end }
  }
  scale 10000
  hollow on
  no_shadow
}

//===================== THE SCENERY ITSELF ====================================

#include "colors.inc"

camera { location <500,150,0> 
         angle 65 // direction z 
         right     x*image_width/image_height
         look_at <0,150,320>
       }

plane {y,0 pigment {color rgb <0.776,0.706,0.706>}}

#declare Arch_01 =
union {
 difference {
  cylinder {<-20,0,0>,<20,0,0>,140}
  cylinder {<-21,0,0>,<21,0,0>,130}
  torus {130 2 rotate z*90 translate x*20}
  torus {130 2 rotate z*90 translate x*-20}
 }
 difference {
  cylinder {<-18,0,0>,<18,0,0>,130}
  cylinder {<-21,0,0>,<21,0,0>,125}
 }
 torus {139 1 rotate z*90 translate x*20}
 torus {136 1 rotate z*90 translate x*20}
 torus {139 1 rotate z*90 translate x*-20}
 torus {136 1 rotate z*90 translate x*-20}
clipped_by {plane {y,0 inverse}}
}

#macro SphereBox (Radius)
 #local SpRad = sqrt (Radius*Radius + Radius*Radius);
 intersection {
  sphere {0,SpRad}
  box {<-Radius,0,-Radius>,<Radius,Radius,Radius>}
  }
#end

#declare Column_01 = union {
 box {<-40,0,-40>,<40,50,40>}
 box {<-35,50,-35>,<35,60,35>}
 cylinder {<0,60,0>,<0,66,0>,28}
 torus {28 3 translate y*63}
 difference {
  cylinder {<0,66,0>,<0,70,0>,25}
  torus {25 2 translate y*68}
  }
 cylinder {<0,70,0>,<0,74,0>,25}
 torus {25 2 translate y*72}
 cylinder {<0,74,0>,<0,76,0>,25}
 sphere {<0,0,0>,23 scale <1,15,1> translate y*76 clipped_by {cylinder {<0,76,0>,<0,265,0>,30}}}
 torus {20 2 translate y*255}
 torus {19 2 translate y*258}
 object {SphereBox (20) rotate z*180 translate y*(260+22)}
 box {<-25,282,-25>,<25,285,25>}
 box {<-20,285,-22>,<20,295,22>}
 difference {
  cylinder {<-22,290,0>,<22,290,0>,5}
  cylinder {<-23,290,0>,<23,290,0>,3}
  }
 box {<-23,295,-23>,<23,298,23>}
 box {<-28,298,-28>,<28,300,28>}
}

#declare Vault_01 =
difference {
 box {<-160,0,-160>,<160,250,160>}
 cylinder {<-170,0,0>,<170,0,0>,130}
 cylinder {<-170,0,0>,<170,0,0>,130 rotate y*90}
}

#declare Vault_02 = //(vault de coin)
difference {
 union {
  box {<-180,0,-160>,<180,250,160>}
  box {<-160,0,-180>,<160,250,180>}
  }
 cylinder {<-190,0,0>,<190,0,0>,130}
 cylinder {<-190,0,0>,<190,0,0>,130 rotate y*90}
}

#declare Spindle_01 =
lathe{
	cubic_spline
	12,
	<0.017005,-0.005668>,
	<0.117619,-0.004251>,
	<0.123287,0.072272>,
	<0.068020,0.124704>,
	<0.076523,0.195559>,
	<0.141709,0.444967>,
	<0.075106,0.524324>,
	<0.138875,0.616435>,
	<0.055267,0.916859>,
	<0.137458,0.973543>,
	<0.161549,1.000468>,
	<0.204061,0.991965>
}

#declare Band_01 =
union {
 box {<0,0,-25>,<-1,60,25>}
 box {<0,0,-25>,<5,2,25>}
 box {<0,8,-25>,<3,2,25>}
 box {<0,8,-25>,<6,15,25>}
 box {<0,8,-10>,<6,15,-8>}
 box {<0,8,10>,<6,15,8>}
 box {<0,20,-25>,<3,19,25>}
 box {<0,50,-25>,<5,60,25>}
 box {<0,50,-25>,<3,55,25>}
 box {<0,20,-2>,<3,40,-4>}
 box {<0,20,-6>,<3,40,-8>}
 box {<0,20,2>,<3,40,4>}
 box {<0,20,6>,<3,40,8>}
 box {<0,42,-25>,<6,40,25>}
 box {<0,0,-2>,<7,8,-4>}
 box {<0,0,-6>,<7,8,-8>}
 box {<0,0,2>,<7,8,4>}
 box {<0,0,6>,<7,8,8>}
}

#declare Balcony_01 = union {
 box {<-10,0,-.5>,<10,10,.5>}
 cylinder {<-10,5,-.5>,<-10,5,.5>,4}
 cylinder {<10,5,-.5>,<10,5,.5>,4}
}

#declare Group1 = union {
 object {Arch_01 translate <-490,300,0>}
 object {Arch_01 translate <-490,300,300>}
 object {Arch_01 translate <-490,300,-300>}

 object {Column_01 translate <-490,0,150>}
 object {Column_01 translate <-490,0,-150>}
 object {Column_01 translate <-490,0,-450>}
 object {Column_01 translate <-490,0,450>}

 object {Column_01 translate <-790,0,150>}
 object {Column_01 translate <-790,0,-150>}
 object {Column_01 translate <-790,0,-450>}
 object {Column_01 translate <-790,0,450>}
 object {Column_01 translate <-790,0,-450-40>}
 object {Column_01 translate <-790,0,450+40>}

 object {Arch_01 rotate y*90 translate <-490-150,300,150>}
 object {Arch_01 rotate y*90 translate <-490-150,300,-150>}
 object {Arch_01 rotate y*90 translate <-490-150,300,450>}
 object {Arch_01 rotate y*90 translate <-490-150,300,-450>}
 object {Arch_01 rotate y*90 translate <-490-150,300,450+40>}//doubleaux
 object {Arch_01 rotate y*90 translate <-490-150,300,-450-40>}

 object {Vault_01 translate <-640,300,0>}
 object {Vault_01 translate <-640,300,300>}
 object {Vault_01 translate <-640,300,-300>}
 object {Vault_02 translate <-640,300,640>}//coin

 #declare I=0;
 #while (I < 1000)
  object {Band_01 translate <-480,500,(-470 + I)>}
 #declare I=I+50;
 #end

 #declare I=0;
 #while (I < 1000)
  object {Spindle_01 scale <60,60,60> translate <-500,550,(-500 + I)>}
 #declare I=I+40;
 #end

 object {Balcony_01 scale <1,1,1020> translate <-500,610,0>}
 object {Balcony_01 scale <1,1,1020> translate <-500,610,0> rotate y*90}
 object {Balcony_01 scale <1,1,1020> translate <-500,610,0> rotate y*180}
 object {Balcony_01 scale <1,1,1020> translate <-500,610,0> rotate y*270}

 box {<-790,0,-810>,<-810,450,810>}

}

#declare PatioComplete = union {
 object {Group1}
 object {Group1 rotate y*90}
 object {Group1 rotate y*180}
 object {Group1 rotate y*270}
}

object {PatioComplete
        pigment {Wheat}
        finish {ambient 0.0 diffuse 0.6}
}

#declare Paving_01 =
union {
 box {<-40,0,-490>,<40,.1,490> translate x*150}
 box {<-40,0,-490>,<40,.1,490> translate x*-150}
 box {<-40,0,-490>,<40,.1,490> translate x*490}
 box {<-40,0,-490>,<40,.1,490> translate x*-480}

 texture {
    pigment {color rgb <0.706,0.714,0.776>*.8}
    finish {ambient 0.0 diffuse 0.6}
 }
}

object {Paving_01 translate <-10,0,0>}
object {Paving_01 rotate y*90 translate <-10,0,0>}
