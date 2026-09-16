#version 450

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;

layout(location = 0) out vec3 Position;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 UV;

layout(push_constant, std430) uniform pc {
    mat4 view;
    mat4 proj;
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
} ubo;

void main() {

    UV = vertexUV;

    mat4 normalMatrix = transpose(inverse(ubo.model));
    Normal = normalize((normalMatrix * vec4(vertexNormal, 0)).xyz);

    Position = (ubo.model * vec4(vertexPosition, 1.0)).xyz;
    gl_Position = proj * view * vec4(Position, 1.0);
}