#ifndef __RAS_GRAYSCALE2DFILTER
#define __RAS_GRAYSCALE2DFILTER

char * GrayScaleFragmentShader=STRINGIFY(
uniform sampler2D sampler0;

void main(void)
{
	vec4 texcolor = texture2D(sampler0, gl_TexCoord[0].st); 
	float gray = dot(texcolor.rgb, vec3(0.299, 0.587, 0.114));
    gl_FragColor = vec4(gray, gray, gray, texcolor.a);
}
);
#endif
