
uniform mat4 ModelViewProjectionMatrix;
uniform float slicePosition;
uniform int sliceAxis; /* -1 is no slice, 0 is X, 1 is Y, 2 is Z. */

in vec3 pos;

#ifdef VOLUME_SLICE
in vec3 uvs;

out vec3 localPos;
#endif

void main()
{
#ifdef VOLUME_SLICE
	if (sliceAxis == 0) {
		localPos = vec3(slicePosition * 2.0 - 1.0, pos.xy);
	}
	else if (sliceAxis == 1) {
		localPos = vec3(pos.x, slicePosition * 2.0 - 1.0, pos.y);
	}
	else {
		localPos = vec3(pos.xy, slicePosition * 2.0 - 1.0);
	}

	gl_Position = ModelViewProjectionMatrix * vec4(localPos, 1.0);
#else
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#endif
}
