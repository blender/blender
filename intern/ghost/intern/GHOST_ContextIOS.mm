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

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextIOS class.
 */

#include "GHOST_ContextIOS.hh"
#include "GHOST_Debug.hh"

#include <DNA_userdef_types.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#import <MetalKit/MTKDefines.h>
#import <MetalKit/MTKView.h>
#import <UIKit/UIKit.h>

#include <cassert>
#include <vector>

bool GHOST_ContextIOS::current_drawable_presented = false;
id<CAMetalDrawable> GHOST_ContextIOS::prevDrawable = nil;

static void ghost_fatal_error_dialog(const char * /*msg*/)
{
  exit(1);
}

MTLCommandQueue *GHOST_ContextIOS::s_sharedMetalCommandQueue = nil;
int GHOST_ContextIOS::s_sharedCount = 0;

GHOST_ContextIOS::GHOST_ContextIOS(UIView *uiView, MTKView *metalView)
    : GHOST_Context(false), m_uiView(uiView), m_metalView(metalView), m_metalRenderPipeline(nil)
{
  /* Init swapchain */
  current_swapchain_index = 0;
  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    m_defaultFramebufferMetalTexture[i].texture = nil;
    m_defaultFramebufferMetalTexture[i].index = i;
  }

  /* Verify and initialise View */
  if (m_metalView) {
    ownsMetalDevice = false;
    metalInit();
  }
  else {
    /* Initialize Metal device (Using system default) */
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();

    CGRect screenRect = [[UIScreen mainScreen] bounds];
    CGFloat screenWidth = screenRect.size.width;
    CGFloat screenHeight = screenRect.size.height;
    if (screenWidth <= 0 || screenHeight <= 0) {
      /* TODO: Avoid using default resolution, this path should however not be hit. */
      screenWidth = 2532;
      screenHeight = 1170;
    }

    assert(screenWidth > 0 && screenHeight > 0);

    if (screenWidth <= 0) {
      /* TODO: Avoid using default resolution, this path should however not be hit. */
      screenWidth = 2532;
      screenHeight = 1170;
    }

    /* Create own device */
    m_metalView = [[MTKView alloc] initWithFrame:CGRectMake(0, 0, screenWidth, screenHeight)];
    assert(m_metalView);
    m_metalView.device = metalDevice;
    m_uiView = (UIView *)m_metalView;

    ownsMetalDevice = true;

    if (metalDevice) {
      metalInit();
    }
    else {
      GHOST_PRINT("[ERROR] Failed to create Metal device for offscreen GHOST Context\n");
    }
  }

  /* Initialise swapinterval */
  mtl_SwapInterval = 60;

  /* IOS_FIXME: Temp fix for swapbuffers issue causing sporadic lockups.
   * Repros on loading assets screen */
  m_allow_presents = false;
  defer_swap_buffers = true;
  swap_buffers_requested_count = 0;
}

GHOST_ContextIOS::~GHOST_ContextIOS()
{
  metalFree();

  if (ownsMetalDevice) {
    if (m_metalView) {
      [m_metalView release];
      m_metalView = nil;
    }
  }
}

GHOST_TSuccess GHOST_ContextIOS::swapBuffers()
{
  if (!defer_swap_buffers) {
    metalSwapBuffers();
  }
  else {
    swap_buffers_requested_count++;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextIOS::setSwapInterval(int interval)
{
  mtl_SwapInterval = interval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextIOS::getSwapInterval(int &intervalOut)
{

  intervalOut = mtl_SwapInterval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextIOS::activateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextIOS::releaseDrawingContext()
{
  return GHOST_kSuccess;
}

unsigned int GHOST_ContextIOS::getDefaultFramebuffer()
{
  return 0;
}

GHOST_TSuccess GHOST_ContextIOS::updateDrawingContext()
{

  if (m_metalView) {
    metalUpdateFramebuffer();
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

id<MTLTexture> GHOST_ContextIOS::metalOverlayTexture()
{
  /* Increment Swapchain - Only needed if context is requesting a new texture */
  current_swapchain_index = (current_swapchain_index + 1) % METAL_SWAPCHAIN_SIZE;

  /* Ensure backing texture is ready for current swapchain index */
  updateDrawingContext();

  /* Return texture */
  return m_defaultFramebufferMetalTexture[current_swapchain_index].texture;
}

MTLCommandQueue *GHOST_ContextIOS::metalCommandQueue()
{
  return s_sharedMetalCommandQueue;
}

id<MTLDevice> extern_device = nil;
MTLDevice *GHOST_ContextIOS::metalDevice()
{
  id<MTLDevice> device = m_metalView.device;
  extern_device = m_metalView.device;
  return (MTLDevice *)device;
}

GHOST_TSuccess GHOST_ContextIOS::initializeDrawingContext()
{
  @autoreleasepool {
    if (m_metalView) {
      metalInitFramebuffer();
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextIOS::releaseNativeHandles()
{
  m_metalView = nil;

  return GHOST_kSuccess;
}

static const MTLPixelFormat METAL_FRAMEBUFFERPIXEL_FORMAT = MTLPixelFormatBGRA8Unorm;

void GHOST_ContextIOS::metalInit()
{
  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    id<MTLDevice> device = m_metalView.device;

    /* Create a command queue for blit/present operation.
     * NOTE: All context should share a single command queue
     * to ensure correct ordering of work submitted from multiple contexts. */
    if (s_sharedMetalCommandQueue == nil) {
      s_sharedMetalCommandQueue = (MTLCommandQueue *)[device
          newCommandQueueWithMaxCommandBufferCount:GHOST_ContextIOS::max_command_buffer_count];
    }
    /* Ensure active GHOSTContext retains a reference to the shared context. */
    [s_sharedMetalCommandQueue retain];

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
          "GHOST_ContextIOS::metalInit: newLibraryWithSource:options:error: failed!");
    }

    // Create a render pipeline for blit operation
    MTLRenderPipelineDescriptor *desc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];

    desc.label = @"Blit";
    desc.fragmentFunction = [library newFunctionWithName:@"fragment_shader"];
    desc.vertexFunction = [library newFunctionWithName:@"vertex_shader"];
    [library autorelease];

    [desc.colorAttachments objectAtIndexedSubscript:0].pixelFormat = METAL_FRAMEBUFFERPIXEL_FORMAT;

    m_metalRenderPipeline = (MTLRenderPipelineState *)[device
        newRenderPipelineStateWithDescriptor:desc
                                       error:&error];
    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextIOS::metalInit: newRenderPipelineStateWithDescriptor:error: failed!");
    }

    // Create a render pipeline to composite things rendered with Metal on top
    // of the framebuffer contents. Uses the same vertex and fragment shader
    // as the blit above, but with alpha blending enabled.
    desc.label = @"Metal Overlay";
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    if (error) {
      ghost_fatal_error_dialog(
          "GHOST_ContextIOS::metalInit: newRenderPipelineStateWithDescriptor:error: failed (when "
          "creating the Metal overlay pipeline)!");
    }
  }
}

void GHOST_ContextIOS::metalFree()
{
  if (m_metalRenderPipeline) {
    [m_metalRenderPipeline release];
    m_metalRenderPipeline = nil;
  }

  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    if (m_defaultFramebufferMetalTexture[i].texture) {
      [m_defaultFramebufferMetalTexture[i].texture release];
      m_defaultFramebufferMetalTexture[i].texture = nil;
    }
  }
}

void GHOST_ContextIOS::metalInitFramebuffer()
{
  updateDrawingContext();
}

void GHOST_ContextIOS::metalUpdateFramebuffer()
{
  CGRect screenRect = [[UIScreen mainScreen] bounds];
  CGFloat scaling_fac = [UIScreen mainScreen].scale;
  CGFloat screenWidth = screenRect.size.width;
  CGFloat screenHeight = screenRect.size.height;
  size_t width = screenWidth * scaling_fac;
  size_t height = screenHeight * scaling_fac;

  if (width <= 0 && height <= 0) {
    assert(false);
    /* TOOD: Better default size. This should not happen but is here to avoid erroneous
     * initialization. */
    width = 1440;
    height = 960;
  }

  /* METAL-only path -- Test metal overlay. */
  if (m_defaultFramebufferMetalTexture[current_swapchain_index].texture &&
      m_defaultFramebufferMetalTexture[current_swapchain_index].texture.width == width &&
      m_defaultFramebufferMetalTexture[current_swapchain_index].texture.height == height)
  {
    return;
  }

  /* Free old texture */
  [m_defaultFramebufferMetalTexture[current_swapchain_index].texture release];

  id<MTLDevice> device = m_metalView.device;
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
        "GHOST_ContextIOS::metalUpdateFramebuffer: failed to create Metal overlay texture!");
  }
  else {
    overlayTex.label = [NSString stringWithFormat:@"Metal Overlay for GHOST Context %p", this];
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
}

void GHOST_ContextIOS::metalRegisterPresentCallback(void (*callback)(
    MTLRenderPassDescriptor *, id<MTLRenderPipelineState>, id<MTLTexture>, id<CAMetalDrawable>))
{
  this->contextPresentCallback = callback;
}

void GHOST_ContextIOS::allowPresents(bool allow_presents)
{
  m_allow_presents = allow_presents;
}

void GHOST_ContextIOS::metalSwapBuffers()
{
  /* Check a request was made. */
  if (defer_swap_buffers && !swap_buffers_requested_count) {
    return;
  }
  /* If we hit this it implies that we have a request to swap/present
   * from an inactive window and we need to debug why. */
  if (!m_allow_presents) {
    NSLog(@"Present for inactive window observed");
  }

  /* Get the next drawable. */
  id<CAMetalDrawable> current_drawable = m_metalView.currentDrawable;

  if (!current_drawable) {
    NSLog(@"Failed to acquire CAMetalDrawable");
    return;
  }

  /* Double presents indicate that we are trying to present updates faster
   * than the display's refresh rate. We should always display the latest update
   * (or the screen will lag Blender's view of the world) but output a message
   * so we are aware and can investigate. */
  if (current_drawable != GHOST_ContextIOS::prevDrawable) {
    GHOST_ContextIOS::current_drawable_presented = false;
    GHOST_ContextIOS::prevDrawable = current_drawable;
  }
  if (current_drawable_presented) {
    NSLog(@"Double present (MTKView)%p!", m_metalView);
  }

  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    updateDrawingContext();

    /* Get a renderpass descriptor for the current view. */
    MTLRenderPassDescriptor *passDescriptor = m_metalView.currentRenderPassDescriptor;
    if (passDescriptor == nil) {
      NSLog(@"Failed to acquire MTLRenderPassDescriptor");
      return;
    }
    else {
      auto attachment = [passDescriptor.colorAttachments objectAtIndexedSubscript:0];
      attachment.loadAction = MTLLoadActionClear;
      attachment.clearColor = MTLClearColorMake(0.294, 0.294, 0.294, 1.000);
      attachment.storeAction = MTLStoreActionStore;
    }

    assert(contextPresentCallback);
    assert(m_defaultFramebufferMetalTexture[current_swapchain_index].texture != nil);
    (*contextPresentCallback)(passDescriptor,
                              (id<MTLRenderPipelineState>)m_metalRenderPipeline,
                              m_defaultFramebufferMetalTexture[current_swapchain_index].texture,
                              m_allow_presents ? current_drawable : nullptr);
    GHOST_ContextIOS::current_drawable_presented = true;
  }

  if (defer_swap_buffers) {
    swap_buffers_requested_count = 0;
  }
}
