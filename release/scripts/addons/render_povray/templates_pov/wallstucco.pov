// This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
// To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
// letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

// Persistence Of Vision Ray Tracer Scene Description File
// Desc: Brick wall & Stucco
// Date: 2000/03/14
// Updated: 2001/07/26  
// Auth: Ingo Janssen
// Updated: 2013/02/15 for 3.7
// 
// -w320 -h240
// -w800 -h600 +a0.3
//

#version 3.7;
global_settings { assumed_gamma 1.0 
                  noise_generator 1 } // change to 2 or 3.  

light_source {
   < 500, 500,500>
   rgb 1
}

camera {
   location  <0.2, 0.0,-25.0>
   look_at   <0.0, 0.0,  0.0>
   right x*image_width/image_height // keep propotions with any aspect ratio
   angle 65 
}

//====== Doing the Stucco ======

#declare L2=0.55; //clock;  //Set amount of decay of stucco, higher value is more decay. animate
#declare PWrink= pigment { //Mortar
   wrinkles
   scale 0.25
   colour_map {
      [0, rgb 0.5]
      [L2, rgb 0.96]
   }
}
#declare Stucco= pigment {    //Stucco
   granite
   scale 0.05
   colour_map {
      [0, rgb 0.96]
      [1, rgb 1]
   }
}

#declare StuMor= pigment {       //Stucco & Mortar
   pigment_pattern{
      wrinkles
      scale 0.25
   }
   pigment_map {
      [L2, PWrink]
      [L2, Stucco]
   }
}

#declare HF_Stucco=height_field {  //Turn it into a hightfield
   function 500, 500{               //500,500 for test higher is better, but watch the memory
      pigment{StuMor }
   }
   translate -0.5
   rotate -90*x
   scale <20,20,2>
   pigment {                         //Use the mortar to colour up the stucco
      pigment_pattern {
         wrinkles
         scale 0.25
      }
      color_map {
         [L2, rgb 0.5]
         [L2, rgb <1,1,0.95>]
      }
      warp {planar z, 0}
      translate <-0.5, -0.5, 0>
      rotate <180,0,0>
      scale <20,20,2>
   }
}

//====== Lay some Bricks ======

//Size           : dimension of the brick in a vector x, y, z
//Mortar         : width of the joint.
//Turbulence etc : control the stone deformation.
#macro BrickWall(Size, Mortar, Turbulence, Octaves, Lambda, Omega)

   #local Brick= pigment {
      boxed                                // one single brick ...
      scale <Size.x/2,Size.y/2,Size.z/2>
      translate <Size.x/2+Mortar,Size.y/2+Mortar,0>
      warp {repeat x*(Size.x+Mortar)}      // ... repeated over and over again.
      warp {repeat y*(2*(Size.y+Mortar))}
   }

   #declare FBrickWall= function {
      pigment {
         pigment_pattern {
            gradient y
            warp {repeat y}
            scale <1,2*(Size.y+Mortar),1>
         }
         pigment_map {
            [0.5, Brick
                  warp {                  // deforming the bricks ...
                     turbulence Turbulence
                     octaves Octaves
                     lambda Lambda
                     omega Omega
                  }
                  translate <0,-(Mortar/2),0>
            ]
            [0.5, Brick                  // ... row by row.
                  warp {
                     turbulence Turbulence
                     octaves Octaves
                     lambda Lambda
                     omega Omega
                  }
                  translate <(Size.x/2)+Mortar,(Size.y+(Mortar/2)),0>
            ]
         }
      }
   }
#end

#declare Wall=pigment {
   BrickWall(<4,1,1>,0.2,<0.05,0.1,0>,6,0.5,0.5)
   function{FBrickWall(x,y,z).gray}
   pigment_map {                    // give some stucture to the joint ...
      [0, granite
          scale 0.1
          colour_map {
            [0, rgb 0][1, rgb 0.3]
          }
      ]
      [0.05, crackle                // ... and the bricks.
             scale <3,1,1>
             turbulence 0.5
             colour_map {
               [0, rgb 0.34][1, rgb 0.5]
             }
      ]
   }
   scale 0.04
}

#declare HF_Wall=height_field {      // Build the wall
   function 500, 500 {   //500,500, for test, higher is better & slower. Watch the memory use.
      pigment{Wall}
   }
   smooth
   translate -0.5
   rotate <-90,0,0>
   scale <33,33,1>
   pigment {
      Wall
      pigment_map {
         [0, rgb 0.6]
         [0.05, wrinkles
                turbulence 0.3
                scale <2,0.3,1>
                colour_map {
                  [0.0, rgb <0.5,0.3,0.25>]
                  [0.15, rgb <0.5,0.3,0.25>/1.3]
                  [0.3, rgb <0.5,0.3,0.25>]
                  [0.6, rgb <0.6,0.3,0.25>/1.6]
                  [0.8, rgb <0.5,0.3,0.25>]
                  [1.0, rgb <0.5,0.3,0.35>/2]
               }
         ]
      }
      translate <-0.5, -0.5, 0>
      rotate -90*x
      warp {planar y, 0}
      scale <33,33,1>
   }
}

object {HF_Stucco}
object {HF_Wall  translate <0,0,-0.8>}
