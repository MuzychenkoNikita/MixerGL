#version 330 core
layout(location = 0) in vec3 aPos; // Position attribute
layout(location = 1) in vec2 aTexCoords; // Texture coordinates attribute

out vec2 TexCoords; // Pass the texture coordinates to the fragment shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
