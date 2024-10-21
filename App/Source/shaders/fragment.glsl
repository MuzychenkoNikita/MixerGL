#version 330 core
in vec2 TexCoords;  // Texture coordinates from vertex shader
uniform sampler2D texture1; // Texture sampler
uniform bool useTexture;    // Boolean indicating whether to use texture or color
uniform vec4 color;         // The color to use if not using texture
out vec4 FragColor;         // Output color

void main()
{
    if (useTexture) {
        FragColor = texture(texture1, TexCoords); // Sample the texture
    } else {
        FragColor = color; // Use the specified color if no texture
    }
}