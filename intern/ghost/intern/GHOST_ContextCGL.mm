/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextCGL class.
 */

/* Don't generate OpenGL deprecation warning. This is a known thing, and is not something easily
 * solvable in a short term. */
#ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "GHOST_ContextCGL.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <epoxy/gl.h>

#include <cassert>
#include <vector>

static void ghost_fatal_error_dialog(const char *msg)
{
  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
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
MTLCommandQueue *GHOST_ContextCGL::s_sharedMetalCommandQueue = nil;
int GHOST_ContextCGL::s_sharedCount = 0;

GHOST_ContextCGL::GHOST_ContextCGL(bool stereoVisual,
                                   NSView *metalView,
                                   CAMetalLayer *metalLayer,
                                   NSOpenGLView *openGLView,
                                   GHOST_TDrawingContextType type)
    : GHOST_Context(stereoVisual),
      m_useMetalForRendering(type == GHOST_kDrawingContextTypeMetal),
      m_metalView(metalView),
      m_metalLayer(metalLayer),
      m_metalRenderPipeline(nil),
      m_openGLView(openGLView),
      m_openGLContext(nil),
      m_defaultFramebuffer(0),
      m_debug(false)
{
  /* Initialize Metal Swap-chain. */
  current_swapchain_index = 0;
  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    m_defaultFramebufferMetalTexture[i].texture = nil;
    m_defaultFramebufferMetalTexture[i].index = i;
  }
  if (m_metalView) {
    m_ownsMetalDevice = false;
    metalInit();
  }
  else {
    /* Prepare offscreen GHOST Context Metal device. */
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();

    if (m_debug) {
      printf("Selected Metal Device: %s\n", [metalDevice.name UTF8String]);
    }

    m_ownsMetalDevice = true;
    if (metalDevice) {
      m_metalLayer = [[CAMetalLayer alloc] init];
      [m_metalLayer setEdgeAntialiasingMask:0];
      [m_metalLayer setMasksToBounds:NO];
      [m_metalLayer setOpaque:YES];
      [m_metalLayer setFramebufferOnly:YES];
      [m_metalLayer setPresentsWithTransaction:NO];
      [m_metalLayer removeAllAnimations];
      [m_metalLayer setDevice:metalDevice];
      m_metalLayer.allowsNextDrawableTimeout = NO;
      metalInit();
    }
    else {
      ghost_fatal_error_dialog(
          "[ERROR] Failed to create Metal device for offscreen GHOST Context.\n");
    }
  }

  /* Initialize swap-interval. */
  mtl_SwapInterval = 60;
}

GHOST_ContextCGL::~GHOST_ContextCGL()
{
  metalFree();

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
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
#endif
  }

  if (m_ownsMetalDevice) {
    if (m_metalLayer) {
      [m_metalLayer release];
      m_metalLayer = nil;
    }
  }
}

GHOST_TSuccess GHOST_ContextCGL::swapBuffers()
{
  GHOST_TSuccess return_value = GHOST_kFailure;

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    if (m_openGLContext != nil) {
      if (m_metalView) {
        metalSwapBuffers();
      }
      else if (m_openGLView) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        [m_openGLContext flushBuffer];
        [pool drain];
      }
      return_value = GHOST_kSuccess;
    }
    else {
      return_value = GHOST_kFailure;
    }
#endif
  }
  else {
    if (m_metalView) {
      metalSwapBuffers();
    }
    return_value = GHOST_kSuccess;
  }
  return return_value;
}

GHOST_TSuccess GHOST_ContextCGL::setSwapInterval(int interval)
{

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    if (m_openGLContext != nil) {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      [m_openGLContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
      [pool drain];
      return GHOST_kSuccess;
    }
    else {
      return GHOST_kFailure;
    }
#endif
  }
  else {
    mtl_SwapInterval = interval;
    return GHOST_kSuccess;
  }
}

GHOST_TSuccess GHOST_ContextCGL::getSwapInterval(int &intervalOut)
{

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
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
#endif
  }
  else {
    intervalOut = mtl_SwapInterval;
    return GHOST_kSuccess;
  }
}

GHOST_TSuccess GHOST_ContextCGL::activateDrawingContext()
{

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    if (m_openGLContext != nil) {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      [m_openGLContext makeCurrentContext];
      [pool drain];
      return GHOST_kSuccess;
    }
    else {
      return GHOST_kFailure;
    }
#endif
  }
  else {
    return GHOST_kSuccess;
  }
}

GHOST_TSuccess GHOST_ContextCGL::releaseDrawingContext()
{

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    if (m_openGLContext != nil) {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      [NSOpenGLContext clearCurrentContext];
      [pool drain];
      return GHOST_kSuccess;
    }
    else {
      return GHOST_kFailure;
    }
#endif
  }
  else {
    return GHOST_kSuccess;
  }
}

unsigned int GHOST_ContextCGL::getDefaultFramebuffer()
{

  if (!m_useMetalForRendering) {
    return m_defaultFramebuffer;
  }
  /* NOTE(Metal): This is not valid. */
  return 0;
}

GHOST_TSuccess GHOST_ContextCGL::updateDrawingContext()
{

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    if (m_openGLContext != nil) {
      if (m_metalView) {
        metalUpdateFramebuffer();
      }
      else if (m_openGLView) {
        @autoreleasepool {
          [m_openGLContext update];
        }
      }

      return GHOST_kSuccess;
    }
    else {
      return GHOST_kFailure;
    }
#endif
  }
  else {
    if (m_metalView) {
      metalUpdateFramebuffer();
      return GHOST_kSuccess;
    }
  }
  return GHOST_kFailure;
}

id<MTLTexture> GHOST_ContextCGL::metalOverlayTexture()
{
  /* Increment Swap-chain - Only needed if context is requesting a new texture */
  current_swapchain_index = (current_swapchain_index + 1) % METAL_SWAPCHAIN_SIZE;

  /* Ensure backing texture is ready for current swapchain index */
  updateDrawingContext();

  /* Return texture. */
  return m_defaultFramebufferMetalTexture[current_swapchain_index].texture;
}

MTLCommandQueue *GHOST_ContextCGL::metalCommandQueue()
{
  return s_sharedMetalCommandQueue;
}
MTLDevice *GHOST_ContextCGL::metalDevice()
{
  id<MTLDevice> device = m_metalLayer.device;
  return (MTLDevice *)device;
}

void GHOST_ContextCGL::metalRegisterPresentCallback(void (*callback)(
    MTLRenderPassDescriptor *, id<MTLRenderPipelineState>, id<MTLTexture>, id<CAMetalDrawable>))
{
  this->contextPresentCallback = callback;
}

static void makeAttribList(std::vector<NSOpenGLPixelFormatAttribute> &attribs,
                           bool stereoVisual,
                           bool needAlpha,
                           bool softwareGL,
                           bool increasedSamplerLimit)
{
  attribs.clear();

  attribs.push_back(NSOpenGLPFAOpenGLProfile);
  attribs.push_back(NSOpenGLProfileVersion3_2Core);

  /* Pixel Format Attributes for the windowed #NSOpenGLContext. */
  attribs.push_back(NSOpenGLPFADoubleBuffer);

  if (softwareGL) {
    attribs.push_back(NSOpenGLPFARendererID);
    attribs.push_back(kCGLRendererGenericFloatID);
  }
  else {
    attribs.push_back(NSOpenGLPFAAccelerated);
    attribs.push_back(NSOpenGLPFANoRecovery);

    /* Attempt to initialize device with extended sampler limit.
     * Resolves EEVEE purple rendering artifacts on macOS. */
    if (increasedSamplerLimit) {
      attribs.push_back((NSOpenGLPixelFormatAttribute)400);
    }
  }

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
  @autoreleasepool {

#ifdef GHOST_OPENGL_ALPHA
    static const bool needAlpha = true;
#else
    static const bool needAlpha = false;
#endif

    /* Command-line argument would be better. */
    if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
      /* Command-line argument would be better. */
      static bool softwareGL = getenv("BLENDER_SOFTWAREGL");

      NSOpenGLPixelFormat *pixelFormat = nil;
      std::vector<NSOpenGLPixelFormatAttribute> attribs;
      bool increasedSamplerLimit = false;

      /* Attempt to initialize device with increased sampler limit.
       * If this is unsupported and initialization fails, initialize GL Context as normal.
       *
       * NOTE: This is not available when using the SoftwareGL path, or for Intel-based
       * platforms. */
      if (!softwareGL) {
        if (@available(macos 11.0, *)) {
          increasedSamplerLimit = true;
        }
      }
      const int max_ctx_attempts = increasedSamplerLimit ? 2 : 1;
      for (int ctx_create_attempt = 0; ctx_create_attempt < max_ctx_attempts; ctx_create_attempt++)
      {

        attribs.clear();
        attribs.reserve(40);
        makeAttribList(attribs, m_stereoVisual, needAlpha, softwareGL, increasedSamplerLimit);

        pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs[0]];
        if (pixelFormat == nil) {
          /* If pixel format creation fails when testing increased sampler limit,
           * attempt initialization again with feature disabled, otherwise, fail. */
          if (increasedSamplerLimit) {
            increasedSamplerLimit = false;
            continue;
          }
          return GHOST_kFailure;
        }

        /* Attempt to create context. */
        m_openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                                     shareContext:s_sharedOpenGLContext];
        [pixelFormat release];

        if (m_openGLContext == nil) {
          /* If context creation fails when testing increased sampler limit,
           * attempt re-creation with feature disabled. Otherwise, error. */
          if (increasedSamplerLimit) {
            increasedSamplerLimit = false;
            continue;
          }

          /* Default context creation attempt failed. */
          return GHOST_kFailure;
        }

        /* Created GL context successfully, activate. */
        [m_openGLContext makeCurrentContext];

        /* When increasing sampler limit, verify created context is a supported configuration. */
        if (increasedSamplerLimit) {
          const char *vendor = (const char *)glGetString(GL_VENDOR);
          const char *renderer = (const char *)glGetString(GL_RENDERER);

          /* If generated context type is unsupported, release existing context and
           * fallback to creating a normal context below. */
          if (strstr(vendor, "Intel") || strstr(renderer, "Software")) {
            [m_openGLContext release];
            m_openGLContext = nil;
            increasedSamplerLimit = false;
            continue;
          }
        }
      }

      if (m_debug) {
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        fprintf(stderr, "OpenGL version %d.%d%s\n", major, minor, softwareGL ? " (software)" : "");
        fprintf(stderr, "Renderer: %s\n", glGetString(GL_RENDERER));
      }

#  ifdef GHOST_WAIT_FOR_VSYNC
      {
        GLint swapInt = 1;
        /* Wait for vertical-sync, to avoid tearing artifacts. */
        [m_openGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
      }
#  endif

      if (m_metalView) {
        if (m_defaultFramebuffer == 0) {
          /* Create a virtual frame-buffer. */
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
#endif
    }
    else {
      /* NOTE(Metal): Metal-only path. */
      if (m_metalView) {
        metalInitFramebuffer();
      }
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::releaseNativeHandles()
{
#ifdef WITH_OPENGL_BACKEND
  m_openGLContext = nil;
  m_openGLView = nil;
#endif
  m_metalView = nil;

  return GHOST_kSuccess;
}

/* OpenGL on Metal
 *
 * Use Metal layer to avoid Viewport lagging on macOS, see #60043. */

static const MTLPixelFormat METAL_FRAMEBUFFERPIXEL_FORMAT = MTLPixelFormatBGRA8Unorm;
static const OSType METAL_CORE_VIDEO_PIXEL_FORMAT = kCVPixelFormatType_32BGRA;

void GHOST_ContextCGL::metalInit()
{
  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    id<MTLDevice> device = m_metalLayer.device;

    /* Create a command queue for blit/present operation. Note: All context should share a single
     * command queue to ensure correct ordering of work submitted from multiple contexts. */
    if (s_sharedMetalCommandQueue == nil) {
      s_sharedMetalCommandQueue = (MTLCommandQueue *)[device
          newCommandQueueWithMaxCommandBufferCount:GHOST_ContextCGL::max_command_buffer_count];
    }
    /* Ensure active GHOSTContext retains a reference to the shared context. */
    [s_sharedMetalCommandQueue retain];

    /* Create shaders for blit operation. */
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

        /* Final blit should ensure alpha is 1.0. This resolves
         * rendering artifacts for blitting of final backbuffer. */
        float4 out_tex = t.sample(s, v.texCoord);
        out_tex.a = 1.0;
        return out_tex;
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

    /* Create a render pipeline for blit operation. */
    MTLRenderPipelineDescriptor *desc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];

    desc.fragmentFunction = [library newFunctionWithName:@"fragment_shader"];
    desc.vertexFunction = [library newFunctionWithName:@"vertex_shader"];
    /* Ensure library is released. */
    [library autorelease];

    [desc.colorAttachments objectAtIndexedSubscript:0].pixelFormat = METAL_FRAMEBUFFERPIXEL_FORMAT;

    m_metalRenderPipeline = (MTLRenderPipelineState *)[device
        newRenderPipelineStateWithDescriptor:desc
                                       error:&error];
    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalInit: newRenderPipelineStateWithDescriptor:error: failed!");
    }

    /* Create a render pipeline to composite things rendered with Metal on top
     * of the frame-buffer contents. Uses the same vertex and fragment shader
     * as the blit above, but with alpha blending enabled. */
    desc.label = @"Metal Overlay";
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalInit: newRenderPipelineStateWithDescriptor:error: failed (when "
          "creating the Metal overlay pipeline)!");
    }
  }
}

void GHOST_ContextCGL::metalFree()
{
  if (m_metalRenderPipeline) {
    [m_metalRenderPipeline release];
  }

  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    if (m_defaultFramebufferMetalTexture[i].texture) {
      [m_defaultFramebufferMetalTexture[i].texture release];
    }
  }
}

void GHOST_ContextCGL::metalInitFramebuffer()
{
  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    glGenFramebuffers(1, &m_defaultFramebuffer);
#endif
  }
  updateDrawingContext();

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFramebuffer);
#endif
  }
}

void GHOST_ContextCGL::metalUpdateFramebuffer()
{
  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    assert(m_defaultFramebuffer != 0);
#endif
  }

  NSRect bounds = [m_metalView bounds];
  NSSize backingSize = [m_metalView convertSizeToBacking:bounds.size];
  size_t width = (size_t)backingSize.width;
  size_t height = (size_t)backingSize.height;

#ifdef WITH_OPENGL_BACKEND
  unsigned int glTex;
  CVPixelBufferRef cvPixelBuffer = nil;
  CVOpenGLTextureCacheRef cvGLTexCache = nil;
  CVOpenGLTextureRef cvGLTex = nil;
  CVMetalTextureCacheRef cvMetalTexCache = nil;
  CVMetalTextureRef cvMetalTex = nil;
#endif

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    /* OPENGL path */
    {
      /* Test if there is anything to update */
      id<MTLTexture> tex = m_defaultFramebufferMetalTexture[current_swapchain_index].texture;
      if (tex && tex.width == width && tex.height == height) {
        return;
      }
    }

    activateDrawingContext();

    NSDictionary *cvPixelBufferProps = @{
      (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
      (__bridge NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
    };
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

    /* Create an OpenGL texture. */
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

    cvret = CVOpenGLTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault, cvGLTexCache, cvPixelBuffer, nil, &cvGLTex);
    if (cvret != kCVReturnSuccess) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalUpdateFramebuffer: "
          "CVOpenGLTextureCacheCreateTextureFromImage failed!");
    }

    glTex = CVOpenGLTextureGetName(cvGLTex);

    /* Create a Metal texture. */
    cvret = CVMetalTextureCacheCreate(
        kCFAllocatorDefault, nil, m_metalLayer.device, nil, &cvMetalTexCache);
    if (cvret != kCVReturnSuccess) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalUpdateFramebuffer: CVMetalTextureCacheCreate failed!");
    }

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

    id<MTLTexture> tex = CVMetalTextureGetTexture(cvMetalTex);

    if (!tex) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalUpdateFramebuffer: CVMetalTextureGetTexture failed!");
    }

    [m_defaultFramebufferMetalTexture[current_swapchain_index].texture release];
    m_defaultFramebufferMetalTexture[current_swapchain_index].texture = [tex retain];
#endif
  }
  else {
    /* NOTE(Metal): Metal API Path. */
    if (m_defaultFramebufferMetalTexture[current_swapchain_index].texture &&
        m_defaultFramebufferMetalTexture[current_swapchain_index].texture.width == width &&
        m_defaultFramebufferMetalTexture[current_swapchain_index].texture.height == height)
    {
      return;
    }

    /* Free old texture */
    [m_defaultFramebufferMetalTexture[current_swapchain_index].texture release];

    id<MTLDevice> device = m_metalLayer.device;
    MTLTextureDescriptor *overlayDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                     width:width
                                    height:height
                                 mipmapped:NO];
    overlayDesc.storageMode = MTLStorageModePrivate;
    overlayDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

    id<MTLTexture> overlayTex = [device newTextureWithDescriptor:overlayDesc];
    if (!overlayTex) {
      ghost_fatal_error_dialog(
          "GHOST_ContextCGL::metalUpdateFramebuffer: failed to create Metal overlay texture!");
    }
    else {
      overlayTex.label = [NSString
          stringWithFormat:@"Metal Overlay for GHOST Context %p", this];  //@"";

      // NSLog(@"Created new Metal Overlay (backbuffer) for context %p\n", this);
    }

    m_defaultFramebufferMetalTexture[current_swapchain_index].texture =
        overlayTex;  //[(MTLTexture *)overlayTex retain];

    /* Clear texture on create */
    id<MTLCommandBuffer> cmdBuffer = [s_sharedMetalCommandQueue commandBuffer];
    MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    {
      auto attachment = [passDescriptor.colorAttachments objectAtIndexedSubscript:0];
      attachment.texture = m_defaultFramebufferMetalTexture[current_swapchain_index].texture;
      attachment.loadAction = MTLLoadActionClear;
      attachment.clearColor = MTLClearColorMake(0.294, 0.294, 0.294, 1.000);
      attachment.storeAction = MTLStoreActionStore;
    }
    {
      id<MTLRenderCommandEncoder> enc = [cmdBuffer
          renderCommandEncoderWithDescriptor:passDescriptor];
      [enc endEncoding];
    }
    [cmdBuffer commit];
  }

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, glTex, 0);
#endif
  }

  [m_metalLayer setDrawableSize:CGSizeMake((CGFloat)width, (CGFloat)height)];
  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    CVPixelBufferRelease(cvPixelBuffer);
    CVOpenGLTextureCacheRelease(cvGLTexCache);
    CVOpenGLTextureRelease(cvGLTex);
    CFRelease(cvMetalTexCache);
    CFRelease(cvMetalTex);
#endif
  }
}

void GHOST_ContextCGL::metalSwapBuffers()
{
  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    updateDrawingContext();

    if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
      glFlush();
      assert(m_defaultFramebufferMetalTexture[current_swapchain_index].texture != nil);
#endif
    }

    id<CAMetalDrawable> drawable = [m_metalLayer nextDrawable];
    if (!drawable) {
      return;
    }

    MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    {
      auto attachment = [passDescriptor.colorAttachments objectAtIndexedSubscript:0];
      attachment.texture = drawable.texture;
      attachment.loadAction = MTLLoadActionClear;
      attachment.clearColor = MTLClearColorMake(1.0, 0.294, 0.294, 1.000);
      attachment.storeAction = MTLStoreActionStore;
    }

    if (!m_useMetalForRendering) {
      id<MTLCommandBuffer> cmdBuffer = [s_sharedMetalCommandQueue commandBuffer];
      {
        assert(m_defaultFramebufferMetalTexture[current_swapchain_index].texture != nil);
        id<MTLRenderCommandEncoder> enc = [cmdBuffer
            renderCommandEncoderWithDescriptor:passDescriptor];
        [enc setRenderPipelineState:(id<MTLRenderPipelineState>)m_metalRenderPipeline];
        [enc setFragmentTexture:m_defaultFramebufferMetalTexture[current_swapchain_index].texture
                        atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
      }

      [cmdBuffer presentDrawable:drawable];

      /* Submit command buffer */
      [cmdBuffer commit];
    }
    else {
      assert(contextPresentCallback);
      assert(m_defaultFramebufferMetalTexture[current_swapchain_index].texture != nil);
      (*contextPresentCallback)(passDescriptor,
                                (id<MTLRenderPipelineState>)m_metalRenderPipeline,
                                m_defaultFramebufferMetalTexture[current_swapchain_index].texture,
                                drawable);
    }
  }
}

void GHOST_ContextCGL::initClear()
{

  if (!m_useMetalForRendering) {
#ifdef WITH_OPENGL_BACKEND
    glClearColor(0.294, 0.294, 0.294, 0.000);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.000, 0.000, 0.000, 0.000);
#endif
  }
}
