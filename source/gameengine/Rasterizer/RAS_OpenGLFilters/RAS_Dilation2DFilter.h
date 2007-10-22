#ifndef __RAS_DILATION2DFILTER
#define __RAS_DILATION2DFILTER

char * DilationFragmentShader=STRINGIFY(
uniform sampler2D sampler0;
uniform vec2 tc_offset[9];

void main(void)
{
    vec4 sample[9];
    vec4 maxValue = vec4(0.0);

    for (int i = 0; i < 9; i++)
    {
        sample[i] = texture2D(sampler0, 
                              gl_TexCoord[0].st + tc_offset[i]);
        maxValue = max(sample[i], maxValue);
    }

    gl_FragColor = maxValue;
}
);
#endif

