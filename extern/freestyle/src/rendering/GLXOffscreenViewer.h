
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////
#ifndef GLXOFFSCREENVIEWER_H
# define GLXOFFSCREENVIEWER_H

# ifdef 0
//#ifndef WIN32
//
// @(#)OffScreen.h	1.4 10/11/00
//


#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <malloc.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/times.h>

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

namespace OFFSCREEN {	// I put this here to avoid conflicts between Qt and
								// X11 in the definition of INT32

	/*! \namespace OFFSCREEN
	 *  \bugs attention a la restauration de context quand il n'avait rien avant l'OffScreenArea !!
	 */

static bool OutOfMemory = false ;
static XErrorHandler oldHandler = NULL ;

static int myXErrorHandler (Display *_d, XErrorEvent *_xee)
{
	OutOfMemory = True;
	if(oldHandler)
	{
		return (*oldHandler)(_d,_xee);
	}
	else
	{
		return false;
	}

	return 0;
}

class OffScreenArea 
{
	public:
		static const int UNKNOWN_OFFSCREEN_TYPE = 0 ;
		static const int PIXMAP_OFFSCREEN_TYPE = 1 ;
		static const int PBUFFER_OFFSCREEN_TYPE = 2 ;
		static const int KEEP_PIXMAP = 1 ;

		OffScreenArea  (int type = UNKNOWN_OFFSCREEN_TYPE,GLXContext shareCtx = NULL)
		{
			DefaultType = type ;
			i_Screen = -1;
			i_Height = 0;
			i_Width  = 0;
			
			i_OffScreenAreaType = UNKNOWN_OFFSCREEN_TYPE ;
			i_GLContext = NULL;
			i_Drawable  = GLXDrawable(0);
			i_XPix      = Pixmap(0);
			i_pXVisual  = NULL;
			i_pDisplay  = NULL;

			SetDisplayAndScreen((Display *)NULL,(int)-1) ;
		}

		GLXContext GetGLXContext() { return i_GLContext ; }
		~OffScreenArea ()
		{
			DestroyOffScreenArea();
		}
	
		/*
		 * 0 : cannot allocate
		 * 1 : allocation done
		 * FIRST : try to allocate PixelBuffer (Single, double buffer)
		 * SECOND : try to allocate GLXPixmap if PixelBuffer is not available
		 */
		/*
		 * Here, we try to allocate an OffScreen Area
		 * first, with PBuffer (if this GLX_ext. is
		 * available and we can create one)
		 * second, with a pixmap 
		 */
		int AllocateOffScreenArea(int width,int height)
		{
			save_GLContext = glXGetCurrentContext();
			save_pDisplay  = glXGetCurrentDisplay();
			save_Drawable  = glXGetCurrentDrawable();

			int AlreadyThere = 0;
#ifdef A_VIRER
			static int AlreadyThere = 0;

			if ( ( width != i_Width ) || ( height != i_Height ) )
			{
				AlreadyThere = 0;
				DestroyOffScreenArea();
			}
#endif
			if(!AlreadyThere)
			{
				AlreadyThere = 1;

				/** Before to use a Pixmap, we try with pbuffer **/	
				if(TryPBuffer(false,width,height))
				{
#ifdef DEBUG
					fprintf(stderr, "Using single-buffer PBuffer for off-screen rendering.\n") ;
#endif
					return true ;
				}

				fprintf(stderr, "Cannot use a single-buffer PBuffer, trying double buffer.\n") ;

				if(TryPBuffer(true,width,height))
				{
#ifdef DEBUG
					fprintf(stderr, "Using double-buffer PBuffer for off-screen rendering.\n") ;
#endif
					return true ;
				}
#ifdef DEBUG
				fprintf(stderr, "Warning : cannot create a PBuffer, trying Pixmap\n");
#endif
				if(TryPixmap(width,height))
				{
#ifdef DEBUG
					fprintf(stderr, "Notice  : using Pixmap for offScreen rendering\n");
#endif
					return true ;
				}
#ifdef DEBUG
				fprintf (stderr, "Warning : cannot create a Pixmap\n");
#endif

				return false;
			}
			return true ;
		}

		void MakeCurrent() 
		{
			glXMakeCurrent(i_pDisplay,i_Drawable,i_GLContext);
		}
	
	protected:
		inline  Display     * XServer         ( void ) { return i_pDisplay; }
		inline  GLXContext    GraphicContext  ( void ) { return i_GLContext; }
		inline  GLXDrawable   Drawable        ( void ) { return i_Drawable; }
		inline  XVisualInfo * XVisual         ( void ) { return i_pXVisual; }

		int DefaultType ;
		int i_OffScreenAreaType;
		int i_Height;
		int i_Width;
	
		GLXContext    save_GLContext;
		Display     * save_pDisplay;
		GLXDrawable   save_Drawable;

		Display     * i_pDisplay;
		int           i_Screen;
		GLXContext    i_GLContext;
		GLXContext    i_shareContext;
		GLXDrawable   i_Drawable;
		Pixmap        i_XPix;
		XVisualInfo * i_pXVisual;
	
		/* 
		 * Define Display and screen
		 * IF display == NULL THEN try to open default Display
		 * IF screenNumber is < 0 THEN take default Screen
		 */
		void SetDisplayAndScreen ( Display *pDisplay , int Screen )
		{
			if ( pDisplay == NULL )
			    i_pDisplay = XOpenDisplay ( NULL );
			else
			    i_pDisplay = pDisplay;
			
			if ( Screen < 0 )
			    i_Screen = DefaultScreen ( i_pDisplay );
			else
			    i_Screen = Screen;
		}
		
		// 
		// Creates a PBuffer
		// 
		// <Return Values>
		// 0 : failure
		// 1 : succeed
		//  

		bool CreatePBuffer (unsigned int width, unsigned int height , int * pAttribList)
		{
#ifdef DEBUG	
			int error = 0 ;
			while((error = glGetError()) > 0)
				std::cerr << "GLError " << (void *)error << " encountered." << std::endl ;
#endif
			GLXFBConfig *pfbConfigs;
			int nbConfigs;
			static int pbAttribs[] = { GLX_LARGEST_PBUFFER, true,
												GLX_PRESERVED_CONTENTS, true,
												GLX_PBUFFER_WIDTH,0,
												GLX_PBUFFER_HEIGHT,0,
			  									None };
			
			pbAttribs[5] = width ;
			pbAttribs[7] = height ;

			// Looks for a config that matches pAttribList
			pfbConfigs = glXChooseFBConfig(i_pDisplay,i_Screen,pAttribList,&nbConfigs) ;
#ifdef DEBUG
			std::cout << nbConfigs << " found for pbuffer." << std::endl ;
#endif

			if(pfbConfigs == NULL)
				return false ;

			i_pXVisual = glXGetVisualFromFBConfig(i_pDisplay,pfbConfigs[0]);
			i_OffScreenAreaType = PBUFFER_OFFSCREEN_TYPE;

			// Sets current error handler
			OutOfMemory = False;
			oldHandler = XSetErrorHandler( myXErrorHandler );

			i_Drawable = glXCreatePbuffer(i_pDisplay,pfbConfigs[0],pbAttribs);
					
			if(i_Drawable == 0)
			{
				i_pXVisual = NULL;
				return false ;
			}
			unsigned int w=0,h=0;
			glXQueryDrawable(i_pDisplay,i_Drawable,GLX_WIDTH,&w) ;
			glXQueryDrawable(i_pDisplay,i_Drawable,GLX_HEIGHT,&h) ;

			if((w != width)||(h != height))
			{
#ifdef DEBUG
				std::cerr << "Could not allocate Pbuffer. Only size " << w << "x" << h << " found." << std::endl ;
#endif
				return false ;
			}
#ifdef DEBUG
			else
				std::cerr << "Could allocate Pbuffer. Size " << w << "x" << h << " found." << std::endl ;
#endif
#ifdef DEBUG	
			while((error = glGetError()) > 0)
				std::cerr << "GLError " << (void *)error << " encountered." << std::endl ;
#endif
			// now create GLXContext

			if((i_GLContext = glXCreateContext(i_pDisplay,i_pXVisual,NULL,true)) == NULL) 
			{
				DestroyOffScreenArea() ;
				return false ;
			}

			/* Restore original X error handler */
			(void) XSetErrorHandler( oldHandler );
		
			if(!OutOfMemory)
			{	
				i_Height = height;
				i_Width  = width;
			
				return true ;
			}
			else
				return false ;
		}

		// 
		// Creates a Pixmap
		// 
		// <Return Values>
		// false : failure
		// true  : succeed
		// 

		bool CreatePixmap (int width, int height , int * pAttribList)
		{
			int depth;
			int totdepth=0;
			XErrorHandler oldHandler;
			XVisualInfo * pvisP;
			
			pvisP = glXChooseVisual ( i_pDisplay, i_Screen , pAttribList);

			if ( pvisP == NULL)
			{
				fprintf( stderr , "Warning : no 24-bit true color visual available\n" );
				return false ;
			}
			
			OutOfMemory = False;
			oldHandler = XSetErrorHandler(myXErrorHandler);
			if(i_XPix == Pixmap(NULL))
			{
				depth = 0;
				for (unsigned int i=0,j=0; (pAttribList[i] != None) && (j<3) ; i++ )
				{
					switch ( pAttribList[i] )
					{
						case GLX_RED_SIZE: 	glXGetConfig(i_pDisplay,pvisP,GLX_RED_SIZE,&depth) ;
													totdepth += depth ;
													i++ ;
													j++ ;
					    							break;
		
						case GLX_GREEN_SIZE: glXGetConfig(i_pDisplay,pvisP,GLX_GREEN_SIZE,&depth) ;
													totdepth += depth ;
													i++ ;
													j++ ;
													break;
		
						case GLX_BLUE_SIZE: 	glXGetConfig(i_pDisplay,pvisP,GLX_BLUE_SIZE,&depth) ;
													totdepth += depth ;
													i++ ;
													j++ ;
													break;
						default:
													break;
					}
				}
			    
				fprintf(stderr,"%d bits color buffer found\n",depth) ;
				i_XPix = XCreatePixmap(i_pDisplay,RootWindow (i_pDisplay,0),width,height,totdepth);
				XSync(i_pDisplay,False);
				if(OutOfMemory)
				{
					i_XPix = Pixmap(0);
					XSetErrorHandler(oldHandler);
					oldHandler = NULL ;
					fprintf(stderr,"Warning : could not allocate Pixmap\n");
					return false ;
				}
			}
		
			// Perhaps should we verify th type of Area (Pixmap) ?
			if ( i_Drawable == GLXDrawable(NULL) )
			{
				// i_Drawable = i_XPix;
				i_Drawable = glXCreateGLXPixmap ( i_pDisplay , pvisP , i_XPix );
				XSync ( i_pDisplay , False );
				if(OutOfMemory)
			 	{ 
					i_Drawable = GLXDrawable(0); 
					DestroyOffScreenArea();
					fprintf ( stderr , "Warning : could not allocate GLX Pixmap\n"); 
					return false ;
				} 
				else
				{
					if(i_GLContext != NULL) 
					{
						glXDestroyContext ( i_pDisplay , i_GLContext );
						i_GLContext = NULL;
					}
					if((i_GLContext = glXCreateContext(i_pDisplay,pvisP,NULL,GL_FALSE)) == NULL)
					{
						DestroyOffScreenArea();
						fprintf(stderr, "Warning : could not create rendering context");
					}
				}
			}
			XSetErrorHandler(oldHandler);
			
			i_pXVisual = (i_Drawable != GLXDrawable(NULL) ? pvisP : NULL);
			
			i_Height = height;
			i_Width  = width;
			
			if(i_Drawable != GLXDrawable(NULL))
			{
				i_OffScreenAreaType = PIXMAP_OFFSCREEN_TYPE;
				return true ;
			}
			
			return false ;
		}

		bool TryPixmap(int width,int height)
		{
			int attrList[30];
			int n = 0;
		
			attrList[n++] = GLX_RED_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_GREEN_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_BLUE_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_RGBA;
			attrList[n++] = GLX_DEPTH_SIZE;
			attrList[n++] = 16;
			attrList[n++] = GLX_STENCIL_SIZE;
			attrList[n++] = 1;
			attrList[n++] = None;
		
			return CreatePixmap(width,height,attrList) ;
		}

		bool TryPBuffer(bool double_buffer,int width,int height)
		{
			int attrList[30];
			int n = 0;
		
			attrList[n++] = GLX_RENDER_TYPE;
			attrList[n++] = GLX_RGBA_BIT;
			attrList[n++] = GLX_DRAWABLE_TYPE;
			attrList[n++] = GLX_PBUFFER_BIT;
			attrList[n++] = GLX_RED_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_GREEN_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_BLUE_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_DEPTH_SIZE;
			attrList[n++] = 8;
			attrList[n++] = GLX_DOUBLEBUFFER;
			attrList[n++] = double_buffer;
			attrList[n++] = None;
		
			return CreatePBuffer(width,height,attrList) ;
		}

		void DestroyOffScreenArea()
		{
			glXMakeCurrent(save_pDisplay,save_Drawable,save_GLContext);

			switch ( i_OffScreenAreaType )
			{
				case PIXMAP_OFFSCREEN_TYPE : 	if(i_Drawable != 0)
					  										glXDestroyGLXPixmap(i_pDisplay,i_Drawable);
					
														if (i_XPix != 0)
														{
															XFreePixmap (i_pDisplay,i_XPix);
															i_XPix = 0;
														}
														break;
			   case PBUFFER_OFFSCREEN_TYPE :	if(i_Drawable != 0)
															glXDestroyPbuffer(i_pDisplay,i_Drawable);
		      										break;
				default: break;
			}
		
			if (i_GLContext != NULL)
			{
				glXDestroyContext(i_pDisplay,i_GLContext);
				i_GLContext = NULL;
			}
		  
			i_Drawable = 0;
			i_OffScreenAreaType = UNKNOWN_OFFSCREEN_TYPE;
		}
} ;

}


#include "Geom.h"
#include "NodeDrawingStyle.h"
using namespace Geometry;
//using namespace OFFSCREEN;
class GLXOffscreenViewer{
public:
  GLXOffscreenViewer(int w, int h);
  virtual ~GLXOffscreenViewer();

  /*! Adds a node directly under the root node */
    void AddNode(Node* iNode);
    /*! Detach the node iNode which must 
     *  be directly under the root node.
     */
    void DetachNode(Node *iNode);

    /*! reads the frame buffer pixels as luminance .
     *  \param x 
     *    The lower-left corner x-coordinate of the 
     *    rectangle we want to grab.
     *  \param y
     *    The lower-left corner y-coordinate of the 
     *    rectangle we want to grab.
     *  \param width
     *    The width of the rectangle we want to grab.
     *  \param height
     *    The height of the rectangle we want to grab.
     *  \params pixels
     *    The array of float (of size width*height) in which 
     *    the read values are stored.
     */
    void readPixels(int x,int y,int width,int height,float *pixels) ;

    inline void SetClearColor(const Vec3f& c) {_clearColor = c;}
    inline Vec3f getClearColor() const {return _clearColor;}

  void init();
  void draw();

protected:
  OffScreenArea *_offscreenArea;
  NodeDrawingStyle       _RootNode;
  Vec3f           _clearColor;
  GLRenderer     *_pGLRenderer;
  
};

#endif // WIN32

#endif
