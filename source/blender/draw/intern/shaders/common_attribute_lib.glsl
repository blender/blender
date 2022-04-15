
/* Prototype of functions to implement to load attributes data.
 * Implementation changes based on object data type. */

vec3 attr_load_orco(vec4 orco);
vec4 attr_load_tangent(vec4 tangent);
vec3 attr_load_uv(vec3 uv);
vec4 attr_load_color(vec4 color);
vec4 attr_load_vec4(vec4 attr);
vec3 attr_load_vec3(vec3 attr);
vec2 attr_load_vec2(vec2 attr);
float attr_load_float(float attr);

vec3 attr_load_orco(samplerBuffer orco);
vec4 attr_load_tangent(samplerBuffer tangent);
vec3 attr_load_uv(samplerBuffer uv);
vec4 attr_load_color(samplerBuffer color);
vec4 attr_load_vec4(samplerBuffer attr);
vec3 attr_load_vec3(samplerBuffer attr);
vec2 attr_load_vec2(samplerBuffer attr);
float attr_load_float(samplerBuffer attr);
