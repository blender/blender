
uniform sampler2D hdrColorBuf;

in vec4 uvcoordsvar;

out vec4 fragColor;

void main() {
	fragColor = texture(hdrColorBuf, uvcoordsvar.st);
}