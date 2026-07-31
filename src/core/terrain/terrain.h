#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Terrain {
public:
    int width;
    int depth;
    float cellSize;

    std::vector<float> vertices;
    std::vector<int> indices;

    unsigned int VAO, VBO, EBO;

    // Constructors & Destructor
    Terrain();
    Terrain(int width, int depth, float cellSize);
    ~Terrain();

    // Member Functions
    void generateMesh();
    void setupBuffers();
    void updateBuffers();
    void draw();
    void replaceMesh(int newWidth, int newDepth, float newCellSize,
        const float* newVertices, size_t vertexCount,
        const int* newIndices, size_t indexCount);

    std::vector<float>& getVertices() { return vertices; }
};
