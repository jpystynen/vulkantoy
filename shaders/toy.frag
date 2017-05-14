#version 450 core

layout (location = 0) out vec4 out_fragColor;

layout (std140, set = 0, binding = 0) readonly uniform u_uniformBuffer
{
    vec4 iChannelResolution[4];
    vec4 iMouse;
    vec4 iDate;
    vec4 iResolution;

    vec4 iChannelTime;

    //float iGlobalDelta;
    //float iGlobalFrame;
    //float iGlobalTime;
    //float iSampleRate;
    vec4 globalVariables_;
};

layout(set = 1, binding = 0) uniform sampler2D iChannel0;
layout(set = 1, binding = 1) uniform sampler2D iChannel1;
layout(set = 1, binding = 2) uniform sampler2D iChannel2;
layout(set = 1, binding = 3) uniform sampler2D iChannel3;

#define iGlobalDelta    globalVariables_.x
#define iGlobalFrame    globalVariables_.y
#define iGlobalTime     globalVariables_.z
#define iSampleRate     globalVariables_.w

#define texture2D       texture

#define tex0            iChannel0
#define tex1            iChannel1
#define tex2            iChannel2
#define tex3            iChannel3

#define gl_FragColor    out_fragColor

#define DEF_USE_IMAGE_SHADER    1

///////////////////////////////////////////////////////////////////////////////
// SHADERTOY SHADER HERE

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    const vec2 middle = iResolution.xy * 0.5;
    vec2 uv = vec2(fragCoord.xy / iResolution.xy);
    uv.y = 1.0 - uv.y;
    if (uv.x < 0.5 && uv.y < 0.5)
    {
        fragColor = texture(iChannel0, uv.xy * vec2(2.0));
    }
    else if (uv.x < 0.5 && uv.y >= 0.5)
    {
        fragColor = texture(iChannel1, uv.xy * vec2(2.0));
    }
    else if (uv.x >= 0.5 && uv.y < 0.5)
    {
        fragColor = texture(iChannel2, uv.xy * vec2(2.0));
    }
    else
    {
        fragColor = texture(iChannel3, uv.xy * vec2(2.0));
    }
}

// SHADERTOY SHADER ENDS HERE
///////////////////////////////////////////////////////////////////////////////

void main(void)
{
#if (DEF_USE_IMAGE_SHADER == 1)
    mainImage(out_fragColor, vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y));
#endif
}

///////////////////////////////////////////////////////////////////////////////
