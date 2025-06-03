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

	struct sFire {
		Vector3f position;
		float scale;
		int noise_octaves;
		float intensity;
		float scroll_speed;
		float fire_height;
		float fire_shape;
		float fire_thickness;
		float fire_sharpness;
		float detail_strength;
		float noise_lacunarity;
		float noise_gain;
		float noise_frequency;
		float noise_amplitude;
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

		std::vector<sFire> fire_list; // List of fire entities

		GFX::Texture* skybox_cubemap;
		Vector2ui screen;
		SCN::Scene* scene;

		GFX::FBO* gbuffer_fbo;
		GFX::FBO* lighting_fbo;
		
		float inv_width;
		float inv_height;
		bool ssao_plus = false;
		GFX::Texture* ssao_noise_texture = nullptr;
		GFX::FBO* ssao_FBO;
		GFX::Shader* ssao_shader = nullptr;
		GFX::Texture* ssao_texture = nullptr;
		int ssao_samples = 32;
		float ssao_radius = 0.05;
		bool ssao_enabled = false;
		bool ssao_blur = false;
		bool ssao_lighting = false;
		std::vector<vec3> ao_sample_points;
		
		// FROM learnopengl.com/Advanced-Lighting/SSAO
		//std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
		//std::default_random_engine generator;
		//std::vector<glm::vec3> ssaoKernel;

		//updated every frame
		Renderer(const char* shaders_atlas_filename );
		void ssao(Camera* camera);
		std::vector<vec3> generateSpherePoints(int num, float radius, bool hemi);
		void initFirelist();
		void initGBuffer();

		void ssao_setup();
		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...
		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		void renderVolumes(Camera* camera);
		void renderDeferred();
		void GBuffer();
        void renderFire(sFire firelist);
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
