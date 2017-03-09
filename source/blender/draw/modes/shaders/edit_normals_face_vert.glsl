
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;
uniform mat4 ProjectionMatrix;
uniform float normalSize;

in vec3 pos;
in vec4 norAndFlag;

flat out vec4 v1;
flat out vec4 v2;

void main()
{
	v1 = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vec3 n = normalize(NormalMatrix * norAndFlag.xyz); /* viewspace */
	v2 = v1 + ProjectionMatrix * vec4(n * normalSize, 0.0);
}
