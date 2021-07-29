// This work is licensed under the Creative Commons Attribution 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by/3.0/
// or send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View,
// California, 94041, USA.

// Persistence Of Vision Ray Tracer Scene Description File
// File: cornell.pov
// Desc: Radiosity demo scene. See also http://www.Graphics.Cornell.EDU/online/box/
// Date: August 2001
// Auth: Kari Kivisalo

// +w300 +h300

#version 3.7;
global_settings {
  assumed_gamma 1.0
  radiosity{
    pretrace_start 0.04
    pretrace_end 0.01
    count 200
    recursion_limit 3
    nearest_count 10
    error_bound 0.5
  }
}

#declare Finish=finish{diffuse 0.75 ambient 0}

#declare White=texture{pigment{rgb<1,1,1>} finish{Finish}}
#declare Red=texture{pigment{rgb<0.57,0.025,0.025>} finish{Finish}}
#declare Green=texture{pigment{rgb<0.025,0.236,0.025>} finish{Finish}}

#declare LightColor=<1,0.67,0.21>;

#declare N=3;       // Divisions per side
#declare DX=13/N;   // Dimensions of sub patches
#declare DZ=10.5/N;

#declare SubPatch=
  light_source{
    <27.8,54.88,27.95>
    color LightColor*7
    area_light DX*x, DZ*z, 4, 4 jitter adaptive 0
    spotlight radius -90 falloff 90 tightness 1 point_at <27.8,0,27.95> // for cosine falloff
    fade_power 2 fade_distance  (DX+DZ)/2
  }

#declare i=0;#while (i<N)
  #declare j=0;#while (j<N)
     light_source{SubPatch translate<i*DX-(13-DX)/2,0,j*DZ-(10.5-DZ)/2>}
  #declare j=j+1;#end
#declare i=i+1;#end




camera{
  location  <27.8, 27.3,-80.0>
  direction <0, 0, 1>
  up        <0, 1, 0>
  right     <-1, 0, 0>
  angle 39.5
}


// ------------------------ OBJECTS ----------------------------

// Light Patch

box{
  <21.3,54.87,33.2><34.3,54.88,22.7> no_shadow
  pigment{rgb<1,1,1>} finish{ambient 0.78 diffuse 0}
}

union{
  // Floor
  triangle{<55.28, 0.0, 0.0>,<0.0, 0.0, 0.0>,<0.0, 0.0, 55.92>}
  triangle{<55.28, 0.0, 0.0>,<0.0, 0.0, 55.92>,<54.96, 0.0, 55.92>}
  // Ceiling
  triangle{<55.60, 54.88, 0.0>,<55.60, 54.88, 55.92>,<0.0, 54.88, 55.92>}
  triangle{<55.60, 54.88, 0.0>,<0.0, 54.88, 55.92>,<0.0, 54.88, 0.0>}
  // Back wall
  triangle{<0.0, 54.88, 55.92>,<55.60, 54.88, 55.92>,<54.96, 0.0, 55.92>}
  triangle{<0.0, 54.88, 55.92>,<54.96, 0.0, 55.92>,<0.0, 0.0, 55.92>}
  texture {White}
}

union {
  // Right wall
  triangle{<0.0, 54.88, 0.0>,<0.0, 54.88, 55.92>,<0.0, 0.0, 55.92>}
  triangle{<0.0, 54.88, 0.0>,<0.0, 0.0, 55.92>,<0.0, 0.0, 0.0>}
  texture {Green}
}

union {
  // Left wall
  triangle{<55.28, 0.0, 0.0>,<54.96, 0.0, 55.92>,<55.60, 54.88, 55.92>}
  triangle{<55.28, 0.0, 0.0>,<55.60, 54.88, 55.92>,<55.60, 54.88, 0.0>}
  texture {Red}
}

union {
  // Short block
  triangle{<13.00, 16.50, 6.50>,<8.20, 16.50, 22.50>,<24.00, 16.50, 27.20>}
  triangle{<13.00, 16.50, 6.50>,<24.00, 16.50, 27.20>,<29.00, 16.50, 11.40>}
  triangle{<29.00, 0.0, 11.40>,<29.00, 16.50, 11.40>,<24.00, 16.50, 27.20>}
  triangle{<29.00, 0.0, 11.40>,<24.00, 16.50, 27.20>,<24.00, 0.0, 27.20>}
  triangle{<13.00, 0.0, 6.50>,<13.00, 16.50, 6.50>,<29.00, 16.50, 11.40>}
  triangle{<13.00, 0.0, 6.50>,<29.00, 16.50, 11.40>,<29.00, 0.0, 11.40>}
  triangle{<8.20, 0.0, 22.50>,<8.20, 16.50, 22.50>,<13.00, 16.50, 6.50>}
  triangle{<8.20, 0.0, 22.50>,<13.00, 16.50, 6.50>,<13.00, 0.0, 6.50>}
  triangle{<24.00, 0.0, 27.20>,<24.00, 16.50, 27.20>,<8.20, 16.50, 22.50>}
  triangle{<24.00, 0.0, 27.20>,<8.20, 16.50, 22.50>,<8.20, 0.0, 22.50>}
  texture { White }
}

union {
  // Tall block
  triangle{<42.30, 33.00, 24.70>,<26.50, 33.00, 29.60>,<31.40, 33.00, 45.60>}
  triangle{<42.30, 33.00, 24.70>,<31.40, 33.00, 45.60>,<47.20 33.00 40.60>}
  triangle{<42.30, 0.0, 24.70>,<42.30, 33.00, 24.70>,<47.20, 33.00, 40.60>}
  triangle{<42.30, 0.0, 24.70>,<47.20, 33.00, 40.60>,<47.20, 0.0, 40.60>}
  triangle{<47.20, 0.0, 40.60>,<47.20, 33.00, 40.60>,<31.40, 33.00, 45.60>}
  triangle{<47.20, 0.0, 40.60>,<31.40, 33.00, 45.60>,<31.40, 0.0 45.60>}
  triangle{<31.40, 0.0, 45.60>,<31.40, 33.00, 45.60>,<26.50, 33.00, 29.60>}
  triangle{<31.40, 0.0, 45.60>,<26.50, 33.00, 29.60>,<26.50, 0.0, 29.60>}
  triangle{<26.50, 0.0, 29.60>,<26.50, 33.00, 29.60>,<42.30, 33.00, 24.70>}
  triangle{<26.50, 0.0, 29.60>,<42.30, 33.00, 24.70>,<42.30, 0.0, 24.70>}
  texture {White}
}
