#include "renderer.h"
//#include "renderer.cpp"
#include "../gfx/shader.h"
#include "../gfx/shader.h"

using namespace SCN;

void Renderer::renderDeferred()
{
    Camera* camera = Camera::current;
    int texture_slots = 0;

    // A quad is usually a mesh of a plane,
    // always aligned with you view
    GFX::Mesh* quad = GFX::Mesh::getQuad();

    GFX::Shader* shader = NULL;
    shader = GFX::Shader::Get("singlepass_deferred");

    assert(glGetError() == GL_NO_ERROR);

    //no shader? then nothing to render
    if (!shader)
        return;

    shader->enable();

    //send lights
    vec3* light_pos = new vec3[light_list.size()];
    vec3* light_color = new vec3[light_list.size()];
    float* light_int = new float[light_list.size()];
    vec3* light_dir = new vec3[light_list.size()];
    int* light_type = new int[light_list.size()];
    vec2* cone_info = new vec2[light_list.size()];

    int i = 0;
    for (LightEntity* light : light_list) {
        light_pos[i] = light->root.getGlobalMatrix().getTranslation();
        light_int[i] = light->intensity;
        light_color[i] = light->color;
        light_dir[i] = light->root.model.frontVector();
        light_type[i] = light->light_type;
        cone_info[i] = light->cone_info;
        i++;
    }
    
    /*shader->setUniform("u_ssao_enabled", ssao_enabled);
    shader->setUniform("u_ssao_to_lighting", ssao_lighting);
    shader->setTexture("u_ssao_texture", ssao_fbo->color_textures[0], texture_slots++);*/
    shader->setUniform("u_numShadows", (int)fmin(light_list.size(), 10));
    shader->setUniform("u_light_count", (int)fmin(light_list.size(), 10));
    shader->setUniform3Array("u_light_pos", (float*)light_pos, fmin(light_list.size(), 10));
    shader->setUniform3Array("u_light_color", (float*)light_color, fmin(light_list.size(), 10));
    shader->setUniform1Array("u_light_intensity", light_int, fmin(light_list.size(), 10));
    shader->setUniform1Array("u_light_type", (int*)light_type, fmin(light_list.size(), 10));
    shader->setUniform3Array("u_light_dir", (float*)light_dir, fmin(light_list.size(), 10));
    shader->setUniform2Array("u_light_cone", (float*)cone_info, fmin(light_list.size(), 10));
    shader->setUniform("u_ambient_light", scene->ambient_light);
    


    delete[] light_pos;
    delete[] light_color;
    delete[] light_int;
    delete[] light_dir;
    delete[] cone_info;
    delete[] light_type;


    //upload uniforms
    shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
    shader->setUniform("u_camera_position", camera->eye);



    // Upload time, for cool shader effects
    float t = CORE::getTime();
    shader->setUniform("u_time", t);

    // Bind the GBuffers
    shader->setTexture("u_gbuffer_color", gbuffer_fbo->color_textures[0], texture_slots++);
    shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], texture_slots++);
    shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, texture_slots++);

    Matrix44 inv_vp = Camera::current->viewprojection_matrix;
    inv_vp.inverse();
    shader->setUniform("u_inverse_viewprojection", inv_vp);
    shader->setUniform("u_res_inv", Vector2f(1.0f / gbuffer_fbo->width, 1.0f / gbuffer_fbo->height));

    if (render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    quad->render(GL_TRIANGLES);

    shader->disable();
}

void Renderer::renderFire(const Vector3f& position, float scale) {
    GFX::Mesh* quad = GFX::Mesh::getQuad();


    GFX::Shader* shader = GFX::Shader::Get("fire");

    if (!shader) return;

    shader->enable();

    // Build model matrix
    Matrix44 model;
    model.setTranslation(position.x, position.y, position.z);
    model.scale(scale, scale, scale);

    #ifdef WIN32
        float t = (float)(getTime());
    #else
        float t = (float)(395500000 - getTime()); //396755008
    #endif

    // Upload uniforms
    shader->setUniform("u_model", model);
    shader->setUniform("u_viewprojection", Camera::current->viewprojection_matrix);
    shader->setUniform("u_time", t);

    // Optional tuning
    shader->setUniform("intensity", 1.0f);
    shader->setUniform("detail_strength", 3.0f);
    shader->setUniform("scroll_speed", 1.2f);
    shader->setUniform("fire_height", 1.0f);
    shader->setUniform("fire_shape", 1.0f);
    shader->setUniform("fire_thickness", 1.0f);
    shader->setUniform("fire_sharpness", 1.0f);
    shader->setUniform("noise_octaves", 1);
    shader->setUniform("noise_lacunarity", 1.0f);
    shader->setUniform("noise_gain", 1.0f);
    shader->setUniform("noise_amplitude", 1.0f);
    shader->setUniform("noise_frequency", 1.0f);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glDisable(GL_CULL_FACE); // Optional

    // Render
    quad->render(GL_TRIANGLES);


    // Restore state
    glDisable(GL_BLEND);
    // glEnable(GL_CULL_FACE);
    shader->disable();
}

void Renderer::renderVolumes(Camera* camera)
{
    GFX::Shader* light_volume_shader = GFX::Shader::Get("volume");
    if (!light_volume_shader)
        return;

    light_volume_shader->enable();

    // Bind GBuffer textures
    light_volume_shader->setTexture("u_gbuffer_color", gbuffer_fbo->color_textures[0], 0);
    light_volume_shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], 1);
    light_volume_shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, 2);

    // Camera and inverse matrices
    light_volume_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
    light_volume_shader->setUniform("u_camera_position", camera->eye);
    Matrix44 inv_view_projection_matrix = camera->inverse_viewprojection_matrix;
    light_volume_shader->setUniform("u_inverse_viewprojection", inv_view_projection_matrix);
    light_volume_shader->setUniform("u_res_inv", vec2(1.0f / gbuffer_fbo->width, 1.0f / gbuffer_fbo->height));

    // Enable additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    // Render each light volume
    for (LightEntity* light : light_list)
    {
        // Skip directional lights as they affect the whole scene
        if (light->light_type == eLightType::DIRECTIONAL)
            continue;

        Matrix44 model;
        Vector3f translation = light->root.getGlobalMatrix().getTranslation();
        model.setTranslation(translation.x, translation.y, translation.z);
        model.scale(light->max_distance, light->max_distance, light->max_distance);

        light_volume_shader->setUniform("u_model", model);
        light_volume_shader->setUniform("u_light_pos", model.getTranslation());
        light_volume_shader->setUniform("u_light_color", light->color);
        light_volume_shader->setUniform("u_light_intensity", light->intensity);
        light_volume_shader->setUniform("u_light_type", (int)light->light_type);

        if (light->light_type == eLightType::SPOT)
        {
            light_volume_shader->setUniform("u_light_dir", light->root.model.frontVector());
            light_volume_shader->setUniform("u_light_cone", light->cone_info);
        }

        // Render the sphere
        sphere.render(GL_TRIANGLES);
    }

    // Restore state
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    light_volume_shader->disable();
}
