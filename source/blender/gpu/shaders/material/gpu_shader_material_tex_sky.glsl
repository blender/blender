float sky_angle_between(float thetav, float phiv, float theta, float phi)
{
  float cospsi = sin(thetav) * sin(theta) * cos(phi - phiv) + cos(thetav) * cos(theta);

  if (cospsi > 1.0) {
    return 0.0;
  }
  if (cospsi < -1.0) {
    return M_PI;
  }

  return acos(cospsi);
}

vec3 sky_spherical_coordinates(vec3 dir)
{
  return vec3(M_PI_2 - atan(dir.z, length(dir.xy)), atan(dir.x, dir.y), 0);
}

/* Preetham */
/* lam03+lam4: 5 floats passed as vec4+float */
float sky_perez_function(vec4 lam03, float lam4, float theta, float gamma)
{
  float ctheta = cos(theta);
  float cgamma = cos(gamma);

  return (1.0 + lam03[0] * exp(lam03[1] / ctheta)) *
         (1.0 + lam03[2] * exp(lam03[3] * gamma) + lam4 * cgamma * cgamma);
}

vec3 xyY_to_xyz(float x, float y, float Y)
{
  float X, Z;

  if (y != 0.0) {
    X = (x / y) * Y;
  }
  else {
    X = 0.0;
  }

  if (y != 0.0 && Y != 0.0) {
    Z = ((1.0 - x - y) / y) * Y;
  }
  else {
    Z = 0.0;
  }

  return vec3(X, Y, Z);
}

void node_tex_sky_preetham(vec3 co,
                           vec4 config_Y03,
                           float config_Y4,
                           vec4 config_x03,
                           float config_x4,
                           vec4 config_y03,
                           float config_y4,
                           vec2 sun_angles,
                           vec3 radiance,
                           vec3 xyz_to_r,
                           vec3 xyz_to_g,
                           vec3 xyz_to_b,
                           out vec4 color)
{
  /* convert vector to spherical coordinates */
  vec3 spherical = sky_spherical_coordinates(co);
  float theta = spherical[0];
  float phi = spherical[1];

  float suntheta = sun_angles[0];
  float sunphi = sun_angles[1];

  /* angle between sun direction and dir */
  float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2 - 0.001);

  /* compute xyY color space values */
  float Y = radiance[0] * sky_perez_function(config_Y03, config_Y4, theta, gamma);
  float x = radiance[1] * sky_perez_function(config_x03, config_x4, theta, gamma);
  float y = radiance[2] * sky_perez_function(config_y03, config_y4, theta, gamma);

  /* convert to RGB */
  vec3 xyz = xyY_to_xyz(x, y, Y);
  color = vec4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1);
}

/* Hosek / Wilkie */
float sky_radiance_hosekwilkie(
    vec4 config03, vec4 config47, float config8, float theta, float gamma)
{
  float ctheta = cos(theta);
  float cgamma = cos(gamma);

  float expM = exp(config47[0] * gamma);
  float rayM = cgamma * cgamma;
  float mieM = (1.0 + rayM) / pow((1.0 + config8 * config8 - 2.0 * config8 * cgamma), 1.5);
  float zenith = sqrt(ctheta);

  return (1.0 + config03[0] * exp(config03[1] / (ctheta + 0.01))) *
         (config03[2] + config03[3] * expM + config47[1] * rayM + config47[2] * mieM +
          config47[3] * zenith);
}

void node_tex_sky_hosekwilkie(vec3 co,
                              vec4 config_x03,
                              vec4 config_x47,
                              vec4 config_y03,
                              vec4 config_y47,
                              vec4 config_z03,
                              vec4 config_z47,
                              vec3 config_xyz8,
                              vec2 sun_angles,
                              vec3 radiance,
                              vec3 xyz_to_r,
                              vec3 xyz_to_g,
                              vec3 xyz_to_b,
                              out vec4 color)
{
  /* convert vector to spherical coordinates */
  vec3 spherical = sky_spherical_coordinates(co);
  float theta = spherical[0];
  float phi = spherical[1];

  float suntheta = sun_angles[0];
  float sunphi = sun_angles[1];

  /* angle between sun direction and dir */
  float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2 - 0.001);

  vec3 xyz;
  xyz.x = sky_radiance_hosekwilkie(config_x03, config_x47, config_xyz8[0], theta, gamma) *
          radiance.x;
  xyz.y = sky_radiance_hosekwilkie(config_y03, config_y47, config_xyz8[1], theta, gamma) *
          radiance.y;
  xyz.z = sky_radiance_hosekwilkie(config_z03, config_z47, config_xyz8[2], theta, gamma) *
          radiance.z;

  color = vec4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1);
}

void node_tex_sky_nishita(vec3 co, out vec4 color)
{
  color = vec4(1.0);
}
