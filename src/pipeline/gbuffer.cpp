#include "renderer.h"
#include "../gfx/fbo.h"
#include "../core/core.h"
#include "../gfx/shader.h"
#include <iostream>

using namespace SCN;

GFX::FBO* shadow_map_fbo = nullptr; // FBO for shadow mapping
void Renderer::initGBuffer() {
   Vector2ui screen = CORE::getWindowSize();

   // Create the GBuffer FBO
   gbuffer_fbo = new GFX::FBO();

   if (!gbuffer_fbo->create(screen.x, screen.y, 2, GL_RGBA, GL_UNSIGNED_BYTE, true))
   {
       std::cerr << "Error: Failed to create GBuffer FBO." << std::endl;
       return;
   }
   
   //gbuffer_fbo.setTexture(GFX::Texture::Get("gbuffer_diffuse"), 0); Alreaady done

   gbuffer_fbo->color_textures[0]->filename = "Albedo";
   gbuffer_fbo->color_textures[1]->filename = "Normal";
   gbuffer_fbo->depth_texture->filename = "Depth";


   lighting_fbo = new GFX::FBO();




   lighting_fbo->create(screen.x, screen.y, 1, GL_RGBA, GL_UNSIGNED_BYTE, true);
   lighting_fbo->color_textures[0]->filename = "Lighting";
   lighting_fbo->depth_texture->filename = "Depth_Lightning";

   //ssao_fbo = new GFX::FBO();
   //ssao_fbo->create(screen.x, screen.y, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);


   gbuffer_fbo->bind();


   // Check FBO completeness
   GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (status != GL_FRAMEBUFFER_COMPLETE)
   {
       std::cerr << "Error: GBuffer FBO is incomplete. Status: " << status << std::endl;

       switch (status)
       {
       case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
           std::cerr << "Incomplete attachment." << std::endl;
           break;
       case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
           std::cerr << "Missing attachment." << std::endl;
           break;
       case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
           std::cerr << "Incomplete draw buffer." << std::endl;
           break;
       case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
           std::cerr << "Incomplete read buffer." << std::endl;
           break;
       case GL_FRAMEBUFFER_UNSUPPORTED:
           std::cerr << "Unsupported framebuffer format." << std::endl;
           break;
       default:
           std::cerr << "Unknown error." << std::endl;
           break;
       }

       gbuffer_fbo->unbind();
       return;
   }
   
   //gbuffer_fbo.enableAllBuffers();
   gbuffer_fbo->unbind();
   std::cout << "GBuffer FBO successfully created and is complete." << std::endl;

}

void Renderer::GBuffer()
{
    // Bind G-Buffer FBO

    gbuffer_fbo->bind();

    // Clear all buffers
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Get GBuffer fill shader
    GFX::Shader* shader = GFX::Shader::Get("fill");

    shader->enable();

    // Render all opaque objects
    for (const sDrawCommand& command : opaqueObjects)
    {

        // Set model matrix
        shader->setUniform("u_camera_position", Camera::current->eye);
        shader->setUniform("u_model", command.model);
        shader->setUniform("u_viewprojection", Camera::current->viewprojection_matrix);

        // Bind material properties
        command.material->bind(shader);

        // Render mesh
        command.mesh->render(GL_TRIANGLES);
    }

    shader->disable();
    gbuffer_fbo->unbind();
}
