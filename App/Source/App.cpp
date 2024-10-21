#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Core/imgui/imgui.h"
#include "Core/imgui/imgui_impl_glfw.h"
#include "Core/imgui/imgui_impl_opengl3.h"

#include "Core/shader_m.h"
#include "Core/camera.h"
#include "Core/tinyfiledialogs.h"

#include <iostream>
#include <vector>
#include <string>

// Declare the Object struct before function declarations
struct Object {
    glm::vec3 position;
    glm::vec3 scale;
    glm::vec4 color; // RGBA color
    bool isCube;     // true if cube, false if sphere
    unsigned int textureID; // ID of the texture to be applied
};

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// Helper functions for raycasting and rendering
glm::vec3 ScreenToWorldRay(float mouseX, float mouseY, const glm::mat4& view, const glm::mat4& projection);
bool RayIntersectsObject(const glm::vec3& ray_origin, const glm::vec3& ray_direction, const Object& object);
void RenderScene(Shader& shader);
void RenderImGui(Shader ourShader);
void RenderImGuiConsole();
void Log(const std::string& message);
void RenderGizmo(Shader& shader, const Object& obj);
void DrawArrow(const glm::vec3& start, const glm::vec3& end);
float DistanceFromRayToLineSegment(const glm::vec3& ray_origin, const glm::vec3& ray_direction, const glm::vec3& line_start, const glm::vec3& line_end);
void DrawCircle(const glm::vec3& center, const glm::vec3& normal, float radius, Shader& shader);
void RenderTranslationGizmo(Shader& shader, const Object& obj);
void RenderRotationGizmo(const Object& obj, Shader& gizmoShader);
void RenderScalingGizmo(Shader& shader, const Object& obj);
void HandleGizmoInteraction(GLFWwindow* window);
bool IsRayNearCircle(const glm::vec3& ray_origin, const glm::vec3& ray_direction, const glm::vec3& center, const glm::vec3& normal, float radius);
void RenderObjectSettings();
void SetupFrameBuffer();
unsigned int LoadTexture(const char* path);
void SetupSphere();
void DrawSmallCube(Shader& shader, const glm::vec3& position, const glm::vec4& color);


// Global settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// Camera setup
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing for frame handling
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Example grid size and step values
float gridSize = 10.0f;
float gridStep = 1.0f;

// Vector to store created objects
std::vector<Object> objects;
int selectedObject = -1;

// Gizmo settings 
int selectedAxis = -1; // -1 means no axis selected, 0 = X, 1 = Y, 2 = Z
bool isDragging = false;
glm::vec3 initialClickPosition;

// Options for obj manipulation
enum TransformationMode {
    TRANSLATE,
    ROTATE,
    SCALE
};

TransformationMode currentMode = TRANSLATE;

bool settingsDisplayed = false;

float movementSensitivity = 4.0f;


// VAOs for cube and sphere
unsigned int cubeVAO, VBO, texture1;
unsigned int gridVAO, gridVBO;
unsigned int sphereVAO;
unsigned int sphereVertexCount;
unsigned int gridVertexCount = 0; // Variable to store the number of grid vertices
unsigned int fbo, fboTexture, rbo;

// Vector to store debug messages for ImGui console
std::vector<std::string> debugMessages;

void SetupGrid(float size, float step);
void DrawGrid(Shader& shader, const glm::mat4& projection, const glm::mat4& view);

int main()
{
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create GLFW window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "MixerGL - by Nikita M.", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Make the cursor visible
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);



    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load all OpenGL functions using GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Enable depth test for proper 3D rendering
    glEnable(GL_DEPTH_TEST);

    // Grid setup
    SetupGrid(gridSize, gridStep);

    // Sphere setup
    SetupSphere();

    // Load shaders
    Shader ourShader("Source/shaders/vertex.glsl", "Source/shaders/fragment.glsl");
    Shader gridShader("Source/shaders/grid_vertex.glsl", "Source/shaders/grid_fragment.glsl");
    Shader gizmoShader("Source/shaders/gizmo_vertex.glsl", "Source/shaders/gizmo_fragment.glsl");

    // Load and create a texture
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);

    // Set the texture wrapping and filtering options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load the texture image
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // Flip texture vertically
    unsigned char* data = stbi_load("Source/textures/texture1.jpg", &width, &height, &nrChannels, 0);
    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data); // Free the texture data

    // Cube vertices with positions and texture coordinates
    float vertices[] = {
        // Positions          // Texture Coords
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    // VAO and VBO setup for the cube
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Add a default cube to the scene at startup
    glm::vec3 startPosition = camera.Position + glm::vec3(0.0f, 0.5f, -3.0f); // Create the cube in front of the camera
    objects.push_back({ startPosition, glm::vec3(1.0f), glm::vec4(1.0f), true });
    Log("Default cube created at position " + glm::to_string(startPosition) + " with scale (1.0, 1.0, 1.0)");


    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate frame time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        settingsDisplayed = false;

        // Process input
        processInput(window);

        // Start a new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear the screen for rendering
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render the 3D scene
        RenderScene(ourShader);

        // Render the XYZ gizmo if an object is selected
        if (selectedObject >= 0) {
            switch (currentMode) {
            case TRANSLATE:
                RenderTranslationGizmo(gizmoShader, objects[selectedObject]);
                break;
            case ROTATE:
                RenderRotationGizmo(objects[selectedObject], gizmoShader);
                break;
            case SCALE:
                RenderScalingGizmo(gizmoShader, objects[selectedObject]);
                break;
            }
        }

        // Handle dragging and transformations
        if (isDragging && selectedObject >= 0 && selectedAxis >= 0) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

            glm::vec3 ray_origin = camera.Position;
            glm::vec3 ray_direction = ScreenToWorldRay(static_cast<float>(xpos), static_cast<float>(ypos), view, projection);
            glm::vec3 currentRayPosition = ray_origin + ray_direction;

            glm::vec3 axisDirection;
            if (selectedAxis == 0) {
                axisDirection = glm::vec3(1.0f, 0.0f, 0.0f);
            }
            else if (selectedAxis == 1) {
                axisDirection = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            else if (selectedAxis == 2) {
                axisDirection = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            // Calculate the movement and apply the sensitivity factor
            glm::vec3 movement = glm::dot(currentRayPosition - initialClickPosition, axisDirection) * axisDirection * movementSensitivity;

            // Apply the transformation based on the current mode
            switch (currentMode) {
            case TRANSLATE:
                objects[selectedObject].position += movement;
                Log("Translating along axis " + std::to_string(selectedAxis) +
                    ", movement: " + glm::to_string(movement) +
                    ", new position: " + glm::to_string(objects[selectedObject].position));
                break;

            case ROTATE: {
                float angle = glm::length(movement) * 5.0f * movementSensitivity; // Apply sensitivity to the rotation
                glm::vec3 rotationAxis = glm::normalize(axisDirection);
                glm::quat rotation = glm::angleAxis(glm::radians(angle), rotationAxis);
                glm::vec3 objectCenter = objects[selectedObject].position;
                glm::vec3 newPosition = rotation * (objects[selectedObject].position - objectCenter) + objectCenter;
                objects[selectedObject].position = newPosition;

                Log("Rotating around axis " + std::to_string(selectedAxis) +
                    ", angle: " + std::to_string(angle) +
                    ", new position: " + glm::to_string(objects[selectedObject].position));
                break;
            }

            case SCALE:
                objects[selectedObject].scale += movement;
                objects[selectedObject].scale = glm::max(objects[selectedObject].scale, glm::vec3(0.1f));
                Log("Scaling along axis " + std::to_string(selectedAxis) +
                    ", scale change: " + glm::to_string(movement) +
                    ", new scale: " + glm::to_string(objects[selectedObject].scale));
                break;
            }

            initialClickPosition = currentRayPosition;
        }

        // Render the grid
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        DrawGrid(gridShader, projection, view);

        // Render ImGui interface
        RenderImGui(ourShader);
        RenderImGuiConsole();
        RenderObjectSettings();

        // End ImGui frame and render ImGui data
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Handle multi-viewports if enabled
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        // Swap buffers and poll for events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

// Framebuffer size callback for resizing the window
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Mouse callback for handling camera rotation
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    // Only rotate the camera if the right mouse button is pressed
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top

        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.1f; // Adjust this value for mouse sensitivity
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        camera.ProcessMouseMovement(xoffset, yoffset);
    }
    else {
        firstMouse = true; // Reset when the right mouse button is not pressed
    }
}

// Mouse button callback for raycasting
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // Check if the mouse is over any ImGui window, and if so, don't perform object selection
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        glm::vec3 ray_origin = camera.Position;
        glm::vec3 ray_direction = ScreenToWorldRay(static_cast<float>(xpos), static_cast<float>(ypos), view, projection);

        selectedAxis = -1; // Reset selected axis

        // Check if a gizmo is clicked if an object is selected
        if (selectedObject >= 0) {
            const float gizmoClickRadius = 0.2f; // Fine-tune this value as needed
            Object& obj = objects[selectedObject];

            // Check if the click is near the X axis arrow
            if (DistanceFromRayToLineSegment(ray_origin, ray_direction, obj.position, obj.position + glm::vec3(1.0f, 0.0f, 0.0f)) < gizmoClickRadius) {
                selectedAxis = 0; // X-axis selected
                isDragging = true;
                Log("Selected X-axis for dragging");
            }
            // Check if the click is near the Y axis arrow
            else if (DistanceFromRayToLineSegment(ray_origin, ray_direction, obj.position, obj.position + glm::vec3(0.0f, 1.0f, 0.0f)) < gizmoClickRadius) {
                selectedAxis = 1; // Y-axis selected
                isDragging = true;
                Log("Selected Y-axis for dragging");
            }
            // Check if the click is near the Z axis arrow
            else if (DistanceFromRayToLineSegment(ray_origin, ray_direction, obj.position, obj.position + glm::vec3(0.0f, 0.0f, 1.0f)) < gizmoClickRadius) {
                selectedAxis = 2; // Z-axis selected
                isDragging = true;
                Log("Selected Z-axis for dragging");
            }

            // Store the initial click position if dragging started
            if (isDragging) {
                initialClickPosition = ray_origin + ray_direction;
                Log("Started dragging along axis " + std::to_string(selectedAxis));
            }
            else {
                Log("Gizmo not selected");
            }
        }

        // Handle object selection if no gizmo is clicked or the selection failed
        if (selectedObject == -1 || !isDragging) {
            selectedObject = -1;
            for (size_t i = 0; i < objects.size(); ++i) {
                if (RayIntersectsObject(ray_origin, ray_direction, objects[i])) {
                    selectedObject = static_cast<int>(i);
                    Log("Object " + std::to_string(i) + " selected at position " + glm::to_string(objects[i].position));
                    break;
                }
            }
            if (selectedObject == -1) {
                Log("No object selected");
            }
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        isDragging = false;
        selectedAxis = -1;
        Log("Stopped dragging");
    }
}

// Scroll callback for zooming
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// Handle keyboard input
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// Convert screen coordinates to a world ray
glm::vec3 ScreenToWorldRay(float mouseX, float mouseY, const glm::mat4& view, const glm::mat4& projection) {
    float x = (2.0f * mouseX) / SCR_WIDTH - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / SCR_HEIGHT;
    glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);

    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    glm::vec3 ray_wor = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    return ray_wor;
}

// Improved collision detection for object selection
bool RayIntersectsObject(const glm::vec3& ray_origin, const glm::vec3& ray_direction, const Object& object) {
    // Bounding sphere test
    float radius = 0.5f * glm::length(object.scale); // Use object's scale as radius
    glm::vec3 oc = ray_origin - object.position;
    float a = glm::dot(ray_direction, ray_direction);
    float b = 2.0f * glm::dot(oc, ray_direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant > 0.0f) {
        return true; // The ray intersects the bounding sphere
    }
    return false;
}

void RenderObjectWithOutline(Shader& shader, const Object& obj) {
    shader.use();

    // Set up projection and view matrices
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    // Enable stencil testing
    glEnable(GL_STENCIL_TEST);

    // First pass: Render the original object normally to set the stencil buffer
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilMask(0xFF); // Write to the stencil buffer

    // Render the original object
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::scale(model, obj.scale);
    shader.setMat4("model", model);
    shader.setVec4("color", obj.color); // Use the object's original color

    if (obj.isCube) {
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Second pass: Render the outline where the stencil buffer is not set
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilMask(0x00); // Disable writing to the stencil buffer
    glDisable(GL_DEPTH_TEST); // Disable depth testing so the outline appears on top

    // Render the scaled-up object in yellow as the outline
    model = glm::mat4(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::scale(model, obj.scale * 1.05f); // Slightly larger scale for the outline
    shader.setMat4("model", model);
    shader.setVec4("color", glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow color for the outline

    if (obj.isCube) {
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Reset stencil settings
    glEnable(GL_DEPTH_TEST);
    glStencilMask(0xFF);
    glDisable(GL_STENCIL_TEST);
}

void RenderScene(Shader& shader) {
    shader.use();
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    for (const auto& obj : objects) {
        if (obj.textureID != 0) {
            // Use the texture
            glBindTexture(GL_TEXTURE_2D, obj.textureID);
            shader.setBool("useTexture", true);
        }
        else {
            // No texture available; use a default color
            shader.setBool("useTexture", false);
            shader.setVec4("color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Light gray color
        }

        // Set up the model matrix
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, obj.position);
        model = glm::scale(model, obj.scale);
        shader.setMat4("model", model);

        // Render the object
        if (obj.isCube) {
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        else {
            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, sphereVertexCount, GL_UNSIGNED_INT, 0);
        }
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture after rendering
}

// Render ImGui settings for creating and manipulating objects
void RenderImGui(Shader ourShader) {
    // Setup Docking
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground; // Make the background transparent

    // Create a full-screen window to host the dockspace
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode); // Allow passing mouse events to 3D view
    ImGui::End();

    // Transformation Mode window
    ImGui::Begin("Transformation Mode");

    // Define a fixed square size for the buttons
    ImVec2 buttonSize(50.0f, 50.0f); // Square buttons of size 50x50

    // Text buttons for mode selection in the same row with fixed size
    if (ImGui::Button("Move", buttonSize)) {
        currentMode = TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotate", buttonSize)) {
        currentMode = ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Scale", buttonSize)) {
        currentMode = SCALE;
    }

    ImGui::End();

    // Object List window
    ImGui::Begin("Object List");

    // Add buttons for adding a cube or sphere above the object list
    if (ImGui::Button("Add Cube")) {
        // Add a cube to the scene at (0, 0.5, 0)
        objects.push_back({ glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.0f), glm::vec4(1.0f), true });
        Log("Added a new cube at position (0, 0.5, 0)");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Sphere")) {
        // Add a sphere to the scene at (0, 0.5, 0)
        objects.push_back({ glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.0f), glm::vec4(1.0f), false });
        Log("Added a new sphere at position (0, 0.5, 0)");
    }

    // List all objects
    for (size_t i = 0; i < objects.size(); ++i) {
        std::string objName = "Object " + std::to_string(i);
        if (ImGui::Selectable(objName.c_str(), selectedObject == static_cast<int>(i))) {
            selectedObject = static_cast<int>(i); // Select the object
            Log("Selected object " + objName);
        }
    }

    ImGui::End();

    // Render the console and object settings
    RenderImGuiConsole();
    RenderObjectSettings();
}

// Render the ImGui console to display debug messages
void RenderImGuiConsole() {
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Console");

    for (const auto& message : debugMessages) {
        ImGui::TextUnformatted(message.c_str());
    }

    ImGui::End();
}

// Log function to store messages in both std::cout and ImGui console
void Log(const std::string& message) {
    std::cout << message << std::endl;  // Standard console output
    debugMessages.push_back(message);   // Also add to in-app log
}

void SetupGrid(float size, float step) {
    std::vector<float> vertices;

    // Generate the grid lines along the X and Z axes
    for (float i = -size; i <= size; i += step) {
        // Line parallel to the Z-axis
        vertices.push_back(i);
        vertices.push_back(0.0f); // Y is always 0 for the XZ plane
        vertices.push_back(-size);

        vertices.push_back(i);
        vertices.push_back(0.0f); // Y is always 0 for the XZ plane
        vertices.push_back(size);

        // Line parallel to the X-axis
        vertices.push_back(-size);
        vertices.push_back(0.0f); // Y is always 0 for the XZ plane
        vertices.push_back(i);

        vertices.push_back(size);
        vertices.push_back(0.0f); // Y is always 0 for the XZ plane
        vertices.push_back(i);
    }

    // Update gridVertexCount based on the number of vertices generated
    gridVertexCount = vertices.size() / 3; // Each vertex has 3 components (x, y, z)

    // Create VAO and VBO for the grid
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Check for errors after setting up the grid
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error in SetupGrid: " << err << std::endl;
    }
}

void DrawGrid(Shader& shader, const glm::mat4& projection, const glm::mat4& view) {
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    // Use an identity matrix for the grid's model matrix to keep it fixed
    glm::mat4 model = glm::mat4(1.0f); // Identity matrix
    shader.setMat4("model", model);

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount); // Use the updated gridVertexCount
    glBindVertexArray(0);
}

void RenderGizmo(Shader& gizmoShader, const Object& obj) {
    gizmoShader.use();

    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    gizmoShader.setMat4("projection", projection);
    gizmoShader.setMat4("view", view);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::scale(model, obj.scale);
    gizmoShader.setMat4("model", model);

    // Set line width to make the gizmo wider
    glLineWidth(3.0f); // Increase this value for thicker lines

    // Draw the X axis (red)
    glm::vec4 xColor = (selectedAxis == 0 && isDragging) ? glm::vec4(1.0f, 0.5f, 0.5f, 1.0f) : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    gizmoShader.setVec4("color", xColor);
    DrawArrow(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Draw the Y axis (green)
    glm::vec4 yColor = (selectedAxis == 1 && isDragging) ? glm::vec4(0.5f, 1.0f, 0.5f, 1.0f) : glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    gizmoShader.setVec4("color", yColor);
    DrawArrow(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Draw the Z axis (blue)
    glm::vec4 zColor = (selectedAxis == 2 && isDragging) ? glm::vec4(0.5f, 0.5f, 1.0f, 1.0f) : glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    gizmoShader.setVec4("color", zColor);
    DrawArrow(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // Restore line width to default (optional)
    glLineWidth(1.0f);
}

void DrawArrow(const glm::vec3& start, const glm::vec3& end) {
    float arrowVertices[] = {
        start.x, start.y, start.z,
        end.x, end.y, end.z
    };

    unsigned int arrowVAO, arrowVBO;
    glGenVertexArrays(1, &arrowVAO);
    glGenBuffers(1, &arrowVBO);
    glBindVertexArray(arrowVAO);

    glBindBuffer(GL_ARRAY_BUFFER, arrowVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(arrowVertices), arrowVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(arrowVAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);

    glDeleteVertexArrays(1, &arrowVAO);
    glDeleteBuffers(1, &arrowVBO);
}

// Function to calculate the shortest distance between a ray and a line segment
float DistanceFromRayToLineSegment(const glm::vec3& ray_origin, const glm::vec3& ray_direction, const glm::vec3& line_start, const glm::vec3& line_end) {
    glm::vec3 line_dir = line_end - line_start;
    float line_length = glm::length(line_dir);
    line_dir = glm::normalize(line_dir);

    // Calculate the projection of the ray on the line segment
    glm::vec3 v = ray_origin - line_start;
    glm::vec3 projection = line_start + glm::dot(v, line_dir) * line_dir;

    // Clamp the projection to the line segment
    float t = glm::dot(projection - line_start, line_dir) / line_length;
    t = glm::clamp(t, 0.0f, 1.0f);

    // Closest point on the line segment
    glm::vec3 closest_point = line_start + t * line_dir * line_length;

    // Distance from the ray to the closest point on the line segment
    glm::vec3 perpendicular_dist = glm::cross(ray_direction, closest_point - ray_origin);
    float distance = glm::length(perpendicular_dist) / glm::length(ray_direction);

    return distance;
}

void DrawCircle(const glm::vec3& objectPosition, const glm::vec3& axis, float radius, Shader& shader) {
    const int segments = 64; // Number of segments to approximate the circle
    std::vector<glm::vec3> points;

    // Generate points for the circle in the XY plane
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * glm::pi<float>() * float(i) / float(segments);
        glm::vec3 point = glm::vec3(radius * cos(theta), radius * sin(theta), 0.0f);

        // Rotate the point to align with the specified axis
        glm::vec3 rotationAxis = glm::normalize(axis);
        glm::vec3 defaultAxis = glm::vec3(0.0f, 0.0f, 1.0f);

        if (glm::dot(rotationAxis, defaultAxis) < 0.999f) { // Avoid issues when the axis is already aligned
            glm::vec3 rotationCross = glm::cross(defaultAxis, rotationAxis);
            float rotationAngle = glm::acos(glm::dot(defaultAxis, rotationAxis));
            glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), rotationAngle, rotationCross);
            glm::vec4 rotatedPoint = rotationMatrix * glm::vec4(point, 1.0f);
            point = glm::vec3(rotatedPoint);
        }

        // Translate the rotated point to the object's position
        glm::vec3 finalPosition = point + objectPosition;

        points.push_back(finalPosition);
    }

    // Create a VAO and VBO for the circle
    unsigned int circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), &points[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Draw the circle using GL_LINE_LOOP to connect the points
    glBindVertexArray(circleVAO);
    glDrawArrays(GL_LINE_LOOP, 0, segments);
    glBindVertexArray(0);

    // Clean up the VBO and VAO
    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
}

// Render the translation gizmo
void RenderTranslationGizmo(Shader& shader, const Object& obj) {
    RenderGizmo(shader, obj); // Use your existing function
}

// Render the rotation gizmo
void RenderRotationGizmo(const Object& obj, Shader& gizmoShader) {
    gizmoShader.use();

    // Set up the projection and view matrices
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    gizmoShader.setMat4("projection", projection);
    gizmoShader.setMat4("view", view);

    // The object's position is the center of the rotation gizmo
    float radius = 1.5f; // Adjust the radius as necessary for better visual fit

    // Draw circles for X, Y, and Z axes at the object's position
    DrawCircle(obj.position, glm::vec3(1.0f, 0.0f, 0.0f), radius, gizmoShader); // X-axis
    DrawCircle(obj.position, glm::vec3(0.0f, 1.0f, 0.0f), radius, gizmoShader); // Y-axis
    DrawCircle(obj.position, glm::vec3(0.0f, 0.0f, 1.0f), radius, gizmoShader); // Z-axis
}

// Render the scaling gizmo
void RenderScalingGizmo(Shader& shader, const Object& obj) {
    RenderGizmo(shader, obj); // Render the main gizmo lines first

    // Set up the model matrix for rendering the small cubes
    glm::mat4 model;
    float cubeSize = 0.1f; // Size of the small cubes at the end of the gizmos

    // Define positions for the small cubes at the end of the gizmos
    glm::vec3 xEnd = obj.position + glm::vec3(1.0f * obj.scale.x, 0.0f, 0.0f);
    glm::vec3 yEnd = obj.position + glm::vec3(0.0f, 1.0f * obj.scale.y, 0.0f);
    glm::vec3 zEnd = obj.position + glm::vec3(0.0f, 0.0f, 1.0f * obj.scale.z);

    // Draw the small cube for the X-axis
    shader.setVec4("color", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color
    model = glm::translate(glm::mat4(1.0f), xEnd);
    model = glm::scale(model, glm::vec3(cubeSize));
    shader.setMat4("model", model);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Draw the small cube for the Y-axis
    shader.setVec4("color", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green color
    model = glm::translate(glm::mat4(1.0f), yEnd);
    model = glm::scale(model, glm::vec3(cubeSize));
    shader.setMat4("model", model);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Draw the small cube for the Z-axis
    shader.setVec4("color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)); // Blue color
    model = glm::translate(glm::mat4(1.0f), zEnd);
    model = glm::scale(model, glm::vec3(cubeSize));
    shader.setMat4("model", model);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindVertexArray(0); // Unbind the VAO
}

void HandleGizmoInteraction(GLFWwindow* window) {
    if (selectedObject < 0) return; // No object selected

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec3 ray_origin = camera.Position;
    glm::vec3 ray_direction = ScreenToWorldRay(static_cast<float>(xpos), static_cast<float>(ypos), camera.GetViewMatrix(), glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f));

    switch (currentMode) {
    case TRANSLATE:
        // Existing translation logic
        break;
    case ROTATE:
        // Check if the ray is near the rotation circles
        if (IsRayNearCircle(ray_origin, ray_direction, objects[selectedObject].position, glm::vec3(1, 0, 0), 1.0f)) {
            selectedAxis = 0; // X-axis rotation
            isDragging = true;
            Log("Selected X-axis rotation");
        }
        else if (IsRayNearCircle(ray_origin, ray_direction, objects[selectedObject].position, glm::vec3(0, 1, 0), 1.0f)) {
            selectedAxis = 1; // Y-axis rotation
            isDragging = true;
            Log("Selected Y-axis rotation");
        }
        else if (IsRayNearCircle(ray_origin, ray_direction, objects[selectedObject].position, glm::vec3(0, 0, 1), 1.0f)) {
            selectedAxis = 2; // Z-axis rotation
            isDragging = true;
            Log("Selected Z-axis rotation");
        }
        break;
    case SCALE:
        // Existing scaling logic
        break;
    }
}

bool IsRayNearCircle(const glm::vec3& ray_origin, const glm::vec3& ray_direction, const glm::vec3& center, const glm::vec3& normal, float radius) {
    // Find the point where the ray intersects the plane of the circle
    float t = glm::dot(center - ray_origin, normal) / glm::dot(ray_direction, normal);
    if (t < 0) return false; // Intersection behind the ray origin

    glm::vec3 intersection = ray_origin + t * ray_direction;

    // Check if the intersection point is near the circle's radius
    float distanceToCenter = glm::length(intersection - center);
    const float threshold = 0.1f; // Tolerance for selecting the circle
    return glm::abs(distanceToCenter - radius) < threshold;
}

void RenderObjectSettings() {
    if (settingsDisplayed) return;

    ImGui::Begin("Object Settings");
    if (selectedObject < 0 || selectedObject >= objects.size()) {
        ImGui::Text("No object selected");
    }
    else {
        // Get a reference to the selected object
        Object& obj = objects[selectedObject];

        // Display editable fields for translation, scaling, and rotation
        ImGui::Text("Translation");
        ImGui::DragFloat3("Position (X, Y, Z)", glm::value_ptr(obj.position), 0.1f);
        ImGui::Text("\n");

        ImGui::Text("Scaling");
        ImGui::DragFloat3("Scale (X, Y, Z)", glm::value_ptr(obj.scale), 0.1f, 0.1f, 100.0f);
        ImGui::Text("\n");

        static glm::vec3 rotationEuler(0.0f); // Store rotation in degrees
        ImGui::Text("Rotation (Degrees)");
        ImGui::DragFloat3("Rotation (X, Y, Z)", glm::value_ptr(rotationEuler), 0.1f);
        ImGui::Text("\n");

        // Add some space and a label for texture settings
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("\n");
        ImGui::Text("Texture Settings");

        // Texture selection
        static char texturePath[128] = "Source/textures/texture1.jpg";
        if (ImGui::Button("Browse")) {
            const char* filePath = tinyfd_openFileDialog("Select Texture", "", 0, NULL, NULL, 0);
            if (filePath) {
                strncpy(texturePath, filePath, sizeof(texturePath));
                texturePath[sizeof(texturePath) - 1] = '\0'; // Ensure null termination
            }
        }
        ImGui::SameLine(); // Keep the "Browse" button on the same line
        ImGui::InputText(" ", texturePath, IM_ARRAYSIZE(texturePath));
        
        

        if (ImGui::Button("Load Texture")) {
            obj.textureID = LoadTexture(texturePath);
            Log("Loaded texture: " + std::string(texturePath));
        }
    }

    ImGui::End();
    settingsDisplayed = true;
}

void SetupFrameBuffer() {
    // Generate framebuffer
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create a texture for color attachment
    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    // Create a renderbuffer for depth and stencil attachment
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // Check if the framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

unsigned int LoadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture wrapping/filtering options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load texture
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);

    return textureID;
}

void SetupSphere() {
    const int latitudeBands = 30;
    const int longitudeBands = 30;
    const float radius = 0.5f;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int lat = 0; lat <= latitudeBands; ++lat) {
        float theta = lat * glm::pi<float>() / latitudeBands;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int lon = 0; lon <= longitudeBands; ++lon) {
            float phi = lon * 2 * glm::pi<float>() / longitudeBands;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            float u = 1.0f - (float)lon / longitudeBands;
            float v = 1.0f - (float)lat / latitudeBands;

            vertices.push_back(radius * x);
            vertices.push_back(radius * y);
            vertices.push_back(radius * z);
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    for (int lat = 0; lat < latitudeBands; ++lat) {
        for (int lon = 0; lon < longitudeBands; ++lon) {
            int first = (lat * (longitudeBands + 1)) + lon;
            int second = first + longitudeBands + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    sphereVertexCount = indices.size();

    unsigned int VBO, EBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(sphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void DrawSmallCube(Shader& shader, const glm::vec3& position, const glm::vec4& color) {
    shader.setVec4("color", color);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, glm::vec3(0.1f)); // Scale down for a small cube
    shader.setMat4("model", model);

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}