
uniform mat4 ModelViewProjectionMatrix;
uniform float alpha;

in vec2 pos;
in vec4 color;

noperspective out vec4 finalColor;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	finalColor = vec4(color[0], color[1], color[2], color[3] * alpha);
}
