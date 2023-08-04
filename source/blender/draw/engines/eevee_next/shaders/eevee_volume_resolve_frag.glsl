
#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 4 : Apply final integration on top of the scene color. */

void main()
{
  vec2 uvs = gl_FragCoord.xy / vec2(textureSize(depth_tx, 0));
  float scene_depth = texture(depth_tx, uvs).r;

  VolumeResolveSample vol = volume_resolve(
      vec3(uvs, scene_depth), volume_transmittance_tx, volume_scattering_tx);

  out_radiance = vec4(vol.scattering, 0.0);
  out_transmittance = vec4(vol.transmittance, saturate(avg(vol.transmittance)));

  if (rp_buf.volume_light_id >= 0) {
    imageStore(rp_color_img,
               ivec3(ivec2(gl_FragCoord.xy), rp_buf.volume_light_id),
               vec4(vol.scattering, 1.0));
  }
}
