void particle_info(vec4 sprops,
                   vec4 loc,
                   vec3 vel,
                   vec3 avel,
                   out float index,
                   out float random,
                   out float age,
                   out float life_time,
                   out vec3 location,
                   out float size,
                   out vec3 velocity,
                   out vec3 angular_velocity)
{
  index = sprops.x;
  random = loc.w;
  age = sprops.y;
  life_time = sprops.z;
  size = sprops.w;

  location = loc.xyz;
  velocity = vel;
  angular_velocity = avel;
}
