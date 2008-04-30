#ifndef PBUFFERS_H
#define PBUFFERS_H

#ifndef WIN32
#  ifdef __MACH__
#    include <OpenGL/gl.h>
#  else
#    include <GL/gl.h>
#    include <GL/glx.h>
#  endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Interface of PBuffer
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class PBuffer
{
public:
  enum FormatOption
  {
    DoubleBuffer            = 0x0001,
    DepthBuffer             = 0x0002,
    Rgba                    = 0x0004,
    StencilBuffer           = 0x0020,
    SingleBuffer            = DoubleBuffer  << 16,
    NoDepthBuffer           = DepthBuffer   << 16,
    ColorIndex              = Rgba          << 16,
    NoStencilBuffer         = StencilBuffer << 16,
  };
     
  PBuffer(const unsigned int width,
	  const unsigned int height,
	  int format);
  bool create(const bool shareContext = false,
	      const bool shareLists = false);
  virtual ~PBuffer();
  
  virtual bool makeCurrent();
  unsigned int width() const;
  unsigned int height() const;
protected:
  ///! Flags indicating the type of pbuffer.
  int          format_;
  ///! Flag indicating if the rendering context is shared with another context.
  bool         sharedContext_;
  //! Flag indicating if display lists should be shared between rendering
  // contexts.(If the rendering context is shared (default), then display
  // lists are automatically shared).
  bool         sharedLists_; 
  Display*     display_;
  GLXPbuffer   glxPbuffer_;
  GLXContext   glxContext_;    
  unsigned int width_;
  unsigned int height_;
};
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Interface of PBufferEx
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*!
 * pbuffer layer to keep previous pbuffer states.
 */
class PBufferEx : public PBuffer
{
public:
  PBufferEx(const unsigned int width,
	    const unsigned int height,
	    const int mode );
  virtual bool makeCurrent();
  bool endCurrent();
private:
  Display*    oldDisplay_;
  GLXDrawable glxOldDrawable_;
  GLXContext  glxOldContext_;
  int	      vp[4];
};

#endif //PBUFFERS_H
#endif //WIN32
