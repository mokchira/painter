#version 460

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D image;
layout(set = 0, binding = 1) uniform Brush {
    float x;
    float y;
    float r;
    int   mode;
} brush;

void main()
{
    vec4 tex = texture(image, uv).rgba;
    vec2 brushPos = vec2(brush.x, brush.y);
    float d = distance(uv, brushPos);
    if (d < brush.r)
        outColor = vec4(0.7, 0.8, 0.4, 1);
    else
        outColor = tex;
}