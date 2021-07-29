// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence Of Vision raytracer sample file.
//============================================
// The field, new improved version  October. 2001
// Copyright Gilles Tran 2001 
// http://www.oyonale.com
//--------------------------------------------
// Render with a 2.67 ratio such as 320*120, 640*240, 1024*384, 1280*480
//--------------------------------------------
// -w320 -h120
// -w640 -h240 +a0.1
// -w768 -h288 +a0.1
// -w1024 -h384 +a0.1

// Uncomment AreaOK=true below to turn on the area light
// This will blur the shadow under the submarine
// but the rendering time will extremely slow

#version 3.6;

global_settings{ assumed_gamma 1.0 max_trace_level 15 }

#declare AreaOK=false;
//#declare AreaOK=true;

#include "colors.inc"
#include "functions.inc"
//============================================
// General
//============================================
//--------------------------------------------
// Camera
//--------------------------------------------
#declare PdV=<-20, -20, -400>;
camera{
        location PdV
        angle 65 //   direction z*2
        up y
        right x*image_width/image_height // keep propotions with any aspect ratio  //right 8*x/3
        look_at <-20, 30, 0>
}

//--------------------------------------------
// reorientation macro
//--------------------------------------------
#macro mOrient(P1,P2)
#local yV1=vnormalize(P2-P1);
#local xV1=vnormalize(vcross(yV1,z));
#local zV1=vcross(xV1,yV1);
                matrix <xV1.x,xV1.y,xV1.z,yV1.x,yV1.y,yV1.z,zV1.x,zV1.y,zV1.z,P1.x,P1.y,P1.z>
#end

//--------------------------------------------
// colors
//--------------------------------------------
#declare colWater1=rgb<0,79,159>/255;
#declare colWater2=rgb<7,146,217>/255;
#declare colWater3=rgb<82,239,238>/255;
#declare colSub=<7/255,146/255,217/255>;

//--------------------------------------------
// lights
//--------------------------------------------
light_source {<-10, 1000, -10> color colWater2*10
#if (AreaOK)
    area_light x*200,z*200, 3,3 adaptive 1 jitter orient
#end

}
light_source {<-200, -1000, -300> color colWater2*2 shadowless media_interaction off}
light_source {PdV color colWater2*2 shadowless media_interaction off}

//--------------------------------------------
// mine textures
//--------------------------------------------
#declare txtMine=texture {
        pigment{color colWater3*0.1}
        finish{ambient 0 diffuse 0.4 specular 0.03 roughness 0.2 reflection 0.05}
}
#declare txtCable=texture {
        pigment{color colWater3*0.1}
        finish{ambient 0 diffuse 0.1 specular 0.02 roughness 0.2}
}

//--------------------------------------------
// sub textures
//--------------------------------------------
#declare txtSkin=texture{
        pigment{
                function{min(1,max(0,y))}
                turbulence 0.01 omega 1.5 lambda 5 poly_wave 1.5
                color_map{[0 Clear][0.25  rgbt<0,0,0,0.7>] [0.4 rgbt<0,0,0,0.3>]}
                scale 38 translate -y*17
                }
                
        finish{ambient 0 diffuse 0.6 specular 0.1 roughness 1/10}
}
#declare trb=0.0001;
#declare pigLettre=pigment{bozo color_map{[0 White*1.3][1 White*0.5]}}
#declare txtLettre=texture{ // submarine name
        pigment {
                object {
                        text{ttf "cyrvetic.ttf" "PERSISTENCE" 10, 0.3*x
                             translate -z*0.5 scale <1,1,10>
                        }
                        pigment{color Clear}, pigment{pigLettre}
                }
                rotate y*90
        scale 1.5 translate <-10,-1,-25>
        }

        finish{ambient 0 diffuse 0.4}

}

#declare txtSub0=texture {
    pigment{rgb colSub*0.2}
    finish {ambient 0 diffuse 0.3 specular 0.05   roughness 0.1}
}
// Thanks to Bob H. for the help regarding these textures
#declare txtSubBase=texture {
    pigment {
    
        cells
        color_map {
            [.45 rgb <colSub.x*0.1,colSub.y*0.1,colSub.z*0.1>]
            [.55 rgb <colSub.x,colSub.y,colSub.z>*0.8]
        }
        scale <100,.125,1>
    }
    
    scale 3
    finish {ambient 0 diffuse 0.3 specular 0.05   roughness 0.1}
}

#declare txtSubTop=

    texture{txtSubBase}
    texture {
        pigment {
            cells
            color_map {
                [.25 rgbf <colSub.x*0.1,colSub.y*0.1,colSub.z*0.1,0>]
                [.75 rgbf <colSub.x,colSub.y,colSub.z,1>]
            }
            scale <100,0.75,1>
        }
        scale 3.5
        finish {ambient 0 diffuse 0.3 specular 0.05   roughness 0.1}
    }

    texture {
        pigment {
            cells
            color_map {
                [.25 rgbf <colSub.x*0.4,colSub.y*0.4,colSub.z*0.4,0>]
                [.75 rgbf <colSub.x,colSub.y,colSub.z,1>]
            }
            scale <100,0.45,1>
        }
        scale 2.5
        finish {ambient 0 diffuse 0.3 specular 0.05   roughness 0.1}
    }                                 
    
    texture{txtSkin}

#declare txtSubBottom=

    texture{txtSubBase}
    
    texture {
        pigment {
            cells
            color_map {
                [.25 rgbf <colSub.x*0.5,colSub.y*0.5,colSub.z*0.5,0>]
                [.75 rgbf <colSub.x,colSub.y,colSub.z,1>]
            }
            scale <100,.75,1>
        }
        scale 5
        finish {ambient 0 diffuse 0.3 specular 0.05   roughness 0.1}
    }
    
    texture {
        pigment {
            cells
            color_map {
                [0 rgbf <colSub.x*0.5,colSub.y*0.5,colSub.z*0.5,.5>]
                [1 rgbf <colSub.x,colSub.y,colSub.z,1>]
            }
            scale <100,0.25,1>
        }
        scale 5
        translate 1
        finish {ambient 0 diffuse 0.3 specular 0.05   roughness 0.1}
    } 
    
    texture{txtLettre}  
    texture{txtSkin}


//============================================
// Mine
//============================================
//--------------------------------------------
// Spikes
//--------------------------------------------

#declare Spike = union{
        #declare rSpike1=0.08;
        #declare rSpike2=rSpike1*0.3;
        #declare ySpike=0.4;
        cone{0,rSpike1,y*ySpike,rSpike2}
        sphere{0,rSpike2 translate y*ySpike}
        sphere{0,rSpike1*1.5 scale <1,0.3,1>}
        #declare i=0;#while (i<360) sphere{0,0.015 scale <2,1,2> translate <rSpike1*2.8,-0.04,0> rotate y*i} #declare i=i+30;#end
        translate y
}





//--------------------------------------------
// Mine body
//--------------------------------------------
#declare rd=seed(0);
#declare MineBody=union {
        isosurface {
                function{x*x+y*y+z*z-1 +f_noise3d(x*10,y*10,z*10)*0.05}
                max_gradient 2.492
                contained_by{sphere{0,1}}
        }

        #declare i=0;
        #while (i<360)
                #declare j=0;
                #while (j<180)
                        object{Spike rotate z*(i+rand(rd)*2) rotate y*(j+rand(rd)*2)}
                        #declare j=j+45;
                #end
                #declare i=i+45;
        #end

        object{Spike rotate 90*y}
        object{Spike rotate -90*y}
        rotate 360*rand(rd)

}

//--------------------------------------------
// Mine cable and decorative collar
//--------------------------------------------
#declare rFil=0.03;
#declare yFil=100;
#declare MineCable=isosurface{
        function{f_helix1(x,y,z,3,35,0.35*rFil,0.55*rFil,2,1,0)}
        contained_by {box {<-rFil,0,-rFil>,<rFil,yFil,rFil>}}
        max_gradient 2.552
        scale <1,-1,1>*3 translate -y
}


#declare MineCollar=lathe{
	cubic_spline
	15,
	<0.058,0.003>,<0.081,0.000>,<0.101,0.055>,<0.099,0.085>,<0.104,0.132>,<0.066,0.152>,
	<0.095,0.169>,<0.089,0.194>,<0.144,0.227>,<0.143,0.281>,<0.145,0.307>,<0.109,0.325>,
	<0.067,0.353>,<0.031,0.362>,<0.030,0.363>
	translate -y*0.363
}

//--------------------------------------------
// Mine
//--------------------------------------------
#declare Mine=union{
        object{MineBody}
        sphere{0,1 scale <0.4,0.14,0.4> translate -y*0.91}
        #declare i=0;#while (i<360) cylinder{0,-y*0.1,0.02 translate <0.35,-0.91,0> rotate y*i} #declare i=i+30;#end
        object{MineCollar scale <1.2,2,1.2> translate -y*0.92}
        object{MineCollar translate -y*2}
        object{MineCable}
        texture{txtMine}
}



//============================================
// Submarine
//============================================
#declare Sc=3; // general scaling parameter
#declare SX=6*Sc; // x scaling
#declare SYbot=10*Sc;// y scaling for the bottom
#declare SYtop=2*Sc; // y scaling for the top
#declare SZfront=20*Sc; // z scaling for the front
#declare SZrear=100*Sc;// z scaling for the rear

//--------------------------------------------
// Main parts
//--------------------------------------------
#declare Part1=blob{ // bottom front
        threshold 0.6
        sphere{0,1,1}
        cylinder{-z*2,z,0.04,-1 translate <-0.2,-0.3,1> pigment{Black}}
        cylinder{-z*2,z,0.04,-1 translate <-0.17,-0.18,1> pigment{Black}}
        sphere{0,1,1 scale <0.1,0.45,1.05>}
        sphere{0,1,1 scale <0.3,0.45,0.8>}
}
#declare Part2=blob{ // top front
        threshold 0.6
        sphere{0,1,1}
        sphere{0,1,1 scale <0.3,0.45,0.8>}
        sphere{0,1,1 scale <0.2,1.2,1.05>}
}
#declare Part3=blob{ // bottom rear
        threshold 0.6
        sphere{0,1,1}
        cylinder{-x,0,1,1 scale <0.5,0.03,0.02> translate <0,-0.05,0.45>}
        cylinder{-y,0,1,1 scale <0.03,0.2,0.02> translate <0,-0.05,0.45>}
}
#declare Part4=blob{ // top rear
        threshold 0.6
        sphere{0,1,1}
        cylinder{-y,y,2,2 scale <0.03,0.3,0.012> translate <0,0.5,0.45>}
        sphere{0,1,1 scale <0.2,1.2,0.4>}

        cylinder{-x,0,1,1 scale <0.2,0.2,0.04> rotate x*-10 translate <0,1.5,0.2>}
        cylinder{0,y,0.2,2 scale <0.6,2.5,0.4>*0.7  translate <0,-0.05,0.16>}
        cylinder{0,y,0.2,2 scale <0.4,2.5,0.4>*0.7  translate <0,-0.05,0.165>}
        cylinder{0,y,0.2,2 scale <0.2,2.5,0.4>*0.7  translate <0,-0.05,0.17>}
}


//--------------------------------------------
// Top
//--------------------------------------------
#declare HalfSubTop=union{
       difference{
                object{Part2} // top front
                plane{y,0}
                plane{z,0 inverse}
                plane{x,0 inverse}
                scale <SX,SYtop,SZfront>
        }
        difference{
                object{Part4} // top rear
                plane{y,0}
                plane{z,0}
                plane{x,0 inverse}
                scale <SX,SYtop,SZrear>
        }
}
#declare SubTop=union{
        object{HalfSubTop}
        object{HalfSubTop scale <-1,1,1>}
        texture{txtSubTop}
}
//--------------------------------------------
// Bottom
//--------------------------------------------
#declare HalfSubBottom=union{
         difference{
                object{Part1} // bottom front
                plane{y,0 inverse}
                plane{z,0 inverse}
                plane{x,0 inverse}
                scale <SX,SYbot,SZfront>
        }
        difference{
                object{Part3} // bottom rear
                plane{y,0 inverse}
                plane{z,0}
                plane{x,0 inverse}
                scale <SX,SYbot,SZrear>
        }
}

#declare SubBottom=union{
        object{HalfSubBottom}
        object{HalfSubBottom scale <-1,1,1>}
        texture{txtSubBottom}
}
//--------------------------------------------
// Decorative elements
//--------------------------------------------
#declare Balustrade=union{
        #declare rB1=0.02;
        #declare rB2=0.04;
        #declare yB=1;
        #declare rB3=yB*6;
        #declare rB4=3;
        #declare zB=20;
        #declare zB2=8;
        #declare i=0;
        #while (i<zB)
                cylinder{0,y*yB,rB1 translate z*i}
                #declare i=i+zB/12;
        #end
        cylinder{0,z*zB,rB2 translate y*yB}
        cylinder{0,z*zB,rB2 translate y*yB*0.3}
        cylinder{0,z*zB,rB2 translate y*yB*0.6}
        union{
                difference{torus{rB3,rB2 rotate z*90} plane{y,0} plane{z,0 inverse} plane{z,0 rotate x*-45}}
                cylinder{0,-z*zB*0.1,rB2 translate y*rB3 rotate x*-45}
                translate y*(yB-rB3)
        }
        union{
                difference{torus{rB4,rB2} plane{x,0 inverse} translate <0,yB,0>}
                difference{torus{rB4,rB1} plane{x,0 inverse} translate <0,yB*0.5,0>}
                #while (i<180)
                        cylinder{0,y*yB,rB1 translate -z*rB4 rotate y*i}
                        #declare i=i+180/14;
                #end
                scale <0.4,1,1>
                translate z*(rB4+zB)
        }
        union{
                difference{torus{rB3,rB2 rotate z*90} plane{y,0} plane{z,0 inverse} plane{z,0 rotate x*-65}}
                cylinder{0,-z*zB*0.1,rB2 translate y*rB3 rotate x*-65}
                translate y*(yB-rB3)
                scale <1,1,-1>
                translate z*(zB+rB4*2)
        }

}



//--------------------------------------------
// guns
//--------------------------------------------
#declare Guns0=union{
        superellipsoid{<0.3,0.3> translate z scale <0.8,1,4>}
        union{
                cone{0,0.4,z*12,0.3}
                union{
                        cone{0,0.3,z*1.5,0.5}
                        difference{
                                sphere{0,0.5}
                                cylinder{-z,z,0.3}
                                translate z*1.5
                        }
                        translate z*12
                }
                translate z*8
        }
        translate -z*3
}

#declare Wheel=blob{
        threshold 0.6
        sphere{0,1.3,1 scale <1,1.2,1>}
        cylinder{0,-y*3,0.8,1}
        #declare Teta=0;
        #while (Teta<360)
                cylinder{0,x*3.4,0.4,1  rotate y*Teta}
                cylinder{0,y,0.4,1  translate x*3 rotate y*Teta}
                sphere{0,0.6,1 translate x*3 rotate y*Teta}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+6)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+12)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+18)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+24)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+30)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+36)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+42)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+48)}
                sphere{0,0.4,1 translate x*3 rotate y*(Teta+54)}
                sphere{0,0.5,1 translate x*3 rotate y*(Teta+60)}
                sphere{0,0.5,1 translate x*3 rotate y*(Teta+66)}
                #declare Teta=Teta+72;
        #end
}
#declare Guns1=union{
             object{Guns0}
             object{Wheel rotate y*10 scale 0.7 rotate z*90 translate -x*1.5}

}
#declare Eye=union{
        torus{4.5,0.5}
        difference{
                sphere{0,4.3}
                box{-5,5 scale <1,1,0.05>}
                box{-5,5 scale <1,1,0.05> translate z}
                box{-5,5 scale <1,1,0.05> translate z*2}
                box{-5,5 scale <1,1,0.05> translate z*3}
                box{-5,5 scale <1,1,0.05> translate z*4}
                box{-5,5 scale <1,1,0.05> translate -z}
                box{-5,5 scale <1,1,0.05> translate -z*2}
                box{-5,5 scale <1,1,0.05> translate -z*3}
                box{-5,5 scale <1,1,0.05> translate -z*4}
                scale <1,0.7,1>
        }
}
#declare Ring1=union{
        cylinder{-0.2*x,0.2*x,1.2}
        torus{1.1,0.1 rotate z*90 scale <2,1,1> translate -x*0.2}
        torus{1.1,0.1 rotate z*90 scale <2,1,1> translate x*0.2}
}
#declare Elbow1=intersection{torus{2,1} plane{z,0 inverse} plane{x,0 inverse} }


#declare Thingie=union{
        torus{1.5,0.3 rotate z*90 translate -x}
        cylinder{-x,x,1.5}
        superellipsoid{<0.2,0.2> scale <1.5,2,2.5> translate x*2.5}
        object{Eye scale 1.5/7 rotate -x*90 translate <2.5,0,-2.5>}
        object{Eye scale 1.5/7 rotate -x*90 translate <2.5,0,-2.5> scale <1,1,-1>}
        sphere{0,1.5 scale <0.5,1,1> translate x*4}
        sphere{0,1.5 scale <0.5,1,1> translate x*16}
        cylinder{x*4,x*16,1.2}
        torus{1.9,0.1 rotate z*90 translate x*16.5}
        cylinder{x*16.5,x*17.5,2}
        torus{1.9,0.1 rotate z*90 translate x*17.5}
        cylinder{x*17.5,x*23,1.5}
        union{
              torus{0.5,0.1}
              intersection{torus{2.5,0.5 rotate x*90} plane{y,0 inverse} plane{x,0} translate x*2.5}
              torus{0.5,0.1 translate -x*2.5 rotate z*-30 translate x*2.5 }
              torus{0.5,0.1 translate -x*2.5 rotate z*-60 translate x*2.5 }
              torus{0.5,0.1 translate -x*2.5 rotate z*-90 translate x*2.5 }
              union{
                    cylinder{0,9*x,0.5}
                    cylinder{2*x,5*x,0.7}
                    torus{0.5,0.2 rotate z*90 translate x*2}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*2.3}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*2.6}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*2.9}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*3.2}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*3.5}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*3.8}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*4.1}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*4.4}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*4.7}
                    torus{0.5,0.2 rotate z*90 translate x*5}
                    torus{0.5,0.3 rotate z*90 translate x*8}
                    cone{0,0.7,x,0.9 translate x*8}
                    torus{0.9,0.2 rotate z*90 translate x*9}
                    translate <2.5,2.5,0>
                    }
               translate <2.5,2,1.7>
        }
        union{
              torus{0.5,0.1}
              intersection{torus{2.5,0.5 rotate x*90} plane{y,0 inverse} plane{x,0} translate x*2.5}
              torus{0.5,0.1 translate -x*2.5 rotate z*-30 translate x*2.5 }
              torus{0.5,0.1 translate -x*2.5 rotate z*-60 translate x*2.5 }
              torus{0.5,0.1 translate -x*2.5 rotate z*-90 translate x*2.5 }
              union{
                    cylinder{0,9*x,0.5}
                    cylinder{3*x,6*x,0.7}
                    torus{0.5,0.2 rotate z*90 translate x*3}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*3.3}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*3.6}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*3.9}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*4.2}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*4.5}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*4.8}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*5.1}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*5.4}
                    torus{0.7,0.2 scale <0.2,1,1> rotate z*90 translate x*5.7}
                    torus{0.5,0.2 rotate z*90 translate x*6}
                    torus{0.5,0.3 rotate z*90 translate x*8}
                    cone{0,0.7,x,0.9 translate x*8}
                    torus{0.9,0.2 rotate z*90 translate x*9}
                    translate <2.5,2.5,0>
                    }
               translate <2.5,2,-1.7>
        }
        union{
                superellipsoid{<0.2,0.2> scale <1,1.3,2.6>}
                object{Eye scale 1/7 rotate -x*90 translate z*-2.6}
                object{Eye scale 1/7 rotate -x*90 translate z*2.6}
                object{Eye scale 1/7 rotate y*90 translate <0,1.3,1.7>}
                object{Eye scale 1/7 rotate y*90 translate <0,1.3,-1.7>}
                cylinder{x,x*3,1}
                torus{1,0.2 rotate z*90 translate x*3}
                intersection{torus{4.5,1 rotate x*90} plane{y,0 inverse} plane{x,0 inverse} scale <0.5,1,1> translate <3,-4.5,0>}
                torus{1,0.3 scale <0.5,4,1> translate <3+2.25,-3,0>}
                translate <15,4.5,0>
        }
        #declare Teta=0;
        #while (Teta<360)
        union{
                box{<0,-0.1,-0.05>,<12,0.1,0.05> translate <4,1.2,0>}
                cylinder{-x,2*x,0.1 translate y*1.5}
                sphere{0,0.2 translate <20,1.5,0>}
                sphere{0,0.1 translate <16.8,2,0>}
                sphere{0,0.1 translate <17.2,2,0> rotate x*10}
                cylinder{x*20,x*23,0.18 translate y*1.5}
                rotate x*Teta
        }
        #declare Teta=Teta+20;
        #end

        translate x
}
#declare GunSupport=union{
        superellipsoid{<0.6,0.6> translate y scale <0.3,3,1> translate -z*2}
        union{
                union{
                        superellipsoid{<0.7,0.7> translate y scale <1.5,3.8,1>}
                        #declare i=0;
                        #while (i<6)
                                sphere{0,0.2 translate <-1,i+0.5,0.8>}
                                sphere{0,0.2 translate <0,i+0.1,1>}
                                sphere{0,0.2 translate <1,i+0.5,0.8>}
                                #declare i=i+0.7;
                        #end
                        rotate -x*10 translate z*0.6
                }

                cylinder{y*4,y*9,0.6}
                sphere{0,1 scale <4,1,4>}
        }
}
#declare Guns=union{
        union{
                object{Thingie rotate y*180 scale 0.5 rotate y*-90 rotate z*45 translate <0,4,5>}
                superellipsoid{<0.6,0.6> translate -z scale <0.6,1,3> translate -x*0.5}
                object{Guns1 translate -x*1.7}
                object{Guns1 translate -x*1.7 scale <-1,1,1>}
                rotate x*-20
                translate y*10
        }
        object{GunSupport}
}

#declare GunsBack=union{
        union{
                object{Thingie rotate y*180 scale 0.5 rotate y*-90 rotate z*45 translate <0,4,5>}
                superellipsoid{<0.6,0.6> translate -z scale <0.6,1,3> translate -x*0.5}
                object{Guns1 translate -x*1.7}
                object{Guns1 translate -x*1.7 scale <-1,1,1>}
                rotate x*-5
                translate y*10
        }
        object{GunSupport}
}

//--------------------------------------------
// snorkels and vertical thingies
//--------------------------------------------
#declare Snorkel1=union{
        cone{0,0.3,y*2,0.25}
        cone{y*2,0.25,y*3,0.1}
        union{
                difference{sphere{0,1 scale<0.3,0.2,0.3>}plane{y,0 inverse}}
                difference{sphere{0,1 scale<0.3,0.6,0.3>}plane{y,0}}
                translate y*3
        }
        scale <0.8,1,0.8>
}
#declare Snorkel2=blob{
        threshold 0.6
        cylinder{-y,y*4,0.2,1}
        sphere{0,0.4,1 scale <1,1,2> translate y*3.5}
        sphere{0,0.3,1 scale <3,1,1> translate y*2.5}
        scale <0.8,1,0.8>
        }
#declare Snorkel3=union{
        blob{
                threshold 0.6
                cylinder{0,y*3.4,0.25,1 scale <1,1,3>}
                cylinder{0,y*5,0.03,1 translate <0,0,-0.5>}

        }
        union{
                cylinder{0,y*4,0.03}
                sphere{0,0.1 translate y*4}
                translate  <-0.1,0,0.5>
        }
        scale <0.8,1,0.8>
}

//--------------------------------------------
// lots of decorative stuff
//--------------------------------------------
#declare nDeco=13;
#declare Deco=array[nDeco]
#declare Deco[0]=union{
        cylinder{0,y*2,0.2}
        torus{1,0.2 rotate x*90 translate y*3}
        scale 0.5
}
#declare Deco[1]=cone{-y*0.5,0.2,y*4,0.1}
#declare Deco[2]=blob{
        threshold 0.6
        cylinder{-x,x,0.25,1 scale <1,1,2>}
        cylinder{0,-y,0.21,1 translate -x*0.8}
        cylinder{0,-y,0.21,1 translate x*0.8}
        translate y*0.7
        scale 1
}
#declare Deco[3]=object{Deco[2] rotate y*90}
#declare Deco[4]=torus{1,0.2 rotate z*90}
#declare Deco[5]=object{Deco[3] rotate y*90 scale <1,1.4,1>}
#declare Deco[6]=union{
        cylinder{0,y*0.4,0.1}
        sphere{0,1 scale <0.1,0.1,0.5> translate y*0.4}
}
#declare Deco[7]=difference{sphere{0,1} cylinder{-z,0,0.8} scale <2,0.5,2>translate -y*0.2}
#declare Deco[8]=difference{sphere{0,1} cylinder{-z,0,0.9} scale <2,0.5,4>translate -y*0.2}
#declare Deco[9]=cone{0,0.08,y*2,0.03 scale <1,1,2>}
#declare Deco[10]=sphere{0,1 scale <0.2,0.1,0.4>}
#declare Deco[11]=object{Deco[4] scale 1.2}
#declare Deco[12]=object{Deco[5] scale 1.3}
#declare Ladder=union{
        #declare i=0;
        #while (i<9)
                object{Deco[3] scale 0.8 rotate z*90 translate y*i*0.8}
                #declare i=i+1;
        #end
}

#declare Decos=union{
        #declare rd=seed(4);
        #declare Start0=-40;
        #declare End0=40;
        #declare nstep=200;
        #declare i=0;
        #declare k=0;
        #while (i<1)
                #declare j=i;
                #declare Start=<-rand(rd)*5*(mod(k,2)*2-1),1,(1-j)*Start0+j*End0>;
                #declare Dir=y;
                #declare Norm1=<0,0,0>;
                #declare Inter=trace( SubTop, Start, Dir, Norm1);
                #if (vlength(Norm1)!=0)
                    #if (vlength(vcross(Norm1,y))<0.9)
                        #declare n=int(rand(rd)*nDeco);
                        object{Deco[n]  scale 0.4 mOrient(Inter,Inter+Norm1)}
                    #end
                #end
                #declare k=k+1;
                #declare i=i+1/nstep;
        #end
}


#declare Submarine=union{
        union{
                object{SubTop}
                object{Decos texture{txtSubTop}}
                object{Ladder translate <-1.5,4,40>}
                object{Ladder translate <1.5,4,40>}
                object{Guns rotate y*180 scale 0.3 translate <0,4,30>}
                object{GunsBack scale 0.3 translate <0,4,70>}
                union{
                        object{Snorkel1 translate z*3}
                        object{Snorkel2}
                        object{Snorkel3 translate -z*2}
                        scale 2*<1,1.1,1>
                        translate <0,10,50>
                }
                object{Balustrade scale 2.5 translate <-4,2,5>}
                object{Balustrade scale 2.5 translate <-4,2,5> scale <-1,1,1>}
                union{
                        object{Balustrade scale 2 translate <-3,2,5>}
                        object{Balustrade scale 2 translate <-3,2,5> scale <-1,1,1>}
                        rotate y*180
                        translate z*100
                }
                texture{txtSub0}
                scale <1,1.3,1>
        }
        object{SubBottom}
}



//============================================
// Final
//============================================
#declare posSub=<19,5,0>;
#declare rotSub=-15;

//--------------------------------------------
// mines
//--------------------------------------------
union{
    light_group{
        object{Mine rotate y*80 scale 14 }
        light_source{<-10,-20,-40> color rgb -4 shadowless} // negative light !!!
        translate <-110, 41, -205>
        global_lights on
    }
    light_group{
        object{Mine rotate -y*10 scale 8 }
        light_source{<-10,-20,-40> color rgb -2 shadowless}
        translate <-75, 25, -165>
        global_lights on
    }
    object{Mine rotate y*125 scale 5 translate <105, -5, -155>}
    translate y*-8
}
union{
        #declare rd=seed(0);
        #declare i=0;
        #while (i<20)

                object{Mine rotate y*125 scale 3 translate <50+rand(rd)*(200+i*10),(0.5-rand(rd))*60,i*30>}
                object{Mine rotate y*150 scale 3 translate <-50-rand(rd)*(200+i*10),(0.5-rand(rd))*60,i*30>}

                object{Mine rotate y*10 scale 3 translate <50+rand(rd)*(200+i*10),(0.5-rand(rd))*140+50+i*10,i*30>}
                object{Mine rotate y*37 scale 3 translate <-50-rand(rd)*(200+i*10),(0.5-rand(rd))*140+50+i*10,i*30>}
                #declare i=i+1;
        #end
        rotate y*rotSub translate posSub
        translate -z*150
        translate x*30
}
//--------------------------------------------
// submarine and media
//--------------------------------------------
union{
        object{Submarine scale 3/4 translate z*-10 translate y*10}
        sphere{0,1 scale 410 hollow
                texture{pigment{Clear}finish{ambient 0 diffuse 0}}
                interior{
                        media{
                                scattering {5,0.00034 eccentricity 0.7 extinction 0.8}
                                absorption <255-23,255-171,255-239>*0.0005/255
                                intervals 3
                                method 3
                        }
                }
        }

        scale 4
        rotate y*rotSub  translate posSub
}
