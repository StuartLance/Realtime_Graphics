#include "renderer.h"





bool multipass_on = false; //used to know if we are in the first pass or not


//std::vector<GFX::FBO*> shadow_fbos;




using namespace SCN;

struct compareDrawCommands { // Functor for sorting opaque draw commands by distance
	Camera* camera;

	compareDrawCommands(Camera* cam) : camera(cam) {}

	bool operator()(const sDrawCommand& a, const sDrawCommand& b) const {
		// Only compare distance from camera (assumes both are opaque)
		float distanceA = camera->eye.distance(Vector3f(a.model.m[12], a.model.m[13], a.model.m[14]));
		float distanceB = camera->eye.distance(Vector3f(b.model.m[12], b.model.m[13], b.model.m[14]));
		return distanceA < distanceB; // Draw nearer objects first
	}
};

// GFX::FBO* shadow_map_fbo = nullptr; // FBO for shadow mapping
//void Renderer::initGBuffer() {
//	Vector2ui screen = CORE::getWindowSize();
//
//	// Create the GBuffer FBO
//	gbuffer_fbo = new GFX::FBO();
//
//	if (!gbuffer_fbo->create(screen.x, screen.y, 2, GL_RGBA, GL_UNSIGNED_BYTE, true))
//	{
//		std::cerr << "Error: Failed to create GBuffer FBO." << std::endl;
//		return;
//	}
//	
//	//gbuffer_fbo.setTexture(GFX::Texture::Get("gbuffer_diffuse"), 0); Alreaady done
//
//	gbuffer_fbo->color_textures[0]->filename = "Albedo";
//	gbuffer_fbo->color_textures[1]->filename = "Normal";
//	gbuffer_fbo->depth_texture->filename = "Depth";
//
//
//	lighting_fbo = new GFX::FBO();
//
//
//
//
//	lighting_fbo->create(screen.x, screen.y, 1, GL_RGBA, GL_UNSIGNED_BYTE, true);
//	lighting_fbo->color_textures[0]->filename = "Lighting";
//	lighting_fbo->depth_texture->filename = "Depth_Lightning";
//
//	//ssao_fbo = new GFX::FBO();
//	//ssao_fbo->create(screen.x, screen.y, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);
//
//
//	gbuffer_fbo->bind();
//
//
//	// Check FBO completeness
//	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//	if (status != GL_FRAMEBUFFER_COMPLETE)
//	{
//		std::cerr << "Error: GBuffer FBO is incomplete. Status: " << status << std::endl;
//
//		switch (status)
//		{
//		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
//			std::cerr << "Incomplete attachment." << std::endl;
//			break;
//		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
//			std::cerr << "Missing attachment." << std::endl;
//			break;
//		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
//			std::cerr << "Incomplete draw buffer." << std::endl;
//			break;
//		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
//			std::cerr << "Incomplete read buffer." << std::endl;
//			break;
//		case GL_FRAMEBUFFER_UNSUPPORTED:
//			std::cerr << "Unsupported framebuffer format." << std::endl;
//			break;
//		default:
//			std::cerr << "Unknown error." << std::endl;
//			break;
//		}
//
//		gbuffer_fbo->unbind();
//		return;
//	}
//	
//	//gbuffer_fbo.enableAllBuffers();
//	gbuffer_fbo->unbind();
//	std::cout << "GBuffer FBO successfully created and is complete." << std::endl;
//
//}

std::vector<vec3> generateSpherePoints(int num, float radius, bool hemi) {
	std::vector<vec3> points;
	points.resize(num);

	for (int i = 0; i < num; i++) {
		float u = random();
		float v = random();

		float theta = u * 2.0f * PI;
		float phi = acos(2.0f * v - 1.0f);
		float r = cbrt(random() * 0.9f + 0.1f) * radius;

		vec3 p;
		p.x = r * sin(phi) * cos(theta);
		p.y = r * sin(phi) * sin(theta);
		p.z = r * cos(phi);

		if (hemi && p.z < 0.0f) p.z *= -1.0f;

		points[i] = p;
	}

	return points;
}


//some globals

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	screen = CORE::getWindowSize();

	lab = 2; // Change here or with action
    
	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();

	initGBuffer();
}

//void Renderer::ssao_setup()
//{
//	if (!ssao_shader)
//	{
//		ssao_shader = GFX::Shader::Get("ssao");
//		if (!ssao_shader)
//		{
//			std::cerr << "Error: SSAO shader not found!" << std::endl;
//			return;
//		}
//	}
//
//
//
//	ssao_FBO->create(
//		screen.x,
//		screen.y,
//		1,
//		GL_RGB,
//		GL_UNSIGNED_BYTE,
//		false);
//
//	
//
//
//	ao_sample_points = generateSpherePoints(ssao_samples, 1.0f, ssao_plus); 
//
//	if (!ssao_noise_texture)
//	{
//		int size = 4;
//		std::vector<float> noise_data(size * size * 3);
//
//		for (int i = 0; i < size * size; ++i)
//		{
//			float angle = float(rand()) / RAND_MAX * 2.0f * PI;
//			noise_data[i * 3 + 0] = cos(angle);
//			noise_data[i * 3 + 1] = sin(angle);
//			noise_data[i * 3 + 2] = 0.0f; // z = 0
//		}
//
//		ssao_noise_texture = new GFX::Texture();
//		ssao_noise_texture->create(size, size, GL_RGB, GL_FLOAT, &noise_data[0]);
//
//		ssao_noise_texture->bind();
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	}
//}


//float lerp(float a, float b, float f)
//{
//	return a + f * (b - a);
//}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;

	//ssao_setup();

}

//void Renderer::ssao(Camera* camera)
//{
//	if (!ssao_enabled || !ssao_shader) return;
//
//	GFX::Mesh* quad = GFX::Mesh::getQuad();
//
//
//	ssao_FBO->bind();
//	glClearColor(1.0, 1.0, 1.0, 1.0);
//	glClear(GL_COLOR_BUFFER_BIT);
//
//	ssao_shader->enable();
//
//	// SEND AO PARAMS
//
//	ssao_shader->setUniform("u_sample_count", ssao_samples);
//	ssao_shader->setUniform("u_sample_radius", ssao_radius);
//	ssao_shader->setUniform3Array("u_sample_pos", 
//									(float*)&ao_sample_points[0], 
//									ssao_samples);
//	ssao_shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], 1);
//
//	ssao_shader->setTexture("u_noise_texture", ssao_noise_texture, 2); // slot 2
//	ssao_shader->setUniform("u_noise_scale", Vector2f(float(ssao_FBO->width) / 4.0f, float(ssao_FBO->height) / 4.0f));
//
//	// Bind depth texture
//	ssao_shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, 0);
//
//	// SEND CAMERA MATRICES
//	mat4 proj = camera->projection_matrix;
//	mat4 proj_inv = proj;
//	proj_inv.inverse();
//
//	ssao_shader->setUniform("u_p_mat", proj);
//	ssao_shader->setUniform("u_inv_p_mat", proj_inv);
//
//
//	// Send the inverse of the FBO res, for the UVs
//	inv_width = 1.0f / ssao_FBO->color_textures[0]->width;
//	inv_height = 1.0f / ssao_FBO->color_textures[0]->height;
//	vec2 res_inv = vec2(inv_width, inv_height);
//	ssao_shader->setUniform("u_res_inv", res_inv);
//
//    ssao_shader->setTexture("u_depth_texture", gbuffer_fbo->depth_texture, 0);
//
//	quad->render(GL_TRIANGLES);
//
//	ssao_shader->disable();
//	ssao_FBO->unbind();
//
//}





void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	opaqueObjects.clear();
	transparentObjects.clear();

	this->scene = scene;
	setupScene();

	// Clear previous frame data
	draw_command_list.clear();
	light_list.clear();

	parseSceneEntities(scene, camera);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);
	
	gbuffer_fbo->bind();
	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();




	// HERE =====================
	// TODO: RENDER RENDERABLES
	// ==========================

	for (const sDrawCommand& command : draw_command_list) {
		if (command.material->alpha_mode == eAlphaMode::NO_ALPHA) {
			opaqueObjects.push_back(command);
		}
		else {
			transparentObjects.push_back(command);
		}
	}
    
    //render skybox
    if(skybox_cubemap)
        renderSkybox(skybox_cubemap);
    
	// Sort opaque objects normally
	std::sort(opaqueObjects.begin(), opaqueObjects.end(), compareDrawCommands(camera));

	// Sort transparent objects by distance (farther objects first)
	std::sort(transparentObjects.begin(), transparentObjects.end(),
		[camera](const sDrawCommand& a, const sDrawCommand& b) {
			float distanceA = camera->eye.distance(Vector3f(a.model.m[12], a.model.m[13], a.model.m[14]));
			float distanceB = camera->eye.distance(Vector3f(b.model.m[12], b.model.m[13], b.model.m[14]));
			return distanceA > distanceB; // Farther objects should be drawn first
		});

	// Render opaque objects first
	GBuffer();

	/*if (ssao_enabled) {
		ssao(camera);
	}*/



    
	gbuffer_fbo->unbind();

	// Lighting pass - Must be done after the gbuffer pass and unbind
	gbuffer_fbo->depth_texture->copyTo(lighting_fbo->depth_texture);
	lighting_fbo->bind();
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	lighting_fbo->unbind();

	//lighting_fbo->depth_texture->toViewport();

	if (lab == 2) {
     //   lighting_fbo->bind();
		renderDeferred();
        renderFire(Vector3(2.0f, 0.0f, -5.0f), 1.5f); // Position and size
        // Render transparent objects after
        for (const sDrawCommand& command : transparentObjects) {
            renderMeshWithMaterial(command.model, command.mesh, command.material, false);
        }
       // lighting_fbo->unbind();
       //lighting_fbo->color_textures[0]->toViewport();
	}
	else {
        // Render opaque objects first
        for (const sDrawCommand& command : opaqueObjects) {
            renderMeshWithMaterial(command.model, command.mesh, command.material, true);
        }
		// Render transparent objects after
		for (const sDrawCommand& command : transparentObjects) {
			renderMeshWithMaterial(command.model, command.mesh, command.material, false);
		}
	}
}




// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material, bool opaqueness)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	GFX::Shader* light_pass_shader = NULL;
	Camera* camera = Camera::current;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	//shader = GFX::Shader::Get("texture"); // Change here to Gbuffer shader

	if (lab == 2) {
		shader = GFX::Shader::Get("singlepass");
		//light_pass_shader = GFX::Shader::Get("light_pass");

		
		// A quad is usually a mesh of a plane,
		// always aligned with you view
		GFX::Mesh* quad = GFX::Mesh::getQuad();

		//no shader? then nothing to render
		if (!shader)
			return;
		shader->enable();

		// Send LIGHTS
		vec3* light_pos = new vec3[light_list.size()]; // Dynamic array to store light positions
		vec3* light_color = new vec3[light_list.size()]; // Dynamic array to store light colours
		float* light_intensity = new float[light_list.size()]; // Dynamic array to store light intensities
		vec3* light_dir = new vec3[light_list.size()]; // Dynamic array to store light directions
		int* light_type = new int[light_list.size()];

		// Send lights to shader GPU
		float alpha_max = 0.0f;
		float alpha_min = 0.0f;
		int i = 0u;// Light counter
		int* light_shadow_map_index = new int[light_list.size()];
		for (LightEntity* light : light_list) {
			light_pos[i] = light->root.getGlobalMatrix().getTranslation();
			light_intensity[i] = light->intensity;
			light_color[i] = light->color;
			light_dir[i] = light->root.getGlobalMatrix().rotateVector(vec3(0, 0, -1)); // Get forward direction
			light_type[i] = static_cast<int>(light->light_type);
			light_dir[i] = light->root.model.frontVector();

			//Check shadow map


			if (light->light_type == 2) {
				alpha_min = light->cone_info.x * 6.28 / 360; // Convert degrees to radians
				alpha_max = light->cone_info.y * 6.28 / 360;
			}


			i++;
		}

		int texture_slots = 0;



		// Bind the GBuffers
		shader->setTexture("u_gbuffer_color",
			gbuffer_fbo->color_textures[0],
			texture_slots++);
		shader->setTexture("u_gbuffer_normal",
			gbuffer_fbo->color_textures[1],
			texture_slots++);
		shader->setTexture("u_gbuffer_depth",
			gbuffer_fbo->depth_texture,
			texture_slots++);
		quad->render(GL_TRIANGLES);

		//light_pass_shader->disable();

		delete[] light_pos; // Free memory - no memory leaks
		delete[] light_color; // Free memory - no memory leaks
		delete[] light_intensity; // Free memory - no memory leaks
		delete[] light_dir; // Free memory - no memory leaks
		delete[] light_type; // Free memory - no memory leaks
	}

    assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	material->bind(shader);

	// Send LIGHTS
	vec3* light_pos = new vec3[light_list.size()]; // Dynamic array to store light positions
	vec3* light_color = new vec3[light_list.size()]; // Dynamic array to store light colours
	float* light_intensity = new float[light_list.size()]; // Dynamic array to store light intensities
	vec3* light_dir = new vec3[light_list.size()]; // Dynamic array to store light directions
	int* light_type = new int[light_list.size()];

	
	// Analyse things ...
	// Send lights to shader GPU
	float alpha_max = 0.0f;
	float alpha_min = 0.0f;
	int i = 0u;// Light counter
	int* light_shadow_map_index = new int[light_list.size()];
	for (LightEntity* light : light_list) {
		light_pos[i] = light->root.getGlobalMatrix().getTranslation();
		light_intensity[i] = light->intensity;
		light_color[i] = light->color;
		light_dir[i] = light->root.getGlobalMatrix().rotateVector(vec3(0, 0, -1)); // Get forward direction
		light_type[i] = static_cast<int>(light->light_type);
		light_dir[i] = light->root.model.frontVector();

		//Check shadow map


		if (light->light_type == 2) {
			alpha_min = light->cone_info.x * 6.28 / 360; // Convert degrees to radians
			alpha_max = light->cone_info.y * 6.28 / 360;
		}


		i++;
	}

	shader->setUniform("u_lab", lab);
	if (lab == 1) {
		if (multipass_on) {

			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			for (int i = 0; i < light_list.size(); i++) {
				if (i == 0) {
					if (opaqueness)
						glDisable(GL_BLEND);
					else
						glEnable(GL_BLEND);
				}
				else {
					glEnable(GL_BLEND);
				}
				shader->setUniform("u_multipass", 1);

				// Set only the light at index 0, shader will use it
				shader->setUniform("u_light_pos", light_pos[i]);
				shader->setUniform("u_light_color", light_color[i]);
				shader->setUniform("u_light_intensity", light_intensity[i]);
				shader->setUniform("u_light_dir", light_dir[i]);
				shader->setUniform("u_light_type", light_type[i]);
				shader->setUniform("u_alpha_min", alpha_min);
				shader->setUniform("u_alpha_max", alpha_max);
				shader->setUniform("u_light_index", i);
				shader->setUniform("u_light_count", 1); // just one light per pass
				shader->setUniform("u_alpha_cutoff", 0.1f);

				shader->setUniform("u_ambient_color", (i == 0) ? Scene::instance->ambient_light : vec3(0.f));

				// Upload matrices and other uniforms
				shader->setUniform("u_model", model);
				shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
				shader->setUniform("u_camera_position", camera->eye);
				float t = getTime();
				shader->setUniform("u_time", t);

				if (render_wireframe)
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				mesh->render(GL_TRIANGLES);
			}

			glDisable(GL_BLEND);
			glDepthFunc(GL_LESS);
		}

		else { // Single pass
			shader->setUniform("u_multipass", 0);
			shader->setUniform3Array("u_light_pos", (float*)light_pos, fmin(light_list.size(), 10));
			shader->setUniform3Array("u_light_color", (float*)light_color, fmin(light_list.size(), 10));
			shader->setUniform1Array("u_light_intensity", (float*)light_intensity, fmin(light_list.size(), 10));
			shader->setUniform3Array("u_light_dir", (float*)light_dir, fmin(light_list.size(), 10));
			shader->setUniform1Array("u_light_type", light_type, fmin(light_list.size(), 10));
			shader->setUniform1Array("u_light_shadow_index", light_shadow_map_index, fmin(light_list.size(), 10));
			shader->setUniform("u_alpha_min", alpha_min);
			shader->setUniform("u_alpha_max", alpha_max);
			shader->setUniform("u_light_count", (int)fmin(light_list.size(), 10));
			shader->setUniform("u_alpha_cutoff", 0.1f);

			shader->setUniform("u_ambient_color", Scene::instance->ambient_light);

			//upload uniforms
			shader->setUniform("u_model", model);

			// Upload camera uniforms
			shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			shader->setUniform("u_camera_position", camera->eye);

			// Upload time, for cool shader effects
			float t = getTime();
			shader->setUniform("u_time", t);

			// Render just the verticies as a wireframe
			if (render_wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			//do the draw call that renders the mesh into the screen
			mesh->render(GL_TRIANGLES);
		}
	}
	
	delete[] light_pos; // Free memory - no memory leaks
	delete[] light_color; // Free memory - no memory leaks
	delete[] light_intensity; // Free memory - no memory leaks
	delete[] light_dir; // Free memory - no memory leaks
	delete[] light_type; // Free memory - no memory leaks


	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);
    

	//add here your stuff
	//...
    ImGui::Text("Lighting Mode");
    if (ImGui::Checkbox("Use Multipass", &multipass_on)) {
        // Optional: trigger updates when toggled
    }

	ImGui::Text("SSAO Settings");
	ImGui::Checkbox("SSAO Enabled", &ssao_enabled);
	ImGui::Checkbox("SSAO Blur", &ssao_blur);
	ImGui::Checkbox("SSAO Lighting", &ssao_lighting);
	ImGui::SliderInt("SSAO Samples", &ssao_samples, 1, 64);
	ImGui::SliderFloat("SSAO Radius", &ssao_radius, 0.01f, 0.1f);
	ImGui::Checkbox("SSAO Plus", &ssao_plus);
	ImGui::Text("Lab: %d", lab);


}

#else
void Renderer::showUI() {}
#endif
