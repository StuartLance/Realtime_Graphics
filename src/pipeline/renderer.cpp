#include "renderer.h"
#include "compareDrawCommands.h"

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

// Struct for rendering entity [Further Labs need to add more to this struct]
struct sDrawCommand {
	GFX::Mesh* mesh; // Contains the geometry of the entity
	SCN::Material* material; // Material in scene not GFX. Is the recipe for putting together the surafce
	Matrix44 model; // Contains location of entity

};

//std::vector<sDrawCommand> draw_command_list; // Contains all the entities to be drawn

std::vector<sDrawCommand> translucent_draw_command_list; // Contains all the translucent entities to be drawn
std::vector<sDrawCommand> opaque_draw_command_list; // Contains all opaque entities to be drawn

using namespace SCN;

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

		if (node->material->alpha_mode == (eAlphaMode::BLEND || eAlphaMode::MASK)) {
			translucent_draw_command_list.push_back(draw_com); // Add draw command to list
		}
		else {
			opaque_draw_command_list.push_back(draw_com); // Add draw command to list
		}
		
		//draw_command_list.push_back(draw_com); // Add draw command to list
	}

	for (SCN::Node* child : node->children) {
		parseNodes(child, cam); // Recursively parse children
	}
}

void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam) {
	// HERE =====================
	// TODO: GENERATE RENDERABLES
	// ==========================

	//Clean the list of draw commands
	opaque_draw_command_list.clear();
	translucent_draw_command_list.clear();

	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		if (entity->getType() == eEntityType::PREFAB) {
			// CORRECT: this will add to the scene all the node hierachy correctly and since we are starting with the entity root at the top of
			//   the hierachy it will have a world transform
			parseNodes(&(((PrefabEntity*)entity)->root), cam);
		}
		// Store Prefab Entitys
		// ...
		//		Store Children Prefab Entities

		// Store Lights
		// ...
	}
	
}

// Used in the sorting alg when rendeering, so that glass doesn't block rendering of objects behind
//bool compareDrawCommands(const sDrawCommand& a, const sDrawCommand& b, Camera* camera) 
//{
//    if (a.material->alpha_mode == eAlphaMode::BLEND && b.material->alpha_mode == eAlphaMode::NO_ALPHA) {
//        return false; // Render BLEND after NO_ALPHA
//    } 
//	else if (a.material->alpha_mode == eAlphaMode::NO_ALPHA && b.material->alpha_mode == eAlphaMode::BLEND) {
//        return true; // Render NO_ALPHA before BLEND
//    } 
//	else { // When they have the same Alpha
//        // Render based on distance from camera
//        float distanceA = camera->eye.distance(Vector3f(a.model.m[12], a.model.m[13], a.model.m[14]));
//        float distanceB = camera->eye.distance(Vector3f(b.model.m[12], b.model.m[13], b.model.m[14]));
//        return distanceA < distanceB;
//    }
//}

// Struct compareDrawCommands is now in separate file compareDrawCommands.h

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
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

    #include <algorithm> //sort

    // ...

    // Compare function for sorting draw commands based on alpha mode
	//std::sort(draw_command_list.begin(), draw_command_list.end(), compareDrawCommands(camera)); // Sort draw commands

	std::sort(opaque_draw_command_list.begin(), opaque_draw_command_list.end(), compareDrawCommands(camera, eAlphaMode::NO_ALPHA)); // Sort draw commands
	std::sort(translucent_draw_command_list.begin(), translucent_draw_command_list.end(), compareDrawCommands(camera, eAlphaMode::BLEND)); // Sort draw commands
	// 


	// Render all draw commands

	
	for (sDrawCommand command : opaque_draw_command_list) {
            renderMeshWithMaterial(command.model, command.mesh, command.material);
        }
	for (sDrawCommand command : translucent_draw_command_list) {
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

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

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