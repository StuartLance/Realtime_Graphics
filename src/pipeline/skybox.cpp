#include "renderer.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"

using namespace SCN;

void Renderer::renderSkybox(GFX::Texture* cubemap)
{
    Camera* camera = Camera::current;

    // Apply skybox necesarry config:
    // No blending, no dpeth test, we are always rendering the skybox
    // Set the culling aproppiately, since we just want the back faces
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if (render_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    GFX::Shader* shader = GFX::Shader::Get("skybox");
    if (!shader)
        return;
    shader->enable();

    // Center the skybox at the camera, with a big sphere
    Matrix44 m;
    m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
    m.scale(10, 10, 10);
    shader->setUniform("u_model", m);


    
    // Upload camera uniforms
    shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
    shader->setUniform("u_camera_position", camera->eye);

    shader->setUniform("u_texture", cubemap, 0);

    sphere.render(GL_TRIANGLES);

    shader->disable();

    // Return opengl state to default
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_DEPTH_TEST);
}
