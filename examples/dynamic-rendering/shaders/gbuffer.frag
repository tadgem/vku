#version 450

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;

// locations must match the attachment mapping in main.cpp:
//   0 -> normalBuffer, 1 -> colourBuffer, 2 -> positionBuffer
layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outPosition;

layout(set = 0, binding = 1) uniform sampler2D diffuse;

void main() {
    outNormal   = vec4(normalize(Normal), 1.0);
    outColor    = vec4(texture(diffuse, UV).rgb, 1.0);
    outPosition = vec4(Position, 1.0);
}
