
/* ---------------------------------------------------------------------- */
/** \name Resources
 * \{ */

layout(std140) uniform renderpass_block
{
  bool renderPassDiffuse;
  bool renderPassDiffuseLight;
  bool renderPassGlossy;
  bool renderPassGlossyLight;
  bool renderPassEmit;
  bool renderPassSSSColor;
  bool renderPassEnvironment;
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Functions
 * \{ */

vec3 render_pass_diffuse_mask(vec3 diffuse_color, vec3 diffuse_light)
{
  return renderPassDiffuse ? (renderPassDiffuseLight ? diffuse_light : diffuse_color) : vec3(0.0);
}

vec3 render_pass_sss_mask(vec3 sss_color)
{
  return renderPassSSSColor ? sss_color : vec3(0.0);
}

vec3 render_pass_glossy_mask(vec3 specular_color, vec3 specular_light)
{
  return renderPassGlossy ? (renderPassGlossyLight ? specular_light : specular_color) : vec3(0.0);
}

vec3 render_pass_emission_mask(vec3 emission_light)
{
  return renderPassEmit ? emission_light : vec3(0.0);
}

/** \} */
