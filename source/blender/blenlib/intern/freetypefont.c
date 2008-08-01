/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is written by Rob Haarsma (phase)
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * This code parses the Freetype font outline data to chains of Blender's beziertriples.
 * Additional information can be found at the bottom of this file.
 *
 * Code that uses exotic character maps is present but commented out.
 */

#ifdef WITH_FREETYPE2

#ifdef WIN32
#pragma warning (disable:4244)
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BBOX_H
#include FT_SIZES_H
#include <freetype/ttnameid.h>

#include "MEM_guardedalloc.h"

#include "BLI_vfontdata.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"  

#include "BIF_toolbox.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "DNA_vfont_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_curve_types.h"

#define myMIN_ASCII 	32
#define myMAX_ASCII 	255

/* local variables */
static FT_Library	library;
static FT_Error		err;


void freetypechar_to_vchar(FT_Face face, FT_ULong charcode, VFontData *vfd)
{
	// Blender
	struct Nurb *nu;
	struct VChar *che;
	struct BezTriple *bezt;
	
	// Freetype2
	FT_GlyphSlot glyph;
	FT_UInt glyph_index;
	FT_Outline ftoutline;
	float scale, height;
	float dx, dy;
	int j,k,l,m=0;
	
	// adjust font size
	height= ((double) face->bbox.yMax - (double) face->bbox.yMin);
	if(height != 0.0)
		scale = 1.0 / height;
	else
		scale = 1.0 / 1000.0;
	
	//	
	// Generate the character 3D data
	//
	// Get the FT Glyph index and load the Glyph
	glyph_index= FT_Get_Char_Index(face, charcode);
	err= FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP);
	
	// If loading succeeded, convert the FT glyph to the internal format
	if(!err)
	{
		int *npoints;
		int *onpoints;
		
		// First we create entry for the new character to the character list
		che= (VChar *) MEM_callocN(sizeof(struct VChar), "objfnt_char");
		BLI_addtail(&vfd->characters, che);
		
		// Take some data for modifying purposes
		glyph= face->glyph;
		ftoutline= glyph->outline;
		
		// Set the width and character code
		che->index= charcode;
		che->width= glyph->advance.x * scale;
		
		// Start converting the FT data
		npoints = (int *)MEM_callocN((ftoutline.n_contours)* sizeof(int),"endpoints") ;
		onpoints = (int *)MEM_callocN((ftoutline.n_contours)* sizeof(int),"onpoints") ;

		// calculate total points of each contour
		for(j = 0; j < ftoutline.n_contours; j++) {
			if(j == 0)
				npoints[j] = ftoutline.contours[j] + 1;
			else
				npoints[j] = ftoutline.contours[j] - ftoutline.contours[j - 1];
		}

		// get number of on-curve points for beziertriples (including conic virtual on-points) 
		for(j = 0; j < ftoutline.n_contours; j++) {
			l = 0;
			for(k = 0; k < npoints[j]; k++) {
				if(j > 0) l = k + ftoutline.contours[j - 1] + 1; else l = k;
					if(ftoutline.tags[l] == FT_Curve_Tag_On)
						onpoints[j]++;

				if(k < npoints[j] - 1 )
					if( ftoutline.tags[l]   == FT_Curve_Tag_Conic &&
						ftoutline.tags[l+1] == FT_Curve_Tag_Conic)
						onpoints[j]++;
			}
		}

		//contour loop, bezier & conic styles merged
		for(j = 0; j < ftoutline.n_contours; j++) {
			// add new curve
			nu  =  (Nurb*)MEM_callocN(sizeof(struct Nurb),"objfnt_nurb");
			bezt = (BezTriple*)MEM_callocN((onpoints[j])* sizeof(BezTriple),"objfnt_bezt") ;
			BLI_addtail(&che->nurbsbase, nu);

			nu->type= CU_BEZIER+CU_2D;
			nu->pntsu = onpoints[j];
			nu->resolu= 8;
			nu->flagu= CU_CYCLIC;
			nu->bezt = bezt;

			//individual curve loop, start-end
			for(k = 0; k < npoints[j]; k++) {
				if(j > 0) l = k + ftoutline.contours[j - 1] + 1; else l = k;
				if(k == 0) m = l;
					
				//virtual conic on-curve points
				if(k < npoints[j] - 1 )
				{
					if( ftoutline.tags[l] == FT_Curve_Tag_Conic && ftoutline.tags[l+1] == FT_Curve_Tag_Conic) {
						dx = (ftoutline.points[l].x + ftoutline.points[l+1].x)* scale / 2.0;
						dy = (ftoutline.points[l].y + ftoutline.points[l+1].y)* scale / 2.0;

						//left handle
						bezt->vec[0][0] = (dx +	(2 * ftoutline.points[l].x)* scale) / 3.0;
						bezt->vec[0][1] = (dy +	(2 * ftoutline.points[l].y)* scale) / 3.0;

						//midpoint (virtual on-curve point)
						bezt->vec[1][0] = dx;
						bezt->vec[1][1] = dy;

						//right handle
						bezt->vec[2][0] = (dx + (2 * ftoutline.points[l+1].x)* scale) / 3.0;
						bezt->vec[2][1] = (dy +	(2 * ftoutline.points[l+1].y)* scale) / 3.0;

						bezt->h1= bezt->h2= HD_ALIGN;
						bezt++;
					}
				}

				//on-curve points
				if(ftoutline.tags[l] == FT_Curve_Tag_On) {
					//left handle
					if(k > 0) {
						if(ftoutline.tags[l - 1] == FT_Curve_Tag_Cubic) {
							bezt->vec[0][0] = ftoutline.points[l-1].x* scale;
							bezt->vec[0][1] = ftoutline.points[l-1].y* scale;
							bezt->h1= HD_FREE;
						} else if(ftoutline.tags[l - 1] == FT_Curve_Tag_Conic) {
							bezt->vec[0][0] = (ftoutline.points[l].x + (2 * ftoutline.points[l - 1].x))* scale / 3.0;
							bezt->vec[0][1] = (ftoutline.points[l].y + (2 * ftoutline.points[l - 1].y))* scale / 3.0;
							bezt->h1= HD_FREE;
						} else {
							bezt->vec[0][0] = ftoutline.points[l].x* scale - (ftoutline.points[l].x - ftoutline.points[l-1].x)* scale / 3.0;
							bezt->vec[0][1] = ftoutline.points[l].y* scale - (ftoutline.points[l].y - ftoutline.points[l-1].y)* scale / 3.0;
							bezt->h1= HD_VECT;
						}
					} else { //first point on curve
						if(ftoutline.tags[ftoutline.contours[j]] == FT_Curve_Tag_Cubic) {
							bezt->vec[0][0] = ftoutline.points[ftoutline.contours[j]].x * scale;
							bezt->vec[0][1] = ftoutline.points[ftoutline.contours[j]].y * scale;
							bezt->h1= HD_FREE;
						} else if(ftoutline.tags[ftoutline.contours[j]] == FT_Curve_Tag_Conic) {
							bezt->vec[0][0] = (ftoutline.points[l].x + (2 * ftoutline.points[ftoutline.contours[j]].x))* scale / 3.0 ;
							bezt->vec[0][1] = (ftoutline.points[l].y + (2 * ftoutline.points[ftoutline.contours[j]].y))* scale / 3.0 ;
							bezt->h1= HD_FREE;
						} else {
							bezt->vec[0][0] = ftoutline.points[l].x* scale - (ftoutline.points[l].x - ftoutline.points[ftoutline.contours[j]].x)* scale / 3.0;
							bezt->vec[0][1] = ftoutline.points[l].y* scale - (ftoutline.points[l].y - ftoutline.points[ftoutline.contours[j]].y)* scale / 3.0;
							bezt->h1= HD_VECT;
						}
					}

					//midpoint (on-curve point)
					bezt->vec[1][0] = ftoutline.points[l].x* scale;
					bezt->vec[1][1] = ftoutline.points[l].y* scale;

					//right handle
					if(k < (npoints[j] - 1)) {
						if(ftoutline.tags[l+1] == FT_Curve_Tag_Cubic) {
							bezt->vec[2][0] = ftoutline.points[l+1].x* scale;
							bezt->vec[2][1] = ftoutline.points[l+1].y* scale;
							bezt->h2= HD_FREE;
						} else if(ftoutline.tags[l+1] == FT_Curve_Tag_Conic) {
							bezt->vec[2][0] = (ftoutline.points[l].x + (2 * ftoutline.points[l+1].x))* scale / 3.0;
							bezt->vec[2][1] = (ftoutline.points[l].y + (2 * ftoutline.points[l+1].y))* scale / 3.0;
							bezt->h2= HD_FREE;
						} else {
							bezt->vec[2][0] = ftoutline.points[l].x* scale - (ftoutline.points[l].x - ftoutline.points[l+1].x)* scale / 3.0;
							bezt->vec[2][1] = ftoutline.points[l].y* scale - (ftoutline.points[l].y - ftoutline.points[l+1].y)* scale / 3.0;
							bezt->h2= HD_VECT;
						}
					} else { //last point on curve
						if(ftoutline.tags[m] == FT_Curve_Tag_Cubic) {
							bezt->vec[2][0] = ftoutline.points[m].x* scale;
							bezt->vec[2][1] = ftoutline.points[m].y* scale;
							bezt->h2= HD_FREE;
						} else if(ftoutline.tags[m] == FT_Curve_Tag_Conic) {
							bezt->vec[2][0] = (ftoutline.points[l].x + (2 * ftoutline.points[m].x))* scale / 3.0 ;
							bezt->vec[2][1] = (ftoutline.points[l].y + (2 * ftoutline.points[m].y))* scale / 3.0 ;
							bezt->h2= HD_FREE;
						} else {
							bezt->vec[2][0] = ftoutline.points[l].x* scale - (ftoutline.points[l].x - ftoutline.points[m].x)* scale / 3.0;
							bezt->vec[2][1] = ftoutline.points[l].y* scale - (ftoutline.points[l].y - ftoutline.points[m].y)* scale / 3.0;
							bezt->h2= HD_VECT;
						}
					}

					// get the handles that are aligned, tricky...
					// DistVL2Dfl, check if the three beztriple points are on one line
					// VecLenf, see if there's a distance between the three points
					// VecLenf again, to check the angle between the handles 
					// finally, check if one of them is a vector handle 
					if((DistVL2Dfl(bezt->vec[0],bezt->vec[1],bezt->vec[2]) < 0.001) &&
						(VecLenf(bezt->vec[0], bezt->vec[1]) > 0.0001) &&
						(VecLenf(bezt->vec[1], bezt->vec[2]) > 0.0001) &&
						(VecLenf(bezt->vec[0], bezt->vec[2]) > 0.0002) &&
						(VecLenf(bezt->vec[0], bezt->vec[2]) > MAX2(VecLenf(bezt->vec[0], bezt->vec[1]), VecLenf(bezt->vec[1], bezt->vec[2]))) &&
						bezt->h1 != HD_VECT && bezt->h2 != HD_VECT)
					{
						bezt->h1= bezt->h2= HD_ALIGN;
					}
					bezt++;
				}
			}
		}
		if(npoints) MEM_freeN(npoints);
		if(onpoints) MEM_freeN(onpoints);	
	}
}

int objchr_to_ftvfontdata(VFont *vfont, FT_ULong charcode)
{
	// Freetype2
	FT_Face face;
	struct TmpFont *tf;
	
	// Find the correct FreeType font
	tf= G.ttfdata.first;
	while(tf)
	{
		if(tf->vfont == vfont)
			break;
		tf= tf->next;		
	}
	
	// What, no font found. Something strange here
	if(!tf) return FALSE;
	
	// Load the font to memory
	if(tf->pf)
	{
		err= FT_New_Memory_Face( library,
			tf->pf->data,
			tf->pf->size,
			0,
			&face);			
	}
	else
		err= TRUE;
		
	// Read the char
	freetypechar_to_vchar(face, charcode, vfont->data);
	
	// And everything went ok
	return TRUE;
}


static VFontData *objfnt_to_ftvfontdata(PackedFile * pf)
{
	// Variables
	FT_Face face;
	FT_ULong charcode = 0, lcode;
	FT_UInt glyph_index;
	const char *fontname;
	VFontData *vfd;

/*
	FT_CharMap  found = 0;
	FT_CharMap  charmap;
	FT_UShort my_platform_id = TT_PLATFORM_MICROSOFT;
	FT_UShort my_encoding_id = TT_MS_ID_UNICODE_CS;
	int         n;
*/

	// load the freetype font
	err = FT_New_Memory_Face( library,
						pf->data,
						pf->size,
						0,
						&face );

	if(err) return NULL;
/*
	for ( n = 0; n < face->num_charmaps; n++ )
	{
		charmap = face->charmaps[n];
		if ( charmap->platform_id == my_platform_id &&
			charmap->encoding_id == my_encoding_id )
		{
			found = charmap;
			break;
		}
	}

	if ( !found ) { return NULL; }

	// now, select the charmap for the face object
	err = FT_Set_Charmap( face, found );
	if ( err ) { return NULL; }
*/

	// allocate blender font
	vfd= MEM_callocN(sizeof(*vfd), "FTVFontData");

	// get the name
	fontname = FT_Get_Postscript_Name(face);
	strcpy(vfd->name, (fontname == NULL) ? "Fontname not available" : fontname);

	// Extract the first 256 character from TTF
	lcode= charcode= FT_Get_First_Char(face, &glyph_index);

	// No charmap found from the ttf so we need to figure it out
	if(glyph_index == 0)
	{
		FT_CharMap  found = 0;
		FT_CharMap  charmap;
		int n;

		for ( n = 0; n < face->num_charmaps; n++ )
		{
			charmap = face->charmaps[n];
			if (charmap->encoding == FT_ENCODING_APPLE_ROMAN)
			{
				found = charmap;
				break;
			}
		}

		err = FT_Set_Charmap( face, found );

		if( err ) 
			return NULL;

		lcode= charcode= FT_Get_First_Char(face, &glyph_index);
	}

	// Load characters
	while(charcode < 256)
	{
		// Generate the font data
		freetypechar_to_vchar(face, charcode, vfd);

		// Next glyph
		charcode = FT_Get_Next_Char(face, charcode, &glyph_index);

		// Check that we won't start infinite loop
		if(charcode <= lcode)
			break;
		lcode = charcode;
	}
	
	err = FT_Select_Charmap( face, FT_ENCODING_UNICODE );

	return vfd;	
}


static int check_freetypefont(PackedFile * pf)
{
	FT_Face			face;
	FT_GlyphSlot	glyph;
	FT_UInt			glyph_index;
/*
	FT_CharMap  charmap;
	FT_CharMap  found;
	FT_UShort my_platform_id = TT_PLATFORM_MICROSOFT;
	FT_UShort my_encoding_id = TT_MS_ID_UNICODE_CS;
	int         n;
*/
	int success = 0;

	err = FT_New_Memory_Face( library,
							pf->data,
							pf->size,
							0,
							&face );
	if(err) {
		success = 0;
	    error("This is not a valid font");
	}
	else {
/*
		for ( n = 0; n < face->num_charmaps; n++ )
		{
		  charmap = face->charmaps[n];
		  if ( charmap->platform_id == my_platform_id &&
			   charmap->encoding_id == my_encoding_id )
		  {
			found = charmap;
			break;
		  }
		}

		if ( !found ) { return 0; }

		// now, select the charmap for the face object 
		err = FT_Set_Charmap( face, found );
		if ( err ) { return 0; }
*/
		glyph_index = FT_Get_Char_Index( face, 'A' );
		err = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP);
		if(err) success = 0;
		else {
			glyph = face->glyph;
			if (glyph->format == ft_glyph_format_outline ) {
				success = 1;
			} else {
				error("Selected Font has no outline data");
				success = 0;
			}
		}
	}
	
	return success;
}


VFontData *BLI_vfontdata_from_freetypefont(PackedFile *pf)
{
	VFontData *vfd= NULL;
	int success = 0;

	//init Freetype	
	err = FT_Init_FreeType( &library);
	if(err) {
		error("Failed to load the Freetype font library");
		return 0;
	}

	success = check_freetypefont(pf);
	
	if (success) {
		vfd= objfnt_to_ftvfontdata(pf);
	}

	//free Freetype
	FT_Done_FreeType( library);
	
	return vfd;
}

int BLI_vfontchar_from_freetypefont(VFont *vfont, unsigned long character)
{
	int success = FALSE;

	if(!vfont) return FALSE;

	// Init Freetype
	err = FT_Init_FreeType(&library);
	if(err) {
		error("Failed to load the Freetype font library");
		return 0;
	}

	// Load the character
	success = objchr_to_ftvfontdata(vfont, character);
	if(success == FALSE) return FALSE;

	// Free Freetype
	FT_Done_FreeType(library);

	// Ahh everything ok
	return TRUE;
}

#endif // WITH_FREETYPE2



#if 0

// Freetype2 Outline struct

typedef struct  FT_Outline_
  {
    short       n_contours;      /* number of contours in glyph        */
    short       n_points;        /* number of points in the glyph      */

    FT_Vector*  points;          /* the outline's points               */
    char*       tags;            /* the points flags                   */
    short*      contours;        /* the contour end points             */

    int         flags;           /* outline masks                      */

  } FT_Outline;

#endif

/***//*
from: http://www.freetype.org/freetype2/docs/glyphs/glyphs-6.html#section-1

Vectorial representation of Freetype glyphs

The source format of outlines is a collection of closed paths called "contours". Each contour is
made of a series of line segments and bezier arcs. Depending on the file format, these can be
second-order or third-order polynomials. The former are also called quadratic or conic arcs, and
they come from the TrueType format. The latter are called cubic arcs and mostly come from the
Type1 format.

Each arc is described through a series of start, end and control points. Each point of the outline
has a specific tag which indicates wether it is used to describe a line segment or an arc.


The following rules are applied to decompose the contour's points into segments and arcs :

# two successive "on" points indicate a line segment joining them.

# one conic "off" point amidst two "on" points indicates a conic bezier arc, the "off" point being
  the control point, and the "on" ones the start and end points.

# Two successive cubic "off" points amidst two "on" points indicate a cubic bezier arc. There must
  be exactly two cubic control points and two on points for each cubic arc (using a single cubic 
  "off" point between two "on" points is forbidden, for example).

# finally, two successive conic "off" points forces the rasterizer to create (during the scan-line
  conversion process exclusively) a virtual "on" point amidst them, at their exact middle. This
  greatly facilitates the definition of successive conic bezier arcs. Moreover, it's the way
  outlines are described in the TrueType specification.

Note that it is possible to mix conic and cubic arcs in a single contour, even though no current
font driver produces such outlines.

                                  *            # on      
                                               * off
                               __---__
  #-__                      _--       -_
      --__                _-            -
          --__           #               \
              --__                        #
                  -#
                           Two "on" points
   Two "on" points       and one "conic" point
                            between them



                *
  #            __      Two "on" points with two "conic"
   \          -  -     points between them. The point
    \        /    \    marked '0' is the middle of the
     -      0      \   "off" points, and is a 'virtual'
      -_  _-       #   "on" point where the curve passes.
        --             It does not appear in the point
                       list.
        *




        *                # on
                   *     * off
         __---__
      _--       -_
    _-            -
   #               \
                    #

     Two "on" points
   and two "cubic" point
      between them


Each glyph's original outline points are located on a grid of indivisible units. The points are stored
in the font file as 16-bit integer grid coordinates, with the grid origin's being at (0,0); they thus
range from -16384 to 16383.

Convert conic to bezier arcs:
Conic P0 P1 P2
Bezier B0 B1 B2 B3
B0=P0
B1=(P0+2*P1)/3
B2=(P2+2*P1)/3
B3=P2

*//****/
