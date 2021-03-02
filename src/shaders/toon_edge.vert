#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invModel;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inTexID;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out uint fragTexID;
layout(location = 3) out uint fragEdge;

void main() {
    vec3 pos = inPosition + inNormal * 0.05;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(pos, 1.0);
    fragColor = vec3(0.05, 0.05, 0.05);
    fragTexCoord = inTexCoord;
    fragTexID = inTexID;
    fragEdge = 1;
}