/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextMTL class.
 */

/* Don't generate OpenGL deprecation warning. This is a known thing, and is not something easily
 * solvable in a short term. */
#ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "GHOST_ContextMTL.hh"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <cassert>
#include <mutex>
#include <vector>

static const MTLPixelFormat METAL_FRAMEBUFFERPIXEL_FORMAT_EDR = MTLPixelFormatRGBA16Float;

static void ghost_fatal_error_dialog(const char *msg)
{
  @autoreleasepool {
    NSString *message = [NSString stringWithFormat:@"Error opening window:\n%s", msg];

    NSAlert *alert = [[NSAlert alloc] init];

    alert.messageText = @"Blender";
    alert.informativeText = message;
    alert.alertStyle = NSAlertStyleCritical;

    [alert addButtonWithTitle:@"Quit"];
    [alert runModal];
  }

  exit(1);
}

MTLCommandQueue *GHOST_ContextMTL::s_sharedMetalCommandQueue = nil;
int GHOST_ContextMTL::s_sharedCount = 0;

GHOST_ContextMTL::GHOST_ContextMTL(const GHOST_ContextParams &context_params,
                                   NSView *metalView,
                                   CAMetalLayer *metalLayer)
    : GHOST_Context(context_params),
      metal_view_(metalView),
      metal_layer_(metalLayer),
      metal_render_pipeline_(nil)
{
  @autoreleasepool {
    /* Initialize Metal Swap-chain. */
    current_swapchain_index = 0;
    for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
      default_framebuffer_metal_texture_[i].texture = nil;
      default_framebuffer_metal_texture_[i].index = i;
    }

    if (metal_view_) {
      owns_metal_device_ = false;
      metalInit();
    }
    else {
      /* Prepare offscreen GHOST Context Metal device. */
      id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();

      if (context_params_.is_debug) {
        printf("Selected Metal Device: %s\n", [metalDevice.name UTF8String]);
      }

      owns_metal_device_ = true;
      if (metalDevice) {
        metal_layer_ = [[CAMetalLayer alloc] init];
        metal_layer_.edgeAntialiasingMask = 0;
        metal_layer_.masksToBounds = NO;
        metal_layer_.opaque = YES;
        metal_layer_.framebufferOnly = YES;
        metal_layer_.presentsWithTransaction = NO;
        [metal_layer_ removeAllAnimations];
        metal_layer_.device = metalDevice;
        metal_layer_.allowsNextDrawableTimeout = NO;

        {
          const GHOST_TVSyncModes vsync = getVSync();
          if (vsync != GHOST_kVSyncModeUnset) {
            metal_layer_.displaySyncEnabled = (vsync == GHOST_kVSyncModeOff) ? NO : YES;
          }
        }

        /* Enable EDR support. This is done by:
         * 1. Using a floating point render target, so that values outside 0..1 can be used
         * 2. Informing the OS that we are EDR aware, and intend to use values outside 0..1
         * 3. Setting the extended sRGB color space so that the OS knows how to interpret the
         *    values.
         */
        metal_layer_.wantsExtendedDynamicRangeContent = YES;
        metal_layer_.pixelFormat = METAL_FRAMEBUFFERPIXEL_FORMAT_EDR;
        const CFStringRef name = kCGColorSpaceExtendedSRGB;
        CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(name);
        metal_layer_.colorspace = colorspace;
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
}

GHOST_ContextMTL::~GHOST_ContextMTL()
{
  /* Multiple threads can release their own context at the same time. */
  static std::mutex mutex;
  std::scoped_lock lock(mutex);

  metalFree();

  if (owns_metal_device_) {
    if (metal_layer_) {
      [metal_layer_ release];
      metal_layer_ = nil;
    }
  }
  assert(s_sharedCount);

  s_sharedCount--;
  [s_sharedMetalCommandQueue release];
  if (s_sharedCount == 0) {
    s_sharedMetalCommandQueue = nil;
  }
}

GHOST_TSuccess GHOST_ContextMTL::swapBufferRelease()
{
  if (metal_view_) {
    metalSwapBuffers();
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextMTL::setSwapInterval(int interval)
{
  mtl_SwapInterval = interval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextMTL::getSwapInterval(int &interval_out)
{
  interval_out = mtl_SwapInterval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextMTL::activateDrawingContext()
{
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextMTL::releaseDrawingContext()
{
  active_context_ = nullptr;
  return GHOST_kSuccess;
}

unsigned int GHOST_ContextMTL::getDefaultFramebuffer()
{
  /* NOTE(Metal): This is not valid. */
  return 0;
}

GHOST_TSuccess GHOST_ContextMTL::updateDrawingContext()
{
  if (metal_view_) {
    metalUpdateFramebuffer();
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

id<MTLTexture> GHOST_ContextMTL::metalOverlayTexture()
{
  /* Increment Swap-chain - Only needed if context is requesting a new texture */
  current_swapchain_index = (current_swapchain_index + 1) % METAL_SWAPCHAIN_SIZE;

  /* Ensure backing texture is ready for current swapchain index */
  updateDrawingContext();

  /* Return texture. */
  return default_framebuffer_metal_texture_[current_swapchain_index].texture;
}

MTLCommandQueue *GHOST_ContextMTL::metalCommandQueue()
{
  return s_sharedMetalCommandQueue;
}
MTLDevice *GHOST_ContextMTL::metalDevice()
{
  id<MTLDevice> device = metal_layer_.device;
  return (MTLDevice *)device;
}

void GHOST_ContextMTL::metalRegisterPresentCallback(void (*callback)(
    MTLRenderPassDescriptor *, id<MTLRenderPipelineState>, id<MTLTexture>, id<CAMetalDrawable>))
{
  this->contextPresentCallback = callback;
}

void GHOST_ContextMTL::metalRegisterXrBlitCallback(
    void (*callback)(id<MTLTexture>, int, int, int, int))
{
  this->xrBlitCallback = callback;
}

GHOST_TSuccess GHOST_ContextMTL::initializeDrawingContext()
{
  @autoreleasepool {
    if (metal_view_) {
      metalInitFramebuffer();
    }
  }
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextMTL::releaseNativeHandles()
{
  metal_view_ = nil;

  return GHOST_kSuccess;
}

void GHOST_ContextMTL::metalInit()
{
  @autoreleasepool {
    id<MTLDevice> device = metal_layer_.device;

    /* Create a command queue for blit/present operation.
     * NOTE: All context should share a single command queue
     * to ensure correct ordering of work submitted from multiple contexts. */
    if (s_sharedMetalCommandQueue == nil) {
      s_sharedMetalCommandQueue = (MTLCommandQueue *)[device
          newCommandQueueWithMaxCommandBufferCount:GHOST_ContextMTL::max_command_buffer_count];
    }
    /* Ensure active GHOSTContext retains a reference to the shared context. */
    [s_sharedMetalCommandQueue retain];
    s_sharedCount++;

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
          "GHOST_ContextMTL::metalInit: newLibraryWithSource:options:error: failed!");
    }

    /* Create a render pipeline for blit operation. */
    MTLRenderPipelineDescriptor *desc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];

    desc.fragmentFunction = [library newFunctionWithName:@"fragment_shader"];
    desc.vertexFunction = [library newFunctionWithName:@"vertex_shader"];
    [desc.colorAttachments objectAtIndexedSubscript:0].pixelFormat =
        METAL_FRAMEBUFFERPIXEL_FORMAT_EDR;

    /* Ensure library is released. */
    [library autorelease];

    metal_render_pipeline_ = (MTLRenderPipelineState *)[device
        newRenderPipelineStateWithDescriptor:desc
                                       error:&error];
    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextMTL::metalInit: newRenderPipelineStateWithDescriptor:error: failed!");
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
          "GHOST_ContextMTL::metalInit: newRenderPipelineStateWithDescriptor:error: failed (when "
          "creating the Metal overlay pipeline)!");
    }

    [desc.fragmentFunction release];
    [desc.vertexFunction release];
  }
}

void GHOST_ContextMTL::metalFree()
{
  if (metal_render_pipeline_) {
    [metal_render_pipeline_ release];
    metal_render_pipeline_ = nil;
  }

  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    if (default_framebuffer_metal_texture_[i].texture) {
      [default_framebuffer_metal_texture_[i].texture release];
      default_framebuffer_metal_texture_[i].texture = nil;
    }
  }
}

void GHOST_ContextMTL::metalInitFramebuffer()
{
  updateDrawingContext();
}

void GHOST_ContextMTL::metalUpdateFramebuffer()
{
  @autoreleasepool {
    const NSRect bounds = [metal_view_ bounds];
    const NSSize backingSize = [metal_view_ convertSizeToBacking:bounds.size];
    const size_t width = size_t(backingSize.width);
    const size_t height = size_t(backingSize.height);

    if (default_framebuffer_metal_texture_[current_swapchain_index].texture &&
        default_framebuffer_metal_texture_[current_swapchain_index].texture.width == width &&
        default_framebuffer_metal_texture_[current_swapchain_index].texture.height == height)
    {
      return;
    }

    /* Free old texture */
    [default_framebuffer_metal_texture_[current_swapchain_index].texture release];

    id<MTLDevice> device = metal_layer_.device;
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
          "GHOST_ContextMTL::metalUpdateFramebuffer: failed to create Metal overlay texture!");
    }
    else {
      overlayTex.label = [NSString
          stringWithFormat:@"Metal Overlay for GHOST Context %p", this];  //@"";
    }

    default_framebuffer_metal_texture_[current_swapchain_index].texture = overlayTex;

    /* Clear texture on create */
    id<MTLCommandBuffer> cmdBuffer = [s_sharedMetalCommandQueue commandBuffer];
    MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    {
      auto attachment = [passDescriptor.colorAttachments objectAtIndexedSubscript:0];
      attachment.texture = default_framebuffer_metal_texture_[current_swapchain_index].texture;
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

    metal_layer_.drawableSize = CGSizeMake(CGFloat(width), CGFloat(height));
  }
}

void GHOST_ContextMTL::metalSwapBuffers()
{
  @autoreleasepool {
    updateDrawingContext();

    id<CAMetalDrawable> drawable = [metal_layer_ nextDrawable];
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
    assert(default_framebuffer_metal_texture_[current_swapchain_index].texture != nil);
    (*contextPresentCallback)(passDescriptor,
                              (id<MTLRenderPipelineState>)metal_render_pipeline_,
                              default_framebuffer_metal_texture_[current_swapchain_index].texture,
                              drawable);
  }
}
