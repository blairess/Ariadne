#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "core/terrain/terrain.h"
#include "core/camera/camera.h"
#include "core/render/render-modes.h"
#include "tools/raise_lower_brush/raise_lower_brush.h"
#include "tools/smooth_brush/smooth_brush.h"
#include "core/history/history_manager.h"
#include "ui/navigation/top_bar.h"
#include "ui/navigation/tool_bar.h"

// Global Camera Settings
Camera camera(glm::vec3(0.0f, 2.0f, 5.0f));
float lastX = 800.0f / 2.0f;
float lastY = 600.0f / 2.0f;
bool firstMouse = true;

// Active Tool Selection Enum
enum class ToolType {
    RaiseLower,
    Smooth
};

ToolType currentToolType = ToolType::RaiseLower;

// Render Mode, Brush Instances & History Manager
TerrainRenderMode renderMode;
Core::Tools::RaiseLowerBrush raiseLowerBrush;
Core::Tools::SmoothBrush smoothBrush;
Core::Tools::Brush* activeBrush = &raiseLowerBrush; // Active polymorphic brush pointer

HistoryManager historyManager;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Window Dimensions (Windowed mode defaults)
int windowWidth = 800;
int windowHeight = 600;

// Maximized Window State Tracking
bool isMaximized = false;

// Brush visual circle variables
bool brushHit = false;
glm::vec3 brushHitPoint(0.0f);

// UI Instance
UI::TopBar topBar;
UI::ToolBar toolBar;

// Callbacks
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window, Terrain& terrain);
void toggleMaximize(GLFWwindow* window);

// Raycast helper to project mouse cursor onto ground plane
bool raycastToTerrainPlane(Camera& cam, float screenX, float screenY, int width, int height, glm::vec3& outHitPoint) {
    float x = (2.0f * screenX) / width - 1.0f;
    float y = 1.0f - (2.0f * screenY) / height;

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 projection = glm::perspective(glm::radians(cam.Zoom), aspectRatio, 0.1f, 100.0f);
    glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 camPos = cam.CameraPos;

    glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(cam.GetViewMatrix()) * rayEye));
    glm::vec3 rayOrigin = camPos;

    if (std::abs(rayDir.y) < 0.0001f) return false;

    float t = -rayOrigin.y / rayDir.y;
    if (t < 0.0f) return false;

    outHitPoint = rayOrigin + rayDir * t;
    return true;
}

// Bilinear interpolation helper to get terrain height at any (x, z) coordinate
float getTerrainHeight(const Terrain& terrain, float x, float z) {
    float max_x = (terrain.width - 1) * terrain.cellSize;
    float max_z = (terrain.depth - 1) * terrain.cellSize;

    // Clamp to terrain boundaries
    if (x < 0.0f) x = 0.0f;
    if (x > max_x) x = max_x;
    if (z < 0.0f) z = 0.0f;
    if (z > max_z) z = max_z;

    int gx = static_cast<int>(x / terrain.cellSize);
    int gz = static_cast<int>(z / terrain.cellSize);

    // Clamp cell coordinates to valid interpolation range
    if (gx >= terrain.width - 1) gx = terrain.width - 2;
    if (gz >= terrain.depth - 1) gz = terrain.depth - 2;
    if (gx < 0) gx = 0;
    if (gz < 0) gz = 0;

    float local_x = (x / terrain.cellSize) - gx;
    float local_z = (z / terrain.cellSize) - gz;

    int idx00 = (gz * terrain.width + gx) * 3 + 1;
    int idx10 = (gz * terrain.width + (gx + 1)) * 3 + 1;
    int idx01 = ((gz + 1) * terrain.width + gx) * 3 + 1;
    int idx11 = ((gz + 1) * terrain.width + (gx + 1)) * 3 + 1;

    float h00 = terrain.vertices[idx00];
    float h10 = terrain.vertices[idx10];
    float h01 = terrain.vertices[idx01];
    float h11 = terrain.vertices[idx11];

    float h_top = h00 * (1.0f - local_x) + h10 * local_x;
    float h_bottom = h01 * (1.0f - local_x) + h11 * local_x;
    return h_top * (1.0f - local_z) + h_bottom * local_z;
}

// Main Function
int main()
{
    // GLFW Initialize
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create Window in normal (windowed) mode
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Ariadnis", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    lastX = windowWidth / 2.0f;
    lastY = windowHeight / 2.0f;

    // Register Callbacks & Lock Mouse to Window
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // GLAD Initialize
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Enable Depth Test
    glEnable(GL_DEPTH_TEST);

    // Viewport Initial Setup
    glViewport(0, 0, windowWidth, windowHeight);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Scale UI text dynamically
    io.FontGlobalScale = 1.25f;

    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding = ImVec2(12.0f, 10.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);

    ImGui::StyleColorsDark();

    // Setup ImGui Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Vertex Shader using View and Projection Matrices
    const char* vertexShaderSource = "#version 330 core \n"
        "layout (location = 0) in vec3 aPos; \n"
        "uniform mat4 view; \n"
        "uniform mat4 projection; \n"
        "void main() \n"
        "{ \n"
        " gl_Position = projection * view * vec4(aPos, 1.0); \n"
        "} \0";

    // Create shader object and shader source code
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR: SHADER / VERTEX / COMPILATION_FAILED \n" << infoLog << std::endl;
    }

    // Fragment Shader
    const char* fragmentShaderSource = "#version 330 core \n"
        "out vec4 FragColor; \n"
        "uniform vec4 color; \n"
        "void main() \n"
        "{ \n"
        " FragColor = color; \n"
        "} \0";

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR: SHADER / FRAGMENT / COMPILATION_FAILED \n" << infoLog << std::endl;
    }

    // Link Shaders
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR: SHADER / PROGRAM / LINKING_FAILED \n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int colorLoc = glGetUniformLocation(shaderProgram, "color");
    int viewLoc = glGetUniformLocation(shaderProgram, "view");
    int projLoc = glGetUniformLocation(shaderProgram, "projection");

    // Instantiate and setup Terrain (Width, Depth, CellSize)
    Terrain myTerrain(64, 64, 0.1f);
    myTerrain.generateMesh();
    myTerrain.setupBuffers();

    // Setup Brush Circle buffers for 2 circles (Outer = Radius, Inner = Noggit Falloff Indicator)
    unsigned int circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, 130 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        // Delta time calculation
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Poll events and process scene input
        glfwPollEvents();
        processInput(window, myTerrain);

        // Clear framebuffers
        glClearColor(0.10f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Dynamic aspect ratio calculation
        float aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight > 0 ? windowHeight : 1);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), aspectRatio, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);

        // Draw Terrain using active Render Mode
        renderMode.render(myTerrain, colorLoc);

        // Draw Noggit-style Dual Brush Circles (Outer: Radius, Inner: Strength Falloff)
        bool isRightMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
        if (brushHit && !isRightMouseDown && !ImGui::GetIO().WantCaptureMouse && activeBrush != nullptr)
        {
            std::vector<float> circleVertices;
            int segments = 64;

            // Strength Mapping Limits: [0.01f, 3.0f]
            float minStrength = 0.01f;
            float maxStrength = 3.0f;

            // Normalize active strength into a smooth percentage ratio
            float strengthNormalized = (activeBrush->strength - minStrength) / (maxStrength - minStrength);
            strengthNormalized = glm::clamp(strengthNormalized, 0.01f, 0.98f);

            float outerRadius = activeBrush->radius;
            float innerRadius = outerRadius * strengthNormalized;

            // 1. Outer Radius Circle (Cyan)
            for (int i = 0; i <= segments; ++i)
            {
                float angle = 2.0f * 3.1415926535f * i / segments;
                float cx = brushHitPoint.x + outerRadius * std::cos(angle);
                float cz = brushHitPoint.z + outerRadius * std::sin(angle);
                float cy = getTerrainHeight(myTerrain, cx, cz) + 0.005f;
                circleVertices.push_back(cx);
                circleVertices.push_back(cy);
                circleVertices.push_back(cz);
            }

            // 2. Inner Strength Circle (Noggit Red / Orange)
            for (int i = 0; i <= segments; ++i)
            {
                float angle = 2.0f * 3.1415926535f * i / segments;
                float cx = brushHitPoint.x + innerRadius * std::cos(angle);
                float cz = brushHitPoint.z + innerRadius * std::sin(angle);
                float cy = getTerrainHeight(myTerrain, cx, cz) + 0.005f;
                circleVertices.push_back(cx);
                circleVertices.push_back(cy);
                circleVertices.push_back(cz);
            }

            // Upload circle data to OpenGL dynamic VBO
            glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, circleVertices.size() * sizeof(float), circleVertices.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glUseProgram(shaderProgram);
            glLineWidth(2.0f);
            glBindVertexArray(circleVAO);

            // Draw Outer Ring (Cyan)
            glUniform4f(colorLoc, 0.0f, 0.8f, 1.0f, 1.0f);
            glDrawArrays(GL_LINE_STRIP, 0, segments + 1);

            // Draw Inner Falloff Ring (Noggit Red / Orange)
            glUniform4f(colorLoc, 1.0f, 0.35f, 0.1f, 1.0f);
            glDrawArrays(GL_LINE_STRIP, segments + 1, segments + 1);

            glBindVertexArray(0);
            glLineWidth(1.0f);
        }

        // ImGui frame rendering
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        topBar.render(window, historyManager, myTerrain);
        toolBar.render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Clean up ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Clean up OpenGL objects
    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

// Window Maximize toggle
void toggleMaximize(GLFWwindow* window)
{
    isMaximized = !isMaximized;
    if (isMaximized)
        glfwMaximizeWindow(window);
    else
        glfwRestoreWindow(window);
}

// Input Handling
void processInput(GLFWwindow* window, Terrain& terrain)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::DOWN, deltaTime);

    renderMode.handleInput(window);

    // Toggle Window Maximize with 'P'
    static bool pKeyPressedLastFrame = false;
    bool pKeyDown = (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS);
    if (pKeyDown && !pKeyPressedLastFrame)
    {
        toggleMaximize(window);
    }
    pKeyPressedLastFrame = pKeyDown;

    // Tool shortcuts
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        currentToolType = ToolType::RaiseLower;
        activeBrush = &raiseLowerBrush;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        currentToolType = ToolType::Smooth;
        activeBrush = &smoothBrush;
    }

    // Invert raise/lower direction with Shift
    raiseLowerBrush.isLowering = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

    if (ImGui::GetIO().WantCaptureMouse)
    {
        brushHit = false;
        return;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    brushHit = raycastToTerrainPlane(camera, static_cast<float>(mouseX), static_cast<float>(mouseY), windowWidth, windowHeight, brushHitPoint);
    if (brushHit)
    {
        float max_x = (terrain.width - 1) * terrain.cellSize;
        float max_z = (terrain.depth - 1) * terrain.cellSize;
        if (brushHitPoint.x < 0.0f || brushHitPoint.x > max_x ||
            brushHitPoint.z < 0.0f || brushHitPoint.z > max_z)
        {
            brushHit = false;
        }
    }

    static bool isSculpting = false;
    static std::vector<float> beforeVertices;

    bool isMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    if (isMouseDown)
    {
        if (!isSculpting)
        {
            isSculpting = true;
            beforeVertices = terrain.getVertices();
        }

        if (brushHit && activeBrush != nullptr)
        {
            activeBrush->apply(terrain, brushHitPoint, deltaTime);
        }
    }
    else
    {
        if (isSculpting)
        {
            isSculpting = false;
            std::vector<float> afterVertices = terrain.getVertices();
            if (beforeVertices != afterVertices)
            {
                auto cmd = std::make_unique<TerrainModifyCommand>(terrain, beforeVertices, afterVertices);
                historyManager.pushCommand(std::move(cmd));
            }
        }
    }

    if (!isSculpting)
    {
        static double undoTimer = 0.0;
        static double redoTimer = 0.0;
        static bool undoHeld = false;
        static bool redoHeld = false;

        const double INITIAL_DELAY = 0.35;
        const double REPEAT_RATE = 0.10;

        double currentTime = glfwGetTime();

        bool ctrlDown = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
            (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

        bool zDown = (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS);
        bool yDown = (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS);

        if (ctrlDown && zDown)
        {
            if (!undoHeld)
            {
                historyManager.undo();
                undoHeld = true;
                undoTimer = currentTime + INITIAL_DELAY;
            }
            else if (currentTime >= undoTimer)
            {
                historyManager.undo();
                undoTimer = currentTime + REPEAT_RATE;
            }
        }
        else
        {
            undoHeld = false;
        }

        if (ctrlDown && yDown)
        {
            if (!redoHeld)
            {
                historyManager.redo();
                redoHeld = true;
                redoTimer = currentTime + INITIAL_DELAY;
            }
            else if (currentTime >= redoTimer)
            {
                historyManager.redo();
                redoTimer = currentTime + REPEAT_RATE;
            }
        }
        else
        {
            redoHeld = false;
        }
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    bool qPressed = (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);
    bool ePressed = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !qPressed && !ePressed)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;

        lastX = xpos;
        lastY = ypos;

        camera.processMouseMovement(xoffset, yoffset);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    bool qPressed = (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);
    bool ePressed = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);

    if (activeBrush != nullptr && (qPressed || ePressed))
    {
        float scrollDelta = static_cast<float>(yoffset);

        // Adjust Outer Radius (Q + Scroll)
        if (qPressed)
        {
            float scaleFactor = 1.0f + (scrollDelta * 0.05f);
            activeBrush->radius *= scaleFactor;
            activeBrush->radius = glm::clamp(activeBrush->radius, 0.05f, 100.0f);
        }

        // Adjust Inner Falloff / Strength (E + Scroll) — Upper Limit 3.0f
        if (ePressed)
        {
            float scaleFactor = 1.0f + (scrollDelta * 0.05f);
            activeBrush->strength *= scaleFactor;
            activeBrush->strength = glm::clamp(activeBrush->strength, 0.01f, 3.0f);
        }

        return; // Prevent camera zoom while holding Q or E
    }

    camera.processMouseScroll(static_cast<float>(yoffset));
}