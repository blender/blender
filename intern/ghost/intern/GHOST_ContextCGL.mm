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

static const MTLPixelFormat METAL_FRAMEBUFFERPIXEL_FORMAT_EDR = MTLPixelFormatRGBA16Float;

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
MTLCommandQueue *GHOST_ContextCGL::s_sharedMetalCommandQueue = nil;
int GHOST_ContextCGL::s_sharedCount = 0;

GHOST_ContextCGL::GHOST_ContextCGL(bool stereoVisual,
                                   NSView *metalView,
                                   CAMetalLayer *metalLayer,
                                   int debug)
    : GHOST_Context(stereoVisual),
      m_metalView(metalView),
      m_metalLayer(metalLayer),
      m_metalRenderPipeline(nil),
      m_debug(debug)
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

      /* Enable EDR support. This is done by:
       * 1. Using a floating point render target, so that values outside 0..1 can be used
       * 2. Informing the OS that we are EDR aware, and intend to use values outside 0..1
       * 3. Setting the extended sRGB color space so that the OS knows how to interpret the
       *    values.
       */
      m_metalLayer.wantsExtendedDynamicRangeContent = YES;
      m_metalLayer.pixelFormat = METAL_FRAMEBUFFERPIXEL_FORMAT_EDR;
      const CFStringRef name = kCGColorSpaceExtendedSRGB;
      CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(name);
      m_metalLayer.colorspace = colorspace;
      CGColorSpaceRelease(colorspace);

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

  if (m_ownsMetalDevice) {
    if (m_metalLayer) {
      [m_metalLayer release];
      m_metalLayer = nil;
    }
  }
}

GHOST_TSuccess GHOST_ContextCGL::swapBuffers()
{
  if (m_metalView) {
    metalSwapBuffers();
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::setSwapInterval(int interval)
{
  mtl_SwapInterval = interval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::getSwapInterval(int &intervalOut)
{
  intervalOut = mtl_SwapInterval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::activateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::releaseDrawingContext()
{
  return GHOST_kSuccess;
}

unsigned int GHOST_ContextCGL::getDefaultFramebuffer()
{
  /* NOTE(Metal): This is not valid. */
  return 0;
}

GHOST_TSuccess GHOST_ContextCGL::updateDrawingContext()
{
  if (m_metalView) {
    metalUpdateFramebuffer();
    return GHOST_kSuccess;
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

GHOST_TSuccess GHOST_ContextCGL::initializeDrawingContext()
{
  @autoreleasepool {
    if (m_metalView) {
      metalInitFramebuffer();
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::releaseNativeHandles()
{
  m_metalView = nil;

  return GHOST_kSuccess;
}

void GHOST_ContextCGL::metalInit()
{
  @autoreleasepool {
    id<MTLDevice> device = m_metalLayer.device;

    /* Create a command queue for blit/present operation.
     * NOTE: All context should share a single command queue
     * to ensure correct ordering of work submitted from multiple contexts. */
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
         * rendering artifacts for blitting of final back-buffer. */
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
    [desc.colorAttachments objectAtIndexedSubscript:0].pixelFormat =
        METAL_FRAMEBUFFERPIXEL_FORMAT_EDR;

    /* Ensure library is released. */
    [library autorelease];

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
  updateDrawingContext();
}

void GHOST_ContextCGL::metalUpdateFramebuffer()
{
  NSRect bounds = [m_metalView bounds];
  NSSize backingSize = [m_metalView convertSizeToBacking:bounds.size];
  size_t width = (size_t)backingSize.width;
  size_t height = (size_t)backingSize.height;

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
      texture2DDescriptorWithPixelFormat:METAL_FRAMEBUFFERPIXEL_FORMAT_EDR
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
  }

  m_defaultFramebufferMetalTexture[current_swapchain_index].texture = overlayTex;

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

  [m_metalLayer setDrawableSize:CGSizeMake((CGFloat)width, (CGFloat)height)];
}

void GHOST_ContextCGL::metalSwapBuffers()
{
  @autoreleasepool {
    updateDrawingContext();

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

    assert(contextPresentCallback);
    assert(m_defaultFramebufferMetalTexture[current_swapchain_index].texture != nil);
    (*contextPresentCallback)(passDescriptor,
                              (id<MTLRenderPipelineState>)m_metalRenderPipeline,
                              m_defaultFramebufferMetalTexture[current_swapchain_index].texture,
                              drawable);
  }
}
