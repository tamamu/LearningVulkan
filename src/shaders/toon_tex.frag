#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invModel;
} ubo;

layout(binding = 1) uniform sampler2D[8] texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uint fragTexID;
layout(location = 3) flat in uint fragEdge;

layout(location = 0) out vec4 outColor;

float toon(float diffuse) {
    if (diffuse > 0.5) {
        return diffuse = 1.0;
    } else if (diffuse > 0.2) {
        return diffuse = 0.7;
    } else {
        return diffuse = 0.4;
    }
}

void main() {
    if (fragEdge == 1) {
        outColor = vec4(fragColor, 1.0);
    } else {
        vec3 lightDirection = vec3(0.3, 0.7, 0.0);
        vec3 invLight = normalize(ubo.invModel * vec4(lightDirection, 0.0)).xyz;
        float diffuse  = clamp(dot(fragColor, invLight), 0.0, 1.0);
        float td = toon(diffuse);
        vec4 smpColor = vec4(td, td, td, 1.0);
        //outColor = vec4(fragTexCoord, 0.0, 1.0);
        outColor = texture(texSampler[fragTexID], fragTexCoord) * smpColor;
    }
}