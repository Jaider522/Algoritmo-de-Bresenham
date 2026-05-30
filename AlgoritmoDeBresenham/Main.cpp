/*
    Programa de dibujo libre estilo Paint usando OpenGL + Bresenham + ImGui
    Basado en rejilla de celdas coloreables.
*/

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>

// Inclusiones de ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// SHADERS
const char* vertexShaderSource =
"#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec4 aColor;\n"
"out vec4 ourColor;\n"
"void main() {\n"
"   gl_Position = vec4(aPos, 1.0);\n"
"   ourColor = aColor;\n"
"}\0";

const char* fragmentShaderSource =
"#version 330 core\n"
"out vec4 FragColor;\n"
"in vec4 ourColor;\n"
"void main() { FragColor = ourColor; }\0";

struct VertexSquare {
    GLfloat pos[3];
    GLfloat color[4];
};

const int GRID_SIZE = 150;
std::vector<VertexSquare> gridVerts;
std::vector<GLuint>       gridIndices;

unsigned int VAO, VBO, EBO;

// ESTADO DEL DIBUJO LIBRE Y HERRAMIENTAS
enum DrawMode { MODE_FREEHAND, MODE_LINE, MODE_CIRCLE, MODE_TRIANGLE };
DrawMode currentMode = MODE_FREEHAND;

bool isDrawing = false;
int lastGridX = -1;
int lastGridY = -1;

// Variables para clics de Bresenham
int clickCount = 0;
int ptsX[3];
int ptsY[3];

int brushSize = 1; // Grosor del trazo

// PALETA DE COLORES
GLfloat baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat drawColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
GLfloat eraserColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

// FUNCIONES DE LA REJILLA
void setupGrid() {
    gridVerts.clear();
    gridIndices.clear();

    float cellW = 2.0f / GRID_SIZE;
    float cellH = 2.0f / GRID_SIZE;

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float xPos = -1.0f + (x * cellW);
            float yPos = -1.0f + (y * cellH);

            gridVerts.push_back({ {xPos,         yPos,         0.0f}, {baseColor[0], baseColor[1], baseColor[2], baseColor[3]} });
            gridVerts.push_back({ {xPos + cellW,  yPos,         0.0f}, {baseColor[0], baseColor[1], baseColor[2], baseColor[3]} });
            gridVerts.push_back({ {xPos + cellW,  yPos + cellH, 0.0f}, {baseColor[0], baseColor[1], baseColor[2], baseColor[3]} });
            gridVerts.push_back({ {xPos,          yPos + cellH, 0.0f}, {baseColor[0], baseColor[1], baseColor[2], baseColor[3]} });

            GLuint base = (y * GRID_SIZE + x) * 4;
            gridIndices.push_back(base + 0); gridIndices.push_back(base + 1); gridIndices.push_back(base + 2);
            gridIndices.push_back(base + 0); gridIndices.push_back(base + 2); gridIndices.push_back(base + 3);
        }
    }
}

// Pintar celda con soporte de grosor (pincel cuadrado)
void paintCell(int x, int y, GLfloat color[4], int thickness = 1) {
    for (int dy = -thickness + 1; dy < thickness; dy++) {
        for (int dx = -thickness + 1; dx < thickness; dx++) {
            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                int base = (ny * GRID_SIZE + nx) * 4;
                for (int i = 0; i < 4; i++) {
                    gridVerts[base + i].color[0] = color[0];
                    gridVerts[base + i].color[1] = color[1];
                    gridVerts[base + i].color[2] = color[2];
                    gridVerts[base + i].color[3] = color[3];
                }
            }
        }
    }
}

void clearGrid() {
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            paintCell(x, y, baseColor, 1);
        }
    }
}

void actualizarGPU() {
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, gridVerts.size() * sizeof(VertexSquare), gridVerts.data());
}

// ==========================================================
// ALGORITMOS DE BRESENHAM
// ==========================================================

void bresenhamLinea(int x0, int y0, int x1, int y1, GLfloat color[4], int thickness) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        paintCell(x0, y0, color, thickness);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void bresenhamCirculo(int xc, int yc, int r, GLfloat color[4], int thickness) {
    int x = 0, y = r;
    int d = 3 - 2 * r;

    auto drawSym = [&](int cx, int cy, int px, int py) {
        paintCell(cx + px, cy + py, color, thickness); paintCell(cx - px, cy + py, color, thickness);
        paintCell(cx + px, cy - py, color, thickness); paintCell(cx - px, cy - py, color, thickness);
        paintCell(cx + py, cy + px, color, thickness); paintCell(cx - py, cy + px, color, thickness);
        paintCell(cx + py, cy - px, color, thickness); paintCell(cx - py, cy - px, color, thickness);
        };

    drawSym(xc, yc, x, y);
    while (y >= x) {
        x++;
        if (d > 0) { y--; d = d + 4 * (x - y) + 10; }
        else { d = d + 4 * x + 6; }
        drawSym(xc, yc, x, y);
    }
}

// CONVERSION DE COORDENADAS
void mousePosToGrid(GLFWwindow* window, double xpos, double ypos, int& gx, int& gy) {
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    gx = (int)(xpos / (w / (float)GRID_SIZE));
    gy = (int)((h - ypos) / (h / (float)GRID_SIZE));
}

// CALLBACKS DE ENTRADA
void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return; // No dibujar si el mouse está sobre la GUI

    if (!isDrawing || currentMode != MODE_FREEHAND) return;

    int gx, gy;
    mousePosToGrid(window, xpos, ypos, gx, gy);
    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) return;

    if (lastGridX == -1 && lastGridY == -1) {
        paintCell(gx, gy, drawColor, brushSize);
    }
    else {
        bresenhamLinea(lastGridX, lastGridY, gx, gy, drawColor, brushSize);
    }
    lastGridX = gx; lastGridY = gy;
    actualizarGPU();
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return; // Bloquear clics si tocamos ImGui

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int gx, gy;
    mousePosToGrid(window, xpos, ypos, gx, gy);

    // Si estamos fuera de rejilla, ignorar
    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            if (currentMode == MODE_FREEHAND) {
                isDrawing = true;
                lastGridX = gx; lastGridY = gy;
                paintCell(gx, gy, drawColor, brushSize);
                actualizarGPU();
            }
            else {
                // Registrar clics para las figuras
                ptsX[clickCount] = gx;
                ptsY[clickCount] = gy;
                clickCount++;

                if (currentMode == MODE_LINE && clickCount == 2) {
                    bresenhamLinea(ptsX[0], ptsY[0], ptsX[1], ptsY[1], drawColor, brushSize);
                    clickCount = 0;
                    actualizarGPU();
                }
                else if (currentMode == MODE_CIRCLE && clickCount == 2) {
                    int r = std::round(std::sqrt(std::pow(ptsX[1] - ptsX[0], 2) + std::pow(ptsY[1] - ptsY[0], 2)));
                    bresenhamCirculo(ptsX[0], ptsY[0], r, drawColor, brushSize);
                    clickCount = 0;
                    actualizarGPU();
                }
                else if (currentMode == MODE_TRIANGLE && clickCount == 3) {
                    bresenhamLinea(ptsX[0], ptsY[0], ptsX[1], ptsY[1], drawColor, brushSize);
                    bresenhamLinea(ptsX[1], ptsY[1], ptsX[2], ptsY[2], drawColor, brushSize);
                    bresenhamLinea(ptsX[2], ptsY[2], ptsX[0], ptsY[0], drawColor, brushSize);
                    clickCount = 0;
                    actualizarGPU();
                }
            }
        }
        else if (action == GLFW_RELEASE && currentMode == MODE_FREEHAND) {
            isDrawing = false;
            lastGridX = -1; lastGridY = -1;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        paintCell(gx, gy, eraserColor, brushSize);
        actualizarGPU();
    }
}

// PUNTO DE ENTRADA
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Paint - Bresenham & ImGui", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    gladLoadGL();
    setupGrid();

    // ---- Inicializar ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // ---- Shaders y Buffers ----
    unsigned int vS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vS, 1, &vertexShaderSource, NULL);
    glCompileShader(vS);

    unsigned int fS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fS, 1, &fragmentShaderSource, NULL);
    glCompileShader(fS);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vS);
    glAttachShader(prog, fS);
    glLinkProgram(prog);
    glDeleteShader(vS); glDeleteShader(fS);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * sizeof(VertexSquare), gridVerts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, gridIndices.size() * sizeof(GLuint), gridIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexSquare), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexSquare), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Bucle principal
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 1. Iniciar frame de ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Definir la Interfaz de Usuario
        ImGui::Begin("Panel de Herramientas");

        // Botón Borrador General
        if (ImGui::Button("Borrador (Limpiar Lienzo)", ImVec2(-1, 30))) {
            clearGrid();
            actualizarGPU();
        }
        ImGui::Separator();

        // Selector de Herramientas
        ImGui::Text("Herramientas:");
        if (ImGui::RadioButton("Lápiz Libre", currentMode == MODE_FREEHAND)) { currentMode = MODE_FREEHAND; clickCount = 0; }
        if (ImGui::RadioButton("Línea (2 clics)", currentMode == MODE_LINE)) { currentMode = MODE_LINE; clickCount = 0; }
        if (ImGui::RadioButton("Círculo (Centro -> Radio)", currentMode == MODE_CIRCLE)) { currentMode = MODE_CIRCLE; clickCount = 0; }
        if (ImGui::RadioButton("Triángulo (3 clics)", currentMode == MODE_TRIANGLE)) { currentMode = MODE_TRIANGLE; clickCount = 0; }

        // Mostrar pasos restantes si aplica
        if (currentMode != MODE_FREEHAND && clickCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Esperando clic(s) %d...", clickCount + 1);
        }

        ImGui::Separator();

        // Atributos de Pincel
        ImGui::Text("Atributos:");
        ImGui::SliderInt("Grosor del Trazo", &brushSize, 1, 10);
        ImGui::ColorEdit4("Color de Dibujo", drawColor);

        ImGui::End();

        // 3. Renderizar Gráficos Principales (OpenGL puro)
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)gridIndices.size(), GL_UNSIGNED_INT, 0);

        // 4. Renderizar GUI sobre la pantalla
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Limpieza
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(prog);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}