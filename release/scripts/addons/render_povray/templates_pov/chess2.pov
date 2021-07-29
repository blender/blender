// This work is licensed under the POV-Ray v3.7 distribution license.
// To view a copy of this license, visit http://www.povray.org/licences/v3.7/.

// Persistence Of Vision raytracer sample file.
// POV-Ray scene description for chess board.
// By Ville Saari
// Copyright (c) 1991 Ferry Island Pixelboys
//
// This scene has 430 primitives in objects and 41 in bounding shapes and
// it takes over 40 hours to render by standard amiga.
//
// If you do some nice modifications or additions to this file, please send
// me a copy. My Internet address is:  vsaari@niksula.hut.fi
//
// -w320 -h240
// -w800 -h600 +a0.3

// Note : CHESS2.POV was created from Ville Saari's chess.pov
// -- Dan Farmer 1996
//  - Cchanged textures
//  - Added camera blur and changed focal length
//  - Use sky sphere
//  - Modularized the code
//  - Added felt pads to bottom of pieces

// remaining manual bounding commented out by Bob Hughes, August 31, 2001

#version 3.6;

global_settings {
  assumed_gamma 2.2
  }

#include "shapes.inc"
#include "colors.inc"
#include "textures.inc"
#include "skies.inc"
#include "metals.inc"
#include "woods.inc"

#declare FB_Quality_Off     =  0;
#declare FB_Quality_Fast    =  1;
#declare FB_Quality_Default =  2;
#declare FB_Quality_High    =  3;

#declare FB_Quality= FB_Quality_High;

camera {
   location <59, 20, -55>
   direction <0, 0, 2>
   up <0, 1, 0>
   right x*image_width/image_height // keep propotions with any aspect ratio
   look_at <0, -1, 1>

#if(FB_Quality != FB_Quality_Off)
      aperture 2.25
      focal_point <0, 0, 0>
#end

#switch(FB_Quality)
#case(FB_Quality_Off)
   aperture 0
        #debug "\nNo focal blur used...\n"
#break
#case (FB_Quality_Fast)
   blur_samples 7
   confidence 0.5             // default is 0.9
   variance 1/64              // default is 1/128 (0.0078125)
        #debug "\nFast focal blur used...\n"
#break
#case(FB_Quality_Default)
   blur_samples 19
   confidence 0.90            // default is 0.9
   variance 1/128             // default is 1/128 (0.0078125)
        #debug "\nDefault focal blur used...\n"
#break
#case(FB_Quality_High)
   blur_samples 37
   confidence 0.975           // default is 0.9
   variance 1/255             // default is 1/128 (0.0078125)
        #debug "\nHigh Quality focal blur used...\n"
#break
#else
        #debug "\nNo focal blur used...\n"
#end
}

light_source { <800, 600, -200> colour White }

#declare PawnBase =
union {
    intersection {
       sphere { <0, 0, 0>, 2.5 }
       plane { -y, 0 }
    }
    cylinder { 0, y*0.35, 2.5 pigment { green 0.65 } }
}

#declare PieceBase =
union {
    intersection {
       sphere { <0, 0, 0>, 3 }
       plane { -y, 0 }
    }
    cylinder { 0, y*0.35, 3.0 pigment { green 0.65 } }
}

#declare Pawn = union {
   sphere { <0, 7, 0>, 1.5 }

   sphere { <0, 0, 0>, 1
      scale <1.2, 0.3, 1.2>
      translate 5.5*y
   }

   intersection {
      plane { y, 5.5 }
      object {
         Hyperboloid_Y
         translate 5*y
         scale <0.5, 1, 0.5>
      }
      plane { -y, -2.5 }
   }

   sphere { <0, 0, 0>, 1
      scale <2, 0.5, 2>
      translate <0, 2.3, 0>
   }
   object { PawnBase }
}


#declare Rook = union {
   intersection {
      union {
         plane { +x, -0.5 }
         plane { -x, -0.5 }
         plane { y, 9 }
      }

      union {
         plane { +z, -0.5 }
         plane { -z, -0.5 }
         plane { y, 9 }
      }

      plane { y, 10 }
      object { Cylinder_Y scale <2, 1, 2> }
      object { Cylinder_Y scale <1.2, 1, 1.2> inverse }
      plane { -y, -8 }
   }

   intersection {
      plane { y, 8 }
      object { Hyperboloid_Y
         scale <1, 1.5, 1>
         translate 5.401924*y
      }
      plane { -y, -3 }
   }

   sphere { <0, 0, 0>, 1
      scale <2.5, 0.5, 2.5>
      translate 2.8*y
   }

   object { PieceBase }
}

#declare Knight = union {
   intersection {
      object { Cylinder_Z
         scale <17.875, 17.875, 1>
         translate <-18.625, 7, 0>
         inverse
      }

      object { Cylinder_Z
         scale <17.875, 17.875, 1>
         translate <18.625, 7, 0>
         inverse
      }

      object { Cylinder_X
         scale <1, 5.1, 5.1>
         translate <0, 11.2, -5>
         inverse
      }

      union {
         plane { y, 0
            rotate 30*x
            translate 9.15*y
         }
         plane { z, 0
            rotate -20*x
            translate 10*y
         }
      }

      union {
         plane { -y, 0
            rotate 30*x
            translate 7.15*y
         }
         plane { y, 0
            rotate 60*x
            translate 7.3*y
         }
      }

      union {
         plane { y, 0
            rotate -45*z
         }
         plane { y, 0
            rotate 45*z
         }
         translate 9*y
      }

      object { Cylinder_Y scale <2, 1, 2> }
      sphere { <0, 7, 0>, 4 }
   }

   sphere { <0, 0, 0>, 1
      scale <2.5, 0.5, 2.5>
      translate <0, 2.8, 0>
   }

   object { PieceBase }
}

#declare Bishop = union {
   sphere { <0, 10.8, 0>, 0.4 }

   intersection {
      union {
         plane { -z, -0.25 }
         plane { +z, -0.25 }
         plane { y, 0  }
         rotate 30*x
         translate 8.5*y
      }

      sphere { <0, 0, 0>, 1
         scale <1.4, 2.1, 1.4>
         translate 8.4*y
      }

      plane { -y, -7 }
   }

   sphere { <0, 0, 0>, 1
      scale <1.5, 0.4, 1.5>
      translate 7*y
   }

   intersection {
      plane { y, 7 }
      object {
         Hyperboloid_Y
         scale <0.6, 1.4, 0.6>
         translate 7*y
      }
      plane { -y, -3 }
   }

   sphere { <0, 0, 0>, 1
      scale <2.5, 0.5, 2.5>
      translate 2.8*y
   }

   object { PieceBase }
}

#declare QueenAndKing = union {
   sphere { <0, 10.5, 0>, 1.5 }

   intersection {
      union {
         sphere { <1.75, 12, 0>, 0.9  rotate 150*y }
         sphere { <1.75, 12, 0>, 0.9  rotate 120*y }
         sphere { <1.75, 12, 0>, 0.9  rotate 90*y }
         sphere { <1.75, 12, 0>, 0.9  rotate 60*y }
         sphere { <1.75, 12, 0>, 0.9  rotate 30*y }
         sphere { <1.75, 12, 0>, 0.9  }
         sphere { <1.75, 12, 0>, 0.9  rotate -30*y }
         sphere { <1.75, 12, 0>, 0.9  rotate -60*y }
         sphere { <1.75, 12, 0>, 0.9  rotate -90*y }
         sphere { <1.75, 12, 0>, 0.9  rotate -120*y }
         sphere { <1.75, 12, 0>, 0.9  rotate -150*y }
         sphere { <1.75, 12, 0>, 0.9  rotate  180*y }
         inverse
      }

      plane { y, 11.5 }

      object { QCone_Y
         scale <1, 3, 1>
         translate 5*y
      }

      plane { -y, -8 }
   }

   sphere { <0, 0, 0>, 1
      scale <1.8, 0.4, 1.8>
      translate 8*y
   }

   intersection {
      plane { y, 8 }
      object { Hyperboloid_Y
         scale <0.7, 1.6, 0.7>
         translate 7*y
      }
      plane { -y, -3 }
   }

   sphere { <0, 0, 0>, 1
      scale <2.5, 0.5, 2.5>
      translate 2.8*y
   }

   object { PieceBase }
}

#declare Queen = union {
   sphere { <0, 12.3, 0>, 0.4 }
   object { QueenAndKing }
}

#declare King = union {
   intersection {
      union {
         intersection {
            plane { y, 13 }
            plane { -y, -12.5 }
         }

         intersection {
            plane { +x, 0.25 }
            plane { -x, 0.25 }
         }
      }

      plane { +z,  0.25 }
      plane { -z,  0.25 }
      plane { +x,  0.75 }
      plane { -x,  0.75 }
      plane { +y,  13.5  }
      plane { -y,  -11.5  }
   }

   object { QueenAndKing }
}

#declare WWood = texture {
    T_Silver_3B
}

#declare BWood = texture {
    T_Gold_3C
}

#declare WPawn = object {
   Pawn

   // bounded_by { sphere { <0, 4, 0>, 4.72 } }

   texture {
      WWood
      pigment { quick_color red 0.95 green 0.62 }
   }
}

#declare BPawn = object {
   Pawn

   // bounded_by { sphere { <0, 4, 0>, 4.72 } }

   texture {
      BWood
      pigment { quick_color red 0.4 green 0.2 }
   }
}

#declare WRook = object {
   Rook

   // bounded_by { sphere { <0, 5, 0>, 5.831 } }

   texture {
      WWood
      pigment { quick_color red 0.95 green 0.62 }
   }
}

#declare BRook = object {
   Rook

   // bounded_by { sphere { <0, 5, 0>, 5.831 } }

   texture {
      BWood
      pigment { quick_color red 0.4 green 0.2 }
   }
}

#declare WKnight = object {
   Knight

   // bounded_by { sphere { <0, 5, 0>, 5.831 } }

   texture {
      WWood
      pigment { quick_color red 0.95 green 0.62 }
   }
}

#declare BKnight = object {
   Knight
   rotate 180*y

   // bounded_by { sphere { <0, 5, 0>, 5.831 } }

   texture {
      BWood
      pigment { quick_color red 0.4 green 0.2 }
   }
}

#declare WBishop = object {
   Bishop

   // bounded_by { sphere { <0, 5.5, 0>, 6.265 } }

   texture {
      WWood
      pigment { quick_color red 0.95 green 0.62 }
   }
}

#declare BBishop = object {
   Bishop
   rotate 180*y

   // bounded_by { sphere { <0, 5.5 ,0>, 6.265 } }

   texture {
      BWood
      pigment { quick_color red 0.4 green 0.2 }
   }
}

#declare WQueen = object {
   Queen
/*
   bounded_by {
      intersection {
         sphere { <0, 6, 0>, 6.71 }
         object { Cylinder_Y scale <3, 1, 3> }
      }
   }
*/
   texture {
      WWood
      pigment { quick_color red 0.95 green 0.62 }
   }
}

#declare BQueen = object {
   Queen
/*
   bounded_by {
      intersection {
         sphere { <0, 6, 0>, 6.71 }
         object { Cylinder_Y scale <3, 1, 3> }
      }
   }
*/
   texture {
      BWood
      pigment { quick_color red 0.4 green 0.2 }
   }
}

#declare WKing = object {
   King
/*
   bounded_by {
      intersection {
         sphere { <0, 6.5, 0>, 7.16 }
         object { Cylinder_Y scale <3, 1, 3> }
      }
   }
*/
   texture {
      WWood
      pigment { quick_color red 0.95 green 0.62 }
   }
}

#declare BKing = object {
   King
/*
   bounded_by {
      intersection {
         sphere { <0, 6.5, 0>, 7.16 }
         object { Cylinder_Y scale <3, 1, 3> }
      }
   }
*/
   texture {
      BWood
      pigment { quick_color red 0.4 green 0.2 }
   }
}

/* Sky */
#declare SkySphere = sky_sphere { S_Cloud1 }

/* Ground */
#declare Ground =
plane { y, -80
   pigment { green 0.65 }
   finish {
      ambient 0.25
      diffuse 0.5
   }
}

#declare FarSide =
union {
   object { BPawn translate <-28, 0, 20> }
   object { BPawn translate <-20, 0, 20> }
   object { BPawn translate <-12, 0, 20> }
   object { BPawn translate < -4, 0, 20> }
   object { BPawn translate <  4, 0, 20> }
   object { BPawn translate < 12, 0, 20> }
   object { BPawn translate < 20, 0, 20> }
   object { BPawn translate < 28, 0, 20> }

   object { BRook   translate <-28, 0, 28> }
   object { BKnight translate <-20, 0, 28> }
   object { BBishop translate <-12, 0, 28> }
   object { BQueen  translate < -4, 0, 28> }
   object { BKing   translate <  4, 0, 28> }
   object { BBishop translate < 12, 0, 28> }
   object { BKnight translate < 20, 0, 28> }
   object { BRook   translate < 28, 0, 28> }
/*
  bounded_by {
      object {
         Cylinder_X
         scale <1, 9.56, 9.56>
         translate <0, 6.5, 24>
      }
   }
*/
}

#declare NearSide =
union {
   object { WPawn translate <-28, 0, -20> }
   object { WPawn translate <-20, 0, -20> }
   object { WPawn translate <-12, 0, -20> }
   object { WPawn translate < -4, 0, -20> }
   object { WPawn translate <  4, 0, -20> }
   object { WPawn translate < 12, 0, -20> }
   object { WPawn translate < 20, 0, -20> }
   object { WPawn translate < 28, 0, -20> }

   object { WRook   translate <-28, 0, -28> }
   object { WKnight translate <-20, 0, -28> }
   object { WBishop translate <-12, 0, -28> }
   object { WQueen  translate < -4, 0, -28> }
   object { WKing   translate <  4, 0, -28> }
   object { WBishop translate < 12, 0, -28> }
   object { WKnight translate < 20, 0, -28> }
   object { WRook   translate < 28, 0, -28> }


}

#declare Pieces =
union {
   object { NearSide }
   object { FarSide }
/*
  bounded_by {
     intersection {
         plane { y, 13.5 }
         sphere { -30*y, 63 }
      }
   }
*/
}

#declare FramePiece =
intersection {
   plane { +y, -0.15 }
   plane { -y, 3 }
   plane { -z, 35 }
   plane { <-1, 0, 1>, 0 }      // 45 degree bevel
   plane { < 1, 0, 1>, 0 }      // 45 degree bevel
}

#declare Frame =
union {
   union {
      object { FramePiece }
      object { FramePiece rotate 180*y }
      texture {
         T_Wood20
         scale 2
         rotate y*87
         translate x*1
         finish {
            specular 1
            roughness 0.02
            ambient 0.35
          }
      }
   }

   union {
      object { FramePiece rotate -90*y }
      object { FramePiece rotate  90*y }
      texture {
         T_Wood20
         scale 2
         rotate y*2
         finish {
            specular 1
            roughness 0.02
            ambient 0.35
          }
      }
   }
}
#declare Board =
   box { <-32, -1, -32> <32, 0, 32>
      texture {
         tiles {
            texture {
               pigment {
                  //marble
                  wrinkles
                  turbulence 1.0
                  colour_map {
                     [0.0 0.7 colour White
                              colour White]
                     [0.7 0.9 colour White
                              colour red 0.8 green 0.8 blue 0.8]
                     [0.9 1.0 colour red 0.8 green 0.8 blue 0.8
                              colour red 0.5 green 0.5 blue 0.5]
                  }
                  scale <0.6, 1, 0.6>
                  rotate -30*y
               }
               finish {
                  specular 1
                  roughness 0.02
                  reflection 0.25
               }
            } // texture
            tile2
            texture {
               pigment {
                  granite
                  scale <0.3, 1, 0.3>
                  colour_map {
                     [0 1 colour Black
                          colour red 0.5 green 0.5 blue 0.5]
                  }
               }
               finish {
                  specular 1
                  roughness 0.02
                  reflection 0.25
               }
            }
         } // texture
         scale <8, 1, 8>
      } //texture
   } // intersection



/* Table */
#declare Table =
union {
   intersection {
      plane { +y, -3 }
      plane { -y,  8 }
      sphere { <0, -5.5, 0>, 55 }
   }

   intersection {
      plane { y, -8 }
      object {
         Hyperboloid_Y
         scale <10, 20, 10>
         translate -20*y
      }
   }

   pigment {
      granite
      scale 6
   }
   finish {
      specular 1
      roughness 0.02
      reflection 0.3
   }
}

object { Pieces }
object { Board }
object { Frame }
object { Ground }
object { Table }
sky_sphere { SkySphere }
