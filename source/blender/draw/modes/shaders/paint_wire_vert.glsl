
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in int data;

flat out int finalFlag;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* Temp hack for william to start using blender 2.8 for icons. Will be removed by T54910 */
	gl_Position.z -= 0.0001;

	finalFlag = data;
}
