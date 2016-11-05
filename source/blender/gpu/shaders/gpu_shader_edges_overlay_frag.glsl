
#define SMOOTH 1

const float transitionWidth = 1.0;

uniform vec4 fillColor = vec4(0);
uniform vec4 outlineColor = vec4(0,0,0,1);

noperspective in vec3 distanceToOutline;

out vec4 FragColor;

void main() {
	float edgeness = min(min(distanceToOutline.x, distanceToOutline.y), distanceToOutline.z);
#if SMOOTH
	FragColor = mix(outlineColor, fillColor, smoothstep(0, transitionWidth, edgeness));
#else
	FragColor = (edgeness <= 0) ? outlineColor : fillColor;
#endif
}
