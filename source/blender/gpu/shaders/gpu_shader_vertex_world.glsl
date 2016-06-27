
varying vec3 varposition;
varying vec3 varnormal;


/* Color, keep in sync with: gpu_shader_vertex.glsl */

float srgb_to_linearrgb(float c)
{
	if (c < 0.04045)
		return (c < 0.0) ? 0.0 : c * (1.0 / 12.92);
	else
		return pow((c + 0.055) * (1.0 / 1.055), 2.4);
}

void srgb_to_linearrgb(vec3 col_from, out vec3 col_to)
{
	col_to.r = srgb_to_linearrgb(col_from.r);
	col_to.g = srgb_to_linearrgb(col_from.g);
	col_to.b = srgb_to_linearrgb(col_from.b);
}

void srgb_to_linearrgb(vec4 col_from, out vec4 col_to)
{
	col_to.r = srgb_to_linearrgb(col_from.r);
	col_to.g = srgb_to_linearrgb(col_from.g);
	col_to.b = srgb_to_linearrgb(col_from.b);
	col_to.a = col_from.a;
}

bool is_srgb(int info)
{
#ifdef USE_NEW_SHADING
	return (info == 1)? true: false;
#else
	return false;
#endif
}

void set_var_from_attr(float attr, int info, out float var)
{
	var = attr;
}

void set_var_from_attr(vec2 attr, int info, out vec2 var)
{
	var = attr;
}

void set_var_from_attr(vec3 attr, int info, out vec3 var)
{
	if (is_srgb(info)) {
		srgb_to_linearrgb(attr, var);
	}
	else {
		var = attr;
	}
}

void set_var_from_attr(vec4 attr, int info, out vec4 var)
{
	if (is_srgb(info)) {
		srgb_to_linearrgb(attr, var);
	}
	else {
		var = attr;
	}
}

/* end color code */


void main()
{
	/* position does not need to be transformed, we already have it */
	gl_Position = gl_Vertex;

	varposition = gl_Vertex.xyz;

	varnormal = normalize(-varposition);

