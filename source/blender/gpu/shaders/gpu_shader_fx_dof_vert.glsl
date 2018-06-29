uniform vec2 invrendertargetdim;

//texture coordinates for framebuffer read
varying vec4 uvcoordsvar;

/* color texture coordinates, offset by a small amount */
varying vec2 color_uv1;
varying vec2 color_uv2;

varying vec2 depth_uv1;
varying vec2 depth_uv2;
varying vec2 depth_uv3;
varying vec2 depth_uv4;

//very simple shader for gull screen FX, just pass values on

void vert_generic()
{
	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
}

void vert_dof_first_pass()
{
	/* we offset the texture coordinates by 1.5 pixel,
	 * then we reuse that to sample the surrounding pixels */
	color_uv1 = gl_MultiTexCoord0.xy + vec2(-1.5, -1.5) * invrendertargetdim;
	color_uv2 = gl_MultiTexCoord0.xy + vec2(0.5, -1.5) * invrendertargetdim;

	depth_uv1 = gl_MultiTexCoord0.xy + vec2(-1.5, -1.5) * invrendertargetdim;
	depth_uv2 = gl_MultiTexCoord0.xy + vec2(-0.5, -1.5) * invrendertargetdim;
	depth_uv3 = gl_MultiTexCoord0.xy + vec2(0.5, -1.5) * invrendertargetdim;
	depth_uv4 = gl_MultiTexCoord0.xy + vec2(1.5, -1.5) * invrendertargetdim;

	gl_Position = gl_Vertex;
}

void vert_dof_fourth_pass()
{
	vec4 halfpixel = vec4(-0.5, 0.5, -0.5, 0.5);
	uvcoordsvar = gl_MultiTexCoord0.xxyy +
	              halfpixel *
	              vec4(invrendertargetdim.x,
	                   invrendertargetdim.x,
	                   invrendertargetdim.y,
	                   invrendertargetdim.y);

	gl_Position = gl_Vertex;
}

void vert_dof_fifth_pass()
{
	vec4 halfpixel = vec4(-0.5, 0.5, -0.5, 0.5);
	color_uv1 = vec2(0.5, 1.5) * invrendertargetdim;

	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
}

void main()
{
#ifdef FIRST_PASS
	vert_dof_first_pass();
#elif defined(FOURTH_PASS)
	vert_dof_fourth_pass();
#elif defined(FIFTH_PASS)
	vert_dof_fifth_pass();
#else
	vert_generic();
#endif
}
