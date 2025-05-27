#pragma once
#include "scene.h"
#include "prefab.h"

#include "light.h"
#include "../gfx/fbo.h"

#include <algorithm> //sort

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"

#include "../pipeline/prefab.h"
#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"

//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}



namespace SCN {
    struct sDrawCommand {
        GFX::Mesh* mesh; // Contains the geometry of the entity
        SCN::Material* material; // Material in scene not GFX. Is the recipe for putting together the surafce
        Matrix44 model; // Contains location of entity

    };
	class Prefab;
	class Material;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;
 
        std::vector<sDrawCommand> draw_command_list; // Contains all the entities to be drawn
        std::vector<sDrawCommand> opaqueObjects;
        std::vector<sDrawCommand> transparentObjects;
        std::vector<SCN::LightEntity*> light_list; // Contains all the lights in the scene
        GFX::Mesh sphere;
		int lab;

		GFX::Texture* skybox_cubemap;
		Vector2ui screen;
		SCN::Scene* scene;

		GFX::FBO* gbuffer_fbo;
		GFX::FBO* lighting_fbo;
		/*GFX::Texture* ssao_noise_texture = nullptr;
		GFX::FBO* ssao_fbo;
		GFX::Shader* ssao_shader = nullptr;
		GFX::Texture* ssao_texture = nullptr;
		int ssao_samples = 32;
		float ssao_radius = 0.05;
		bool ssao_enabled = false;
		bool ssao_blur = false;
		bool ssao_lighting = false;
		std::vector<vec3> ao_sample_points;*/

		//updated every frame
		Renderer(const char* shaders_atlas_filename );
		void ssao(Camera* camera);
		std::vector<vec3> generateSpherePoints(int num, float radius, bool hemi);
		void initGBuffer();

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...
		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		void renderVolumes(Camera* camera);
		void renderDeferred();
		void GBuffer();
        void renderFire(const Vector3f& position, float scale);
        void parseNodes(SCN::Node* node, Camera* cam);

		
		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);

		void setUniform(GFX::Shader* shader);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material, bool opaqueness);

		void showUI();
	};



};
