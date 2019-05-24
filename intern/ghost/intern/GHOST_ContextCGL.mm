/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file ghost/intern/GHOST_ContextCGL.mm
 *  \ingroup GHOST
 *
 * Definition of GHOST_ContextCGL class.
 */

#include "GHOST_ContextCGL.h"

#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>
#include <Metal/Metal.h>

#include <vector>
#include <cassert>

static void ghost_fatal_error_dialog(const char *msg)
{
  @autoreleasepool {
    NSString *message = [NSString stringWithFormat:@"Error opening window:\n%s", msg];

    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"Quit"];
    [alert setMessageText:@"Blender"];
    [alert setInformativeText:message];
    [alert setAlertStyle:NSAlertStyleCritical];
    [alert runModal];
  }

  exit(1);
}

NSOpenGLContext *GHOST_ContextCGL::s_sharedOpenGLContext = nil;
int GHOST_ContextCGL::s_sharedCount = 0;

GHOST_ContextCGL::GHOST_ContextCGL(bool stereoVisual,
                                   NSView *metalView,
                                   CAMetalLayer *metalLayer,
                                   NSOpenGLView *openGLView)
    : GHOST_Context(stereoVisual),
      m_metalView(metalView),
      m_metalLayer(metalLayer),
      m_metalCmdQueue(nil),
      m_metalRenderPipeline(nil),
      m_openGLView(openGLView),
      m_openGLContext(nil),
      m_defaultFramebuffer(0),
      m_defaultFramebufferMetalTexture(nil),
      m_debug(false)
{
#if defined(WITH_GL_PROFILE_CORE)
  m_coreProfile = true;
#else
  m_coreProfile = false;
#endif

  if (m_metalView) {
    metalInit();
  }
}

GHOST_ContextCGL::~GHOST_ContextCGL()
{
  metalFree();

  if (m_openGLContext != nil) {
    if (m_openGLContext == [NSOpenGLContext currentContext]) {
      [NSOpenGLContext clearCurrentContext];

      if (m_openGLView) {
        [m_openGLView clearGLContext];
      }
    }

    if (m_openGLContext != s_sharedOpenGLContext || s_sharedCount == 1) {
      assert(s_sharedCount > 0);

      s_sharedCount--;

      if (s_sharedCount == 0)
        s_sharedOpenGLContext = nil;

      [m_openGLContext release];
    }
  }
}

GHOST_TSuccess GHOST_ContextCGL::swapBuffers()
{
  if (m_openGLContext != nil) {
    if (m_metalView) {
      metalSwapBuffers();
    }
    else if (m_openGLView) {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      [m_openGLContext flushBuffer];
      [pool drain];
    }
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::setSwapInterval(int interval)
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [m_openGLContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::getSwapInterval(int &intervalOut)
{
  if (m_openGLContext != nil) {
    GLint interval;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [m_openGLContext getValues:&interval forParameter:NSOpenGLCPSwapInterval];

    [pool drain];

    intervalOut = static_cast<int>(interval);

    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::activateDrawingContext()
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [m_openGLContext makeCurrentContext];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::releaseDrawingContext()
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSOpenGLContext clearCurrentContext];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

unsigned int GHOST_ContextCGL::getDefaultFramebuffer()
{
  return m_defaultFramebuffer;
}

GHOST_TSuccess GHOST_ContextCGL::updateDrawingContext()
{
  if (m_openGLContext != nil) {
    if (m_metalView) {
      metalUpdateFramebuffer();
    }
    else if (m_openGLView) {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      [m_openGLContext update];
      [pool drain];
    }

    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

static void makeAttribList(std::vector<NSOpenGLPixelFormatAttribute> &attribs,
                           bool coreProfile,
                           bool stereoVisual,
                           bool needAlpha,
                           bool softwareGL)
{
  attribs.clear();

  attribs.push_back(NSOpenGLPFAOpenGLProfile);
  attribs.push_back(coreProfile ? NSOpenGLProfileVersion3_2Core : NSOpenGLProfileVersionLegacy);

  // Pixel Format Attributes for the windowed NSOpenGLContext
  attribs.push_back(NSOpenGLPFADoubleBuffer);

  if (softwareGL) {
    attribs.push_back(NSOpenGLPFARendererID);
    attribs.push_back(kCGLRendererGenericFloatID);
  }
  else {
    attribs.push_back(NSOpenGLPFAAccelerated);
    attribs.push_back(NSOpenGLPFANoRecovery);
  }

  attribs.push_back(NSOpenGLPFAAllowOfflineRenderers);  // for automatic GPU switching

  if (stereoVisual)
    attribs.push_back(NSOpenGLPFAStereo);

  if (needAlpha) {
    attribs.push_back(NSOpenGLPFAAlphaSize);
    attribs.push_back((NSOpenGLPixelFormatAttribute)8);
  }

  attribs.push_back((NSOpenGLPixelFormatAttribute)0);
}

GHOST_TSuccess GHOST_ContextCGL::initializeDrawingContext()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

#ifdef GHOST_OPENGL_ALPHA
  static const bool needAlpha = true;
#else
  static const bool needAlpha = false;
#endif

  static bool softwareGL = getenv("BLENDER_SOFTWAREGL");  // command-line argument would be better

  std::vector<NSOpenGLPixelFormatAttribute> attribs;
  attribs.reserve(40);
  makeAttribList(attribs, m_coreProfile, m_stereoVisual, needAlpha, softwareGL);

  NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs[0]];
  if (pixelFormat == nil) {
    goto error;
  }

  m_openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                               shareContext:s_sharedOpenGLContext];
  [pixelFormat release];

  [m_openGLContext makeCurrentContext];

  if (m_debug) {
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    fprintf(stderr, "OpenGL version %d.%d%s\n", major, minor, softwareGL ? " (software)" : "");
    fprintf(stderr, "Renderer: %s\n", glGetString(GL_RENDERER));
  }

#ifdef GHOST_WAIT_FOR_VSYNC
  {
    GLint swapInt = 1;
    /* wait for vsync, to avoid tearing artifacts */
    [m_openGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
  }
#endif

  initContextGLEW();

  if (m_metalView) {
    if (m_defaultFramebuffer == 0) {
      // Create a virtual framebuffer
      [m_openGLContext makeCurrentContext];
      metalInitFramebuffer();
      initClearGL();
    }
  }
  else if (m_openGLView) {
    [m_openGLView setOpenGLContext:m_openGLContext];
    [m_openGLContext setView:m_openGLView];
    initClearGL();
  }

  [m_openGLContext flushBuffer];

  if (s_sharedCount == 0)
    s_sharedOpenGLContext = m_openGLContext;

  s_sharedCount++;

  [pool drain];

  return GHOST_kSuccess;

error:

  [pixelFormat release];

  [pool drain];

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextCGL::releaseNativeHandles()
{
  m_openGLContext = nil;
  m_openGLView = nil;
  m_metalView = nil;

  return GHOST_kSuccess;
}

/* OpenGL on Metal
 *
 * Use Metal layer to avoid Viewport lagging on macOS, see T60043. */

static const MTLPixelFormat METAL_FRAMEBUFFERPIXEL_FORMAT = MTLPixelFormatBGRA8Unorm;
static const OSType METAL_CORE_VIDEO_PIXEL_FORMAT = kCVPixelFormatType_32BGRA;

void GHOST_ContextCGL::metalInit()
{
  @autoreleasepool {
    id<MTLDevice> device = m_metalLayer.device;

    // Create a command queue for blit/present operation
    m_metalCmdQueue = (MTLCommandQueue *)[device newCommandQueue];
    [m_metalCmdQueue retain];

    // Create shaders for blit operation
    NSString *source = @R"msl(
      using namespace metal;

      struct Vertex {
        float4 position [[position]];
        float2 texCoord [[attribute(0)]];
      };

      vertex Vertex vertex_shader(uint v_id [[vertex_id]]) {
        Vertex vtx;

        vtx.position.x = float(v_id & 1) * 4.0 - 1.0;
        vtx.position.y = float(v_id >> 1) * 4.0 - 1.0;
        vtx.position.z = 0.0;
        vtx.position.w = 1.0;

        vtx.texCoord = vtx.position.xy * 0.5 + 0.5;

        return vtx;
      }

      constexpr sampler s {};

      fragment float4 fragment_shader(Vertex v [[stage_in]],
                      texture2d<float> t [[texture(0)]]) {
        return t.sample(s, v.texCoord);
      }

      )msl";

    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion1_1;

    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalInit: newLibraryWithSource:options:error: failed!");
    }

    // Create a render pipeline for blit operation
    MTLRenderPipelineDescriptor *desc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];

    desc.fragmentFunction = [library newFunctionWithName:@"fragment_shader"];
    desc.vertexFunction = [library newFunctionWithName:@"vertex_shader"];

    [desc.colorAttachments objectAtIndexedSubscript:0].pixelFormat = METAL_FRAMEBUFFERPIXEL_FORMAT;

    m_metalRenderPipeline = (MTLRenderPipelineState *)[device
        newRenderPipelineStateWithDescriptor:desc
                                       error:&error];
    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalInit: newRenderPipelineStateWithDescriptor:error: failed!");
    }
  }
}

void GHOST_ContextCGL::metalFree()
{
  if (m_metalCmdQueue) {
    [m_metalCmdQueue release];
  }
  if (m_metalRenderPipeline) {
    [m_metalRenderPipeline release];
  }
  if (m_defaultFramebufferMetalTexture) {
    [m_defaultFramebufferMetalTexture release];
  }
}

void GHOST_ContextCGL::metalInitFramebuffer()
{
  glGenFramebuffers(1, &m_defaultFramebuffer);
  updateDrawingContext();
  glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFramebuffer);
}

void GHOST_ContextCGL::metalUpdateFramebuffer()
{
  assert(m_defaultFramebuffer != 0);

  NSRect bounds = [m_metalView bounds];
  NSSize backingSize = [m_metalView convertSizeToBacking:bounds.size];
  size_t width = (size_t)backingSize.width;
  size_t height = (size_t)backingSize.height;

  {
    /* Test if there is anything to update */
    id<MTLTexture> tex = (id<MTLTexture>)m_defaultFramebufferMetalTexture;
    if (tex && tex.width == width && tex.height == height) {
      return;
    }
  }

  activateDrawingContext();

  NSDictionary *cvPixelBufferProps = @{
    (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
    (__bridge NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVPixelBufferRef cvPixelBuffer = nil;
  CVReturn cvret = CVPixelBufferCreate(kCFAllocatorDefault,
                                       width,
                                       height,
                                       METAL_CORE_VIDEO_PIXEL_FORMAT,
                                       (__bridge CFDictionaryRef)cvPixelBufferProps,
                                       &cvPixelBuffer);
  if (cvret != kCVReturnSuccess) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: CVPixelBufferCreate failed!");
  }

  // Create an OpenGL texture
  CVOpenGLTextureCacheRef cvGLTexCache = nil;
  cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault,
                                     nil,
                                     m_openGLContext.CGLContextObj,
                                     m_openGLContext.pixelFormat.CGLPixelFormatObj,
                                     nil,
                                     &cvGLTexCache);
  if (cvret != kCVReturnSuccess) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: CVOpenGLTextureCacheCreate failed!");
  }

  CVOpenGLTextureRef cvGLTex = nil;
  cvret = CVOpenGLTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, cvGLTexCache, cvPixelBuffer, nil, &cvGLTex);
  if (cvret != kCVReturnSuccess) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: "
        "CVOpenGLTextureCacheCreateTextureFromImage failed!");
  }

  unsigned int glTex;
  glTex = CVOpenGLTextureGetName(cvGLTex);

  // Create a Metal texture
  CVMetalTextureCacheRef cvMetalTexCache = nil;
  cvret = CVMetalTextureCacheCreate(
      kCFAllocatorDefault, nil, m_metalLayer.device, nil, &cvMetalTexCache);
  if (cvret != kCVReturnSuccess) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: CVMetalTextureCacheCreate failed!");
  }

  CVMetalTextureRef cvMetalTex = nil;
  cvret = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                    cvMetalTexCache,
                                                    cvPixelBuffer,
                                                    nil,
                                                    METAL_FRAMEBUFFERPIXEL_FORMAT,
                                                    width,
                                                    height,
                                                    0,
                                                    &cvMetalTex);
  if (cvret != kCVReturnSuccess) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: "
        "CVMetalTextureCacheCreateTextureFromImage failed!");
  }

  MTLTexture *tex = (MTLTexture *)CVMetalTextureGetTexture(cvMetalTex);

  if (!tex) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: CVMetalTextureGetTexture failed!");
  }

  [m_defaultFramebufferMetalTexture release];
  m_defaultFramebufferMetalTexture = [tex retain];

  glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFramebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, glTex, 0);

  [m_metalLayer setDrawableSize:CGSizeMake((CGFloat)width, (CGFloat)height)];

  CVPixelBufferRelease(cvPixelBuffer);
  CVOpenGLTextureCacheRelease(cvGLTexCache);
  CVOpenGLTextureRelease(cvGLTex);
  CFRelease(cvMetalTexCache);
  CFRelease(cvMetalTex);
}

void GHOST_ContextCGL::metalSwapBuffers()
{
  @autoreleasepool {
    updateDrawingContext();
    glFlush();

    assert(m_defaultFramebufferMetalTexture != 0);

    id<CAMetalDrawable> drawable = [m_metalLayer nextDrawable];
    if (!drawable) {
      return;
    }

    id<MTLCommandBuffer> cmdBuffer = [m_metalCmdQueue commandBuffer];

    MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    {
      auto attachment = [passDescriptor.colorAttachments objectAtIndexedSubscript:0];
      attachment.texture = drawable.texture;
      attachment.loadAction = MTLLoadActionDontCare;
      attachment.storeAction = MTLStoreActionStore;
    }

    id<MTLTexture> srcTexture = (id<MTLTexture>)m_defaultFramebufferMetalTexture;

    {
      id<MTLRenderCommandEncoder> enc = [cmdBuffer
          renderCommandEncoderWithDescriptor:passDescriptor];

      [enc setRenderPipelineState:(id<MTLRenderPipelineState>)m_metalRenderPipeline];
      [enc setFragmentTexture:srcTexture atIndex:0];
      [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

      [enc endEncoding];
    }

    [cmdBuffer presentDrawable:drawable];

    [cmdBuffer commit];
  }
}
