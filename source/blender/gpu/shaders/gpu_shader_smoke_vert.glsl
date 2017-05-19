
uniform mat4 ModelViewProjectionMatrix;

out vec3 coords;

uniform vec3 min_location;
uniform vec3 invsize;
uniform vec3 ob_sizei;

void main()
{
	// TODO: swap gl_Vertex for vec3 pos, update smoke setup code
	gl_Position = ModelViewProjectionMatrix * vec4(gl_Vertex.xyz * ob_sizei, 1.0);
	coords = (gl_Vertex.xyz - min_location) * invsize;
}
