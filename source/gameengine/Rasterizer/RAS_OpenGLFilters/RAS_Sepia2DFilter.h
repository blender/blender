#ifndef __RAS_SEPIA2DFILTER
#define __RAS_SEPIA2DFILTER

char * SepiaFragmentShader=STRINGIFY(
uniform sampler2D bgl_RenderedTexture;

void main(void)
{
	vec4 texcolor = texture2D(bgl_RenderedTexture, gl_TexCoord[0].st); 
	float gray = dot(texcolor.rgb, vec3(0.299, 0.587, 0.114));
	gl_FragColor = vec4(gray * vec3(1.2, 1.0, 0.8), texcolor.a);
}
);
#endif
