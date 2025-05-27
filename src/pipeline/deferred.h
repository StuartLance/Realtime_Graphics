#pragma once

#ifndef DEFERRED_H
#define DEFERRED_H

#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "texture.h"

namespace GFX {
    class Mesh; // Forward declaration
}

namespace SCN {
    class Material; // Forward declaration
}

class Camera; // Forward declaration
class Texture; // Forward declaration
class Shader; // Forward declaration

class DeferredRenderer {
public:
    DeferredRenderer();
    ~DeferredRenderer();

    void initialize(int width, int height);
    void render(GFX::Mesh* mesh, SCN::Material* material, Camera* camera);
    void resize(int width, int height);

private:
    Texture* gBuffer; // Geometry buffer
    Shader* deferredShader; // Shader for deferred rendering
    int screenWidth;
    int screenHeight;

    void createGBuffer();
    void renderToGBuffer(GFX::Mesh* mesh, SCN::Material* material, Camera* camera);
    void renderLightingPass(Camera* camera);
};

#endif // DEFERRED_H
