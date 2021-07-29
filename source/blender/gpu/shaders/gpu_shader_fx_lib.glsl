/* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
 * we change the factors from the article to fit the OpennGL model.  */
#ifdef PERSP_MATRIX

/* perspective camera code */

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec3 viewvec_origin, in vec3 viewvec_diff, in float depth)
{
	float d = 2.0 * depth - 1.0;

	float zview = -gl_ProjectionMatrix[3][2] / (d + gl_ProjectionMatrix[2][2]);

	return zview * (viewvec_origin + vec3(uvcoords, 0.0) * viewvec_diff);
}

vec4 get_view_space_z_from_depth(in vec4 near, in vec4 range, in vec4 depth)
{
	vec4 d = 2.0 * depth - vec4(1.0);

	/* return positive value, so sign differs! */
	return vec4(gl_ProjectionMatrix[3][2]) / (d + vec4(gl_ProjectionMatrix[2][2]));
}

#else
/* orthographic camera code */

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec3 viewvec_origin, in vec3 viewvec_diff, in float depth)
{
	vec3 offset = vec3(uvcoords, depth);

	return vec3(viewvec_origin + offset * viewvec_diff);
}

vec4 get_view_space_z_from_depth(in vec4 near, in vec4 range, in vec4 depth)
{
	return -(near + depth * range);
}

#endif
