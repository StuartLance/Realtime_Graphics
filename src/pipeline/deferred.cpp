#include "deferred.h"
#include "fbo.h"

DeferredRenderer::DeferredRenderer() : gBuffer(nullptr), deferredShader(nullptr), screenWidth(0), screenHeight(0) {}

DeferredRenderer::~DeferredRenderer() {
    if (gBuffer) delete gBuffer;
    if (deferredShader) delete deferredShader;
}

void DeferredRenderer::initialize(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    // Load the deferred shader
    deferredShader = new Shader("deferred.vs", "deferred.fs");

    // Create the geometry buffer
    createGBuffer();
}

void DeferredRenderer::createGBuffer() {
    if (gBuffer) delete gBuffer;

    gBuffer = new Texture();
    gBuffer->createFBO(screenWidth, screenHeight);
    gBuffer->addColorAttachment(GL_RGBA16F, GL_FLOAT); // Position
    gBuffer->addColorAttachment(GL_RGBA16F, GL_FLOAT); // Normal
    gBuffer->addColorAttachment(GL_RGBA, GL_UNSIGNED_BYTE); // Albedo
    gBuffer->addDepthAttachment();
    gBuffer->checkFBO();
}

void DeferredRenderer::resize(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    createGBuffer();
}

void DeferredRenderer::render(GFX::Mesh* mesh, SCN::Material* material, Camera* camera) {
    // Render to GBuffer
    renderToGBuffer(mesh, material, camera);

    // Perform lighting pass
    renderLightingPass(camera);
}

void DeferredRenderer::renderToGBuffer(GFX::Mesh* mesh, SCN::Material* material, Camera* camera) {
    gBuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Shader* shader = material->shader;
    shader->enable();
    shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
    shader->setUniform("u_model", mesh->model);
    shader->setUniform("u_texture", material->texture);

    mesh->render(GL_TRIANGLES);
    shader->disable();

    gBuffer->unbind();
}

void DeferredRenderer::renderLightingPass(Camera* camera) {
    deferredShader->enable();
    deferredShader->setUniform("u_gPosition", gBuffer->getColorAttachment(0));
    deferredShader->setUniform("u_gNormal", gBuffer->getColorAttachment(1));
    deferredShader->setUniform("u_gAlbedo", gBuffer->getColorAttachment(2));
    deferredShader->setUniform("u_camera_position", camera->eye);

    // Render a fullscreen quad
    Mesh quad;
    quad.createQuad(0, 0, 2, 2, false);
    quad.render(GL_TRIANGLES);

    deferredShader->disable();
}
