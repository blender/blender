/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__
typedef struct VolumeState {
#  ifdef __SPLIT_KERNEL__
#  else
  PathState ps;
#  endif
} VolumeState;

/* Get PathState ready for use for volume stack evaluation. */
#  ifdef __SPLIT_KERNEL__
ccl_addr_space
#  endif
    ccl_device_inline PathState *
    shadow_blocked_volume_path_state(KernelGlobals *kg,
                                     VolumeState *volume_state,
                                     ccl_addr_space PathState *state,
                                     ShaderData *sd,
                                     Ray *ray)
{
#  ifdef __SPLIT_KERNEL__
  ccl_addr_space PathState *ps =
      &kernel_split_state.state_shadow[ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0)];
#  else
  PathState *ps = &volume_state->ps;
#  endif
  *ps = *state;
  /* We are checking for shadow on the "other" side of the surface, so need
   * to discard volume we are currently at.
   */
  if (dot(sd->Ng, ray->D) < 0.0f) {
    kernel_volume_stack_enter_exit(kg, sd, ps->volume_stack);
  }
  return ps;
}
#endif /* __VOLUME__ */

/* Attenuate throughput accordingly to the given intersection event.
 * Returns true if the throughput is zero and traversal can be aborted.
 */
ccl_device_forceinline bool shadow_handle_transparent_isect(
    KernelGlobals *kg,
    ShaderData *shadow_sd,
    ccl_addr_space PathState *state,
#ifdef __VOLUME__
    ccl_addr_space struct PathState *volume_state,
#endif
    Intersection *isect,
    Ray *ray,
    float3 *throughput)
{
#ifdef __VOLUME__
  /* Attenuation between last surface and next surface. */
  if (volume_state->volume_stack[0].shader != SHADER_NONE) {
    Ray segment_ray = *ray;
    segment_ray.t = isect->t;
    kernel_volume_shadow(kg, shadow_sd, volume_state, &segment_ray, throughput);
  }
#endif
  /* Setup shader data at surface. */
  shader_setup_from_ray(kg, shadow_sd, isect, ray);
  /* Attenuation from transparent surface. */
  if (!(shadow_sd->flag & SD_HAS_ONLY_VOLUME)) {
    path_state_modify_bounce(state, true);
    shader_eval_surface(kg, shadow_sd, state, PATH_RAY_SHADOW);
    path_state_modify_bounce(state, false);
    *throughput *= shader_bsdf_transparency(kg, shadow_sd);
  }
  /* Stop if all light is blocked. */
  if (is_zero(*throughput)) {
    return true;
  }
#ifdef __VOLUME__
  /* Exit/enter volume. */
  kernel_volume_stack_enter_exit(kg, shadow_sd, volume_state->volume_stack);
#endif
  return false;
}

/* Special version which only handles opaque shadows. */
ccl_device bool shadow_blocked_opaque(KernelGlobals *kg,
                                      ShaderData *shadow_sd,
                                      ccl_addr_space PathState *state,
                                      const uint visibility,
                                      Ray *ray,
                                      Intersection *isect,
                                      float3 *shadow)
{
  const bool blocked = scene_intersect(kg, *ray, visibility & PATH_RAY_SHADOW_OPAQUE, isect);
#ifdef __VOLUME__
  if (!blocked && state->volume_stack[0].shader != SHADER_NONE) {
    /* Apply attenuation from current volume shader. */
    kernel_volume_shadow(kg, shadow_sd, state, ray, shadow);
  }
#endif
  return blocked;
}

#ifdef __TRANSPARENT_SHADOWS__
#  ifdef __SHADOW_RECORD_ALL__
/* Shadow function to compute how much light is blocked,
 *
 * We trace a single ray. If it hits any opaque surface, or more than a given
 * number of transparent surfaces is hit, then we consider the geometry to be
 * entirely blocked. If not, all transparent surfaces will be recorded and we
 * will shade them one by one to determine how much light is blocked. This all
 * happens in one scene intersection function.
 *
 * Recording all hits works well in some cases but may be slower in others. If
 * we have many semi-transparent hairs, one intersection may be faster because
 * you'd be reinteresecting the same hairs a lot with each step otherwise. If
 * however there is mostly binary transparency then we may be recording many
 * unnecessary intersections when one of the first surfaces blocks all light.
 *
 * From tests in real scenes it seems the performance loss is either minimal,
 * or there is a performance increase anyway due to avoiding the need to send
 * two rays with transparent shadows.
 *
 * On CPU it'll handle all transparent bounces (by allocating storage for
 * intersections when they don't fit into the stack storage).
 *
 * On GPU it'll only handle SHADOW_STACK_MAX_HITS-1 intersections, so this
 * is something to be kept an eye on.
 */

#    define SHADOW_STACK_MAX_HITS 64

/* Actual logic with traversal loop implementation which is free from device
 * specific tweaks.
 *
 * Note that hits array should be as big as max_hits+1.
 */
ccl_device bool shadow_blocked_transparent_all_loop(KernelGlobals *kg,
                                                    ShaderData *sd,
                                                    ShaderData *shadow_sd,
                                                    ccl_addr_space PathState *state,
                                                    const uint visibility,
                                                    Ray *ray,
                                                    Intersection *hits,
                                                    uint max_hits,
                                                    float3 *shadow)
{
  /* Intersect to find an opaque surface, or record all transparent
   * surface hits.
   */
  uint num_hits;
  const bool blocked = scene_intersect_shadow_all(kg, ray, hits, visibility, max_hits, &num_hits);
#    ifdef __VOLUME__
  VolumeState volume_state;
#    endif
  /* If no opaque surface found but we did find transparent hits,
   * shade them.
   */
  if (!blocked && num_hits > 0) {
    float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
    float3 Pend = ray->P + ray->D * ray->t;
    float last_t = 0.0f;
    int bounce = state->transparent_bounce;
    Intersection *isect = hits;
#    ifdef __VOLUME__
#      ifdef __SPLIT_KERNEL__
    ccl_addr_space
#      endif
        PathState *ps = shadow_blocked_volume_path_state(kg, &volume_state, state, sd, ray);
#    endif
    sort_intersections(hits, num_hits);
    for (int hit = 0; hit < num_hits; hit++, isect++) {
      /* Adjust intersection distance for moving ray forward. */
      float new_t = isect->t;
      isect->t -= last_t;
      /* Skip hit if we did not move forward, step by step raytracing
       * would have skipped it as well then.
       */
      if (last_t == new_t) {
        continue;
      }
      last_t = new_t;
      /* Attenuate the throughput. */
      if (shadow_handle_transparent_isect(kg,
                                          shadow_sd,
                                          state,
#    ifdef __VOLUME__
                                          ps,
#    endif
                                          isect,
                                          ray,
                                          &throughput)) {
        return true;
      }
      /* Move ray forward. */
      ray->P = shadow_sd->P;
      if (ray->t != FLT_MAX) {
        ray->D = normalize_len(Pend - ray->P, &ray->t);
      }
      bounce++;
    }
#    ifdef __VOLUME__
    /* Attenuation for last line segment towards light. */
    if (ps->volume_stack[0].shader != SHADER_NONE) {
      kernel_volume_shadow(kg, shadow_sd, ps, ray, &throughput);
    }
#    endif
    *shadow = throughput;
    return is_zero(throughput);
  }
#    ifdef __VOLUME__
  if (!blocked && state->volume_stack[0].shader != SHADER_NONE) {
    /* Apply attenuation from current volume shader. */
#      ifdef __SPLIT_KERNEL__
    ccl_addr_space
#      endif
        PathState *ps = shadow_blocked_volume_path_state(kg, &volume_state, state, sd, ray);
    kernel_volume_shadow(kg, shadow_sd, ps, ray, shadow);
  }
#    endif
  return blocked;
}

/* Here we do all device specific trickery before invoking actual traversal
 * loop to help readability of the actual logic.
 */
ccl_device bool shadow_blocked_transparent_all(KernelGlobals *kg,
                                               ShaderData *sd,
                                               ShaderData *shadow_sd,
                                               ccl_addr_space PathState *state,
                                               const uint visibility,
                                               Ray *ray,
                                               uint max_hits,
                                               float3 *shadow)
{
#    ifdef __SPLIT_KERNEL__
  Intersection hits_[SHADOW_STACK_MAX_HITS];
  Intersection *hits = &hits_[0];
#    elif defined(__KERNEL_CUDA__)
  Intersection *hits = kg->hits_stack;
#    else
  Intersection hits_stack[SHADOW_STACK_MAX_HITS];
  Intersection *hits = hits_stack;
#    endif
#    ifndef __KERNEL_GPU__
  /* Prefer to use stack but use dynamic allocation if too deep max hits
   * we need max_hits + 1 storage space due to the logic in
   * scene_intersect_shadow_all which will first store and then check if
   * the limit is exceeded.
   *
   * Ignore this on GPU because of slow/unavailable malloc().
   */
  if (max_hits + 1 > SHADOW_STACK_MAX_HITS) {
    if (kg->transparent_shadow_intersections == NULL) {
      const int transparent_max_bounce = kernel_data.integrator.transparent_max_bounce;
      kg->transparent_shadow_intersections = (Intersection *)malloc(sizeof(Intersection) *
                                                                    (transparent_max_bounce + 1));
    }
    hits = kg->transparent_shadow_intersections;
  }
#    endif /* __KERNEL_GPU__ */
  /* Invoke actual traversal. */
  return shadow_blocked_transparent_all_loop(
      kg, sd, shadow_sd, state, visibility, ray, hits, max_hits, shadow);
}
#  endif /* __SHADOW_RECORD_ALL__ */

#  if defined(__KERNEL_GPU__) || !defined(__SHADOW_RECORD_ALL__)
/* Shadow function to compute how much light is blocked,
 *
 * Here we raytrace from one transparent surface to the next step by step.
 * To minimize overhead in cases where we don't need transparent shadows, we
 * first trace a regular shadow ray. We check if the hit primitive was
 * potentially transparent, and only in that case start marching. this gives
 * one extra ray cast for the cases were we do want transparency.
 */

/* This function is only implementing device-independent traversal logic
 * which requires some precalculation done.
 */
ccl_device bool shadow_blocked_transparent_stepped_loop(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        ShaderData *shadow_sd,
                                                        ccl_addr_space PathState *state,
                                                        const uint visibility,
                                                        Ray *ray,
                                                        Intersection *isect,
                                                        const bool blocked,
                                                        const bool is_transparent_isect,
                                                        float3 *shadow)
{
#    ifdef __VOLUME__
  VolumeState volume_state;
#    endif
  if (blocked && is_transparent_isect) {
    float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
    float3 Pend = ray->P + ray->D * ray->t;
    int bounce = state->transparent_bounce;
#    ifdef __VOLUME__
#      ifdef __SPLIT_KERNEL__
    ccl_addr_space
#      endif
        PathState *ps = shadow_blocked_volume_path_state(kg, &volume_state, state, sd, ray);
#    endif
    for (;;) {
      if (bounce >= kernel_data.integrator.transparent_max_bounce) {
        return true;
      }
      if (!scene_intersect(kg, *ray, visibility & PATH_RAY_SHADOW_TRANSPARENT, isect)) {
        break;
      }
      if (!shader_transparent_shadow(kg, isect)) {
        return true;
      }
      /* Attenuate the throughput. */
      if (shadow_handle_transparent_isect(kg,
                                          shadow_sd,
                                          state,
#    ifdef __VOLUME__
                                          ps,
#    endif
                                          isect,
                                          ray,
                                          &throughput)) {
        return true;
      }
      /* Move ray forward. */
      ray->P = ray_offset(shadow_sd->P, -shadow_sd->Ng);
      if (ray->t != FLT_MAX) {
        ray->D = normalize_len(Pend - ray->P, &ray->t);
      }
      bounce++;
    }
#    ifdef __VOLUME__
    /* Attenuation for last line segment towards light. */
    if (ps->volume_stack[0].shader != SHADER_NONE) {
      kernel_volume_shadow(kg, shadow_sd, ps, ray, &throughput);
    }
#    endif
    *shadow *= throughput;
    return is_zero(throughput);
  }
#    ifdef __VOLUME__
  if (!blocked && state->volume_stack[0].shader != SHADER_NONE) {
    /* Apply attenuation from current volume shader. */
#      ifdef __SPLIT_KERNEL__
    ccl_addr_space
#      endif
        PathState *ps = shadow_blocked_volume_path_state(kg, &volume_state, state, sd, ray);
    kernel_volume_shadow(kg, shadow_sd, ps, ray, shadow);
  }
#    endif
  return blocked;
}

ccl_device bool shadow_blocked_transparent_stepped(KernelGlobals *kg,
                                                   ShaderData *sd,
                                                   ShaderData *shadow_sd,
                                                   ccl_addr_space PathState *state,
                                                   const uint visibility,
                                                   Ray *ray,
                                                   Intersection *isect,
                                                   float3 *shadow)
{
  bool blocked = scene_intersect(kg, *ray, visibility & PATH_RAY_SHADOW_OPAQUE, isect);
  bool is_transparent_isect = blocked ? shader_transparent_shadow(kg, isect) : false;
  return shadow_blocked_transparent_stepped_loop(
      kg, sd, shadow_sd, state, visibility, ray, isect, blocked, is_transparent_isect, shadow);
}

#  endif /* __KERNEL_GPU__ || !__SHADOW_RECORD_ALL__ */
#endif   /* __TRANSPARENT_SHADOWS__ */

ccl_device_inline bool shadow_blocked(KernelGlobals *kg,
                                      ShaderData *sd,
                                      ShaderData *shadow_sd,
                                      ccl_addr_space PathState *state,
                                      Ray *ray_input,
                                      float3 *shadow)
{
  Ray *ray = ray_input;
  Intersection isect;
  /* Some common early checks. */
  *shadow = make_float3(1.0f, 1.0f, 1.0f);
  if (ray->t == 0.0f) {
    return false;
  }
#ifdef __SHADOW_TRICKS__
  const uint visibility = (state->flag & PATH_RAY_SHADOW_CATCHER) ? PATH_RAY_SHADOW_NON_CATCHER :
                                                                    PATH_RAY_SHADOW;
#else
  const uint visibility = PATH_RAY_SHADOW;
#endif
  /* Do actual shadow shading. */
  /* First of all, we check if integrator requires transparent shadows.
   * if not, we use simplest and fastest ever way to calculate occlusion.
   */
#ifdef __TRANSPARENT_SHADOWS__
  if (!kernel_data.integrator.transparent_shadows)
#endif
  {
    return shadow_blocked_opaque(kg, shadow_sd, state, visibility, ray, &isect, shadow);
  }
#ifdef __TRANSPARENT_SHADOWS__
#  ifdef __SHADOW_RECORD_ALL__
  /* For the transparent shadows we try to use record-all logic on the
   * devices which supports this.
   */
  const int transparent_max_bounce = kernel_data.integrator.transparent_max_bounce;
  /* Check transparent bounces here, for volume scatter which can do
   * lighting before surface path termination is checked.
   */
  if (state->transparent_bounce >= transparent_max_bounce) {
    return true;
  }
  const uint max_hits = transparent_max_bounce - state->transparent_bounce - 1;
#    ifdef __KERNEL_GPU__
  /* On GPU we do tricky with tracing opaque ray first, this avoids speed
   * regressions in some files.
   *
   * TODO(sergey): Check why using record-all behavior causes slowdown in such
   * cases. Could that be caused by a higher spill pressure?
   */
  const bool blocked = scene_intersect(kg, *ray, visibility & PATH_RAY_SHADOW_OPAQUE, &isect);
  const bool is_transparent_isect = blocked ? shader_transparent_shadow(kg, &isect) : false;
  if (!blocked || !is_transparent_isect || max_hits + 1 >= SHADOW_STACK_MAX_HITS) {
    return shadow_blocked_transparent_stepped_loop(
        kg, sd, shadow_sd, state, visibility, ray, &isect, blocked, is_transparent_isect, shadow);
  }
#    endif /* __KERNEL_GPU__ */
  return shadow_blocked_transparent_all(
      kg, sd, shadow_sd, state, visibility, ray, max_hits, shadow);
#  else  /* __SHADOW_RECORD_ALL__ */
  /* Fallback to a slowest version which works on all devices. */
  return shadow_blocked_transparent_stepped(
      kg, sd, shadow_sd, state, visibility, ray, &isect, shadow);
#  endif /* __SHADOW_RECORD_ALL__ */
#endif   /* __TRANSPARENT_SHADOWS__ */
}

#undef SHADOW_STACK_MAX_HITS

CCL_NAMESPACE_END
