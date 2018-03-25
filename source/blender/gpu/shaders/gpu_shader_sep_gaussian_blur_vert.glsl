
out vec2 texCoord_interp;

void main()
{
	const vec4 vert[3] = vec4[3](
		vec3(-1.0, -1.0,  0.0,  0.0),
		vec3( 3.0, -1.0,  2.0,  0.0),
		vec3(-1.0,  3.0,  0.0,  2.0)
	);

	gl_Position = vec4(vert[gl_VertexID].xy, 0.0, 1.0);
	texCoord_interp = vert[gl_VertexID].zw;
}
