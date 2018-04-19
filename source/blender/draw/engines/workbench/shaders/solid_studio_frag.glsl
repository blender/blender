uniform vec3 color;
uniform vec3 light_direction = vec3(0.0, 0.0, 1.0);

uniform vec3 world_diffuse_light_xp = vec3(0.5, 0.5, 0.6);
uniform vec3 world_diffuse_light_xn = vec3(0.5, 0.5, 0.6);
uniform vec3 world_diffuse_light_yp = vec3(0.5, 0.5, 0.6);
uniform vec3 world_diffuse_light_yn = vec3(0.5, 0.5, 0.6);
uniform vec3 world_diffuse_light_zp = vec3(0.8, 0.8, 0.8);
uniform vec3 world_diffuse_light_zn = vec3(0.0, 0.0, 0.0);

in vec3 normal_viewport;
out vec4 fragColor;
#define USE_WORLD_DIFFUSE
#define AMBIENT_COLOR vec3(0.2, 0.2, 0.2)

void main()
{

#ifdef USE_WORLD_DIFFUSE
	vec3 diffuse_light_color = get_world_diffuse_light(normal_viewport, world_diffuse_light_xp, world_diffuse_light_xn, world_diffuse_light_yp, world_diffuse_light_yn, world_diffuse_light_zp, world_diffuse_light_zn);
	vec3 shaded_color = (AMBIENT_COLOR + diffuse_light_color)  * color;

#else
	float intensity = lambert_diffuse(light_direction, normal_viewport);
	vec3 shaded_color = color * intensity;

#endif
	fragColor = vec4(shaded_color, 1.0);
}
