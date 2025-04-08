#include "renderer.h"

#include <algorithm> //sort

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"
#include "../gfx/fbo.h"
#include "../pipeline/prefab.h"
#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"

#include "scene.h"


struct sDrawCommand {
	GFX::Mesh* mesh; // Contains the geometry of the entity
	SCN::Material* material; // Material in scene not GFX. Is the recipe for putting together the surafce
	Matrix44 model; // Contains location of entity

};

std::vector<sDrawCommand> draw_command_list; // Contains all the entities to be drawn
std::vector<sDrawCommand> opaqueObjects;
std::vector<sDrawCommand> transparentObjects;
std::vector<SCN::LightEntity*> light_list; // Contains all the lights in the scene

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



//some globals
GFX::Mesh sphere;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

void parseNodes(SCN::Node* node, Camera* cam) {
	if (!node) {
		return;
	}
	// Maybe there is camera here for (extra) frustrum culling purposes
	if (node->mesh) {
		sDrawCommand draw_com; // Create draw command
		draw_com.mesh = node->mesh; // Set mesh to prefab root mesh
		draw_com.material = node->material; // Set material to prefab root material
		draw_com.model = node->getGlobalMatrix(); // I have no idea what this part is

		draw_command_list.push_back(draw_com); // Add draw command to list
	}

	for (SCN::Node* child : node->children) {
		parseNodes(child, cam); // Recursively parse children
	}
}

void parseLights(SCN::Node* node, Camera* cam) {
	if (!node) {
		return;
	}
}

void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam) {
	// HERE =====================
	// TODO: GENERATE RENDERABLES
	// ==========================

	//Clean the list of draw commands
	draw_command_list.clear();
	light_list.clear(); // <- clear light list before filling it again


	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		if (entity->getType() == eEntityType::PREFAB) { // Only render prefabs here

			parseNodes(&(((PrefabEntity*)entity)->root), cam);// Parse nodes of prefab using recursive function
		}
		else if (entity->getType() == eEntityType::LIGHT) {
			//BaseEntity* light = (BaseEntity*)&entity;
			light_list.push_back((LightEntity*)entity);
		}
		// Store Prefab Entitys
		// ...
		//		Store Children Prefab Entities

		// Store Lights
		// ...
	}
	
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	opaqueObjects.clear();
	transparentObjects.clear();

	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);

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
	for (const sDrawCommand& command : opaqueObjects) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	// Render transparent objects after
	for (const sDrawCommand& command : transparentObjects) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}
}


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

// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	shader = GFX::Shader::Get("texture");

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
	// Analyse things ...
	// Send lights to shader GPU
	int i = 0u;// Light counter
	for (LightEntity* light : light_list) {
		light_pos[i] = light->root.getGlobalMatrix().getTranslation();
		light_intensity[i] = light->intensity;
		light_color[i] = light->color;
		light_dir[i] = light->root.getGlobalMatrix().rotateVector(vec3(0, 0, -1)); // Get forward direction
		//light_dir[i] = light->root.frontVector();
		i++;
	}
	

	shader->setUniform3Array("u_light_pos", (float*)light_pos, min(light_list.size(), 10));
	shader->setUniform3Array("u_light_color", (float*)light_color, min(light_list.size(), 10));
	shader->setUniform1Array("u_light_intensity", (float*)light_intensity, min(light_list.size(), 10));

	delete[] light_pos; // Free memory - no memory leaks
	delete[] light_color; // Free memory - no memory leaks
	delete[] light_intensity; // Free memory - no memory leaks
	delete[] light_dir; // Free memory - no memory leaks

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_light_count", (int)light_list.size());

	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t );

	// Render just the verticies as a wireframe
	if (render_wireframe)
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

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
}

#else
void Renderer::showUI() {}
#endif