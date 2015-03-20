uniform vec2 invrendertargetdim;
uniform ivec2 rendertargetdim;
/* initial uv coordinate */
varying vec2 uvcoord;

/* coordinate used for calculating radius et al set in geometry shader */
varying vec2 particlecoord;

/* downsampling coordinates */
varying vec2 downsample1;
varying vec2 downsample2;
varying vec2 downsample3;
varying vec2 downsample4;

void vert_dof_downsample()
{
	/* gather pixels from neighbors. half dimensions means we offset half a pixel to
	 * get this right though it's possible we may lose a pixel at some point */
	downsample1 = gl_MultiTexCoord0.xy + vec2(-0.5, -0.5) * invrendertargetdim;
	downsample2 = gl_MultiTexCoord0.xy + vec2(-0.5, 0.5) * invrendertargetdim;
	downsample3 = gl_MultiTexCoord0.xy + vec2(0.5, 0.5) * invrendertargetdim;
	downsample4 = gl_MultiTexCoord0.xy + vec2(0.5, -0.5) * invrendertargetdim;

	gl_Position = gl_Vertex;
}

/* geometry shading pass, calculate a texture coordinate based on the indexed id */
void vert_dof_coc_scatter_pass()
{
	vec2 pixel = vec2(rendertargetdim.x, rendertargetdim.y);
	/* some math to get the target pixel */
	int row = gl_InstanceID / rendertargetdim.x;
	int column = gl_InstanceID % rendertargetdim.x;
	uvcoord = (vec2(column, row) + vec2(0.5)) / pixel;

	vec2 pos = uvcoord * 2.0 - vec2(1.0);
	gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);

//	uvcoord = vec2(0.5, 0.5);
//	gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

void vert_dof_final()
{
	uvcoord = gl_MultiTexCoord0.xy;
	gl_Position = gl_Vertex;
}

void main()
{
#if defined(FIRST_PASS)
	vert_dof_downsample();
#elif defined(SECOND_PASS)
	vert_dof_coc_scatter_pass();
#else
	vert_dof_final();
#endif
}