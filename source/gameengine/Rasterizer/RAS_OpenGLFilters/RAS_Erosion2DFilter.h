#ifndef __RAS_EROSION2DFILTER
#define __RAS_EROSION2DFILTER

char * ErosionFragmentShader=STRINGIFY(
uniform sampler2D sampler0;
uniform vec2 tc_offset[9];

void main(void)
{
    vec4 sample[9];
    vec4 minValue = vec4(1.0);

    for (int i = 0; i < 9; i++)
    {
        sample[i] = texture2D(sampler0, 
                              gl_TexCoord[0].st + tc_offset[i]);
        minValue = min(sample[i], minValue);
    }

    gl_FragColor = minValue;
}
);
#endif
