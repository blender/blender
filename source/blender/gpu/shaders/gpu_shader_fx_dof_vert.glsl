uniform vec2 invrendertargetdim;


#if __VERSION__ == 120
	attribute vec2 pos;
	attribute vec2 uvs;

	//texture coordinates for framebuffer read
	varying vec4 uvcoordsvar;

	/* color texture coordinates, offset by a small amount */
	varying vec2 color_uv1;
	varying vec2 color_uv2;

	varying vec2 depth_uv1;
	varying vec2 depth_uv2;
	varying vec2 depth_uv3;
	varying vec2 depth_uv4;
#else
	in vec2 pos;
	in vec2 uvs;

	//texture coordinates for framebuffer read
	out vec4 uvcoordsvar;

	/* color texture coordinates, offset by a small amount */
	out vec2 color_uv1;
	out vec2 color_uv2;

	out vec2 depth_uv1;
	out vec2 depth_uv2;
	out vec2 depth_uv3;
	out vec2 depth_uv4;
#endif

//very simple shader for gull screen FX, just pass values on

void vert_generic()
{
	uvcoordsvar = vec4(uvs, 0.0, 0.0);
	gl_Position = vec4(pos, 0.0, 1.0);
}

void vert_dof_first_pass()
{
	/* we offset the texture coordinates by 1.5 pixel,
	 * then we reuse that to sample the surrounding pixels */
	color_uv1 = uvs.xy + vec2(-1.5, -1.5) * invrendertargetdim;
	color_uv2 = uvs.xy + vec2(0.5, -1.5) * invrendertargetdim;

	depth_uv1 = uvs.xy + vec2(-1.5, -1.5) * invrendertargetdim;
	depth_uv2 = uvs.xy + vec2(-0.5, -1.5) * invrendertargetdim;
	depth_uv3 = uvs.xy + vec2(0.5, -1.5) * invrendertargetdim;
	depth_uv4 = uvs.xy + vec2(1.5, -1.5) * invrendertargetdim;

	gl_Position = vec4(pos, 0.0, 1.0);
}

void vert_dof_fourth_pass()
{
	vec4 halfpixel = vec4(-0.5, 0.5, -0.5, 0.5);
	uvcoordsvar = uvs.xxyy +
	              halfpixel *
	              vec4(invrendertargetdim.x,
	                   invrendertargetdim.x,
	                   invrendertargetdim.y,
	                   invrendertargetdim.y);

	gl_Position = vec4(pos, 0.0, 1.0);
}

void vert_dof_fifth_pass()
{
	vec4 halfpixel = vec4(-0.5, 0.5, -0.5, 0.5);
	color_uv1 = vec2(0.5, 1.5) * invrendertargetdim;

	uvcoordsvar = vec4(uvs, 0.0, 0.0);
	gl_Position = vec4(pos, 0.0, 1.0);
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

