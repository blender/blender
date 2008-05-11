#include "pbuffer.h"

#include <memory.h>

#define MAX_PFORMATS 256
#define MAX_ATTRIBS  32
#define PBUFFER_AS_TEXTURE 2048

//************************************************************
// Implementation of PBuffer
//************************************************************
/*!
 * Creates a PBuffer of size \p w by \p h, eith mode \p mode.
 */
PBuffer::PBuffer(const unsigned int w,
		 const unsigned int h,
		 const int format) 
  : format_(format),
    sharedContext_(false),
    sharedLists_(false),
    display_(NULL),
    glxPbuffer_(0),
    glxContext_(0), 
    width_(w),
    height_(h)
{
}    
/*!
 * Initialize the pbuffer. If \p shareContext is true, then the currently
 * active context with be shared by the pbuffer, meaning for example that
 * textures objects or display lists of the currently active GL context will
 * be available to the pbuffer.  If you want it to be the case just for
 * display lists, set \p sharedContext to false and \p shareLists to true.
 * 
 * This function is outside of the constructor for several reasons. First it
 * might raise exceptions so it cannot be done in the constructor. Secondly,
 * it is using the current context for sharing so you might want to create
 * the pbuffer at a moment where this context is not yet active, and then
 * initialise it once the context has been created and activated.
 *
 * Return false if creation failed.
 */
bool
PBuffer::create(const bool shareContext, const bool shareLists)
{
  // Set display and screen
  Display *pDisplay     = glXGetCurrentDisplay();
  if (pDisplay == NULL)
  {
    pDisplay = XOpenDisplay(NULL);
  }
  int iScreen           = DefaultScreen(pDisplay);
  GLXContext glxContext = glXGetCurrentContext();
  
  GLXFBConfig *glxConfig;
  int iConfigCount;   
    
  sharedContext_ = shareContext;
  sharedLists_ = shareLists;

  if (sharedContext_)
  {
    glxConfig = glXGetFBConfigs(pDisplay, iScreen, &iConfigCount);
    if (!glxConfig)
    {
//       "pbuffer creation error:  glXGetFBConfigs() failed"
      return false;
    }
  }  
  else
  {
    int iAttributes[2*MAX_ATTRIBS];
    int curAttrib = 0;
        
    memset(iAttributes, 0, 2*MAX_ATTRIBS*sizeof(int));        
            
    iAttributes[2*curAttrib  ] = GLX_DRAWABLE_TYPE;
    iAttributes[2*curAttrib+1] = GLX_PBUFFER_BIT;
    curAttrib++;
        
    if (format_ & ColorIndex)
    {
      iAttributes[2*curAttrib  ] = GLX_RENDER_TYPE;
      iAttributes[2*curAttrib+1] = GLX_COLOR_INDEX_BIT;
      curAttrib++;
    }
    else
    {
      iAttributes[2*curAttrib  ] = GLX_RENDER_TYPE;
      iAttributes[2*curAttrib+1] = GLX_RGBA_BIT;
      iAttributes[2*curAttrib  ] = GLX_ALPHA_SIZE;
      iAttributes[2*curAttrib+1] = 8;
      curAttrib++;
    }
        
    if (format_ & DoubleBuffer)
    {
      iAttributes[2*curAttrib  ] = GLX_DOUBLEBUFFER;
      iAttributes[2*curAttrib+1] = true;
      curAttrib++;
    }
    else
    {
      iAttributes[2*curAttrib  ] = GLX_DOUBLEBUFFER;
      iAttributes[2*curAttrib+1] = false;
      curAttrib++;
    }  
    if (format_ & DepthBuffer)
    {
      iAttributes[2*curAttrib  ] = GLX_DEPTH_SIZE;
      iAttributes[2*curAttrib+1] = 1;
      curAttrib++;
    }
    else
    {
      iAttributes[2*curAttrib  ] = GLX_DEPTH_SIZE;
      iAttributes[2*curAttrib+1] = 0;
      curAttrib++;
    }    
        
    if (format_ & StencilBuffer)
    {
      iAttributes[2*curAttrib  ] = GLX_STENCIL_SIZE;
      iAttributes[2*curAttrib+1] = 1;
      curAttrib++;
    }
    else
    {
      iAttributes[2*curAttrib  ] = GLX_STENCIL_SIZE;
      iAttributes[2*curAttrib+1] = 0;
      curAttrib++;
    }

    iAttributes[2*curAttrib  ] = None;
    
    glxConfig = glXChooseFBConfigSGIX(pDisplay, iScreen, iAttributes, &iConfigCount);
    if (!glxConfig)
    {
      // "pbuffer creation error:  glXChooseFBConfig() failed"
      return false;
    }
  }
   
  int attributes[5];
  int iCurAttrib = 0;
    
  memset(attributes, 0, 5*sizeof(int));

  attributes[2*iCurAttrib  ] = GLX_LARGEST_PBUFFER;
  attributes[2*iCurAttrib+1] = true;
  iCurAttrib++;

  attributes[2*iCurAttrib  ] = GLX_PRESERVED_CONTENTS;
  attributes[2*iCurAttrib+1] = true;
  iCurAttrib++;
    
  attributes[2*iCurAttrib  ] = None;

  glxPbuffer_ = glXCreateGLXPbufferSGIX(pDisplay, glxConfig[0],
					width_, height_, attributes);
    
  if (!glxPbuffer_)
  {
    // "pbuffer creation error:  glXCreatePbuffer() failed"
    return false;
  }
    
  if (sharedContext_)
  {
    glxContext_ = glxContext;
  }
  else
  {
    if (format_ & ColorIndex)
    {
      if (sharedLists_)
	glxContext_ = glXCreateContextWithConfigSGIX(pDisplay,
						     glxConfig[0],
						     GLX_COLOR_INDEX_TYPE,
						     glxContext, true);
      else
	glxContext_ = glXCreateContextWithConfigSGIX(pDisplay,
						     glxConfig[0],
						     GLX_COLOR_INDEX_TYPE,
						     NULL, true);
    }
    else
    {
      if (sharedLists_)
	glxContext_ = glXCreateContextWithConfigSGIX(pDisplay,
						     glxConfig[0],
						     GLX_RGBA_TYPE,
						     glxContext, true);
      else
	glxContext_ = glXCreateContextWithConfigSGIX(pDisplay,
						     glxConfig[0],
						     GLX_RGBA_TYPE,
						     NULL, true);
    }

    if (!glxConfig)
    {
      // "pbuffer creation error:  glXCreateNewContext() failed"
      return false;
    }
  }

  display_ = pDisplay;

  glXQueryGLXPbufferSGIX(display_, glxPbuffer_, GLX_WIDTH, &width_);
  glXQueryGLXPbufferSGIX(display_, glxPbuffer_, GLX_HEIGHT, &height_);

  return true;
}
/*!
 * Destroy the pbuffer
 */
PBuffer::~PBuffer()
{
  if (glxPbuffer_)
  {
    glXDestroyGLXPbufferSGIX(display_, glxPbuffer_);        
  }
}
/*!
 * Activate the pbuffer as the current GL context. All subsequents GL
 * commands will now affect the pbuffer. If you want to push/pop the current
 * OpenGL context use subclass PBufferEx instead.
 *
 * Return false if it failed.
 */
bool
PBuffer::makeCurrent()
{
  return glXMakeCurrent(display_, glxPbuffer_, glxContext_);
}
/*!
 * Return the width of the pbuffer
 */
unsigned int
PBuffer::width() const
{
  return width_;
}
/*!
 * Return the height of the pbuffer
 */
unsigned int
PBuffer::height() const
{
  return height_;
}
//************************************************************
// Implementation of PBufferEx
//************************************************************
PBufferEx::PBufferEx(const unsigned int width,
		     const unsigned int height,
		     const int mode)
  : PBuffer(width, height, mode),
    oldDisplay_(NULL),
    glxOldDrawable_(0),
    glxOldContext_(0)
{
}
/*!
 * Activate the pbuffer as the current GL context. All subsequents GL
 * commands will now affect the pbuffer. Once you are done with you GL
 * commands, you can call endCurrent() to restore the context that was active
 * when you call makeCurrent().
 *
 * Return false if it failed.
 */
bool
PBufferEx::makeCurrent()
{
  oldDisplay_ = glXGetCurrentDisplay();
  glxOldDrawable_ = glXGetCurrentDrawable();
  glxOldContext_ = glXGetCurrentContext();
  
  return PBuffer::makeCurrent();
}
/*!
 * Restore the GL context that was active when makeCurrent() was called.
 *
 * Return false if it failed.
 */
bool
PBufferEx::endCurrent()
{
  return glXMakeCurrent(oldDisplay_, glxOldDrawable_, glxOldContext_);
}

