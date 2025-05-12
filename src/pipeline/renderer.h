#pragma once
#include "scene.h"
#include "prefab.h"

#include "light.h"

//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}



namespace SCN {

	class Prefab;
	class Material;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;

		GFX::Texture* skybox_cubemap;
		Vector2ui screen;
		SCN::Scene* scene;

		GFX::FBO gbuffer_fbo;

		//updated every frame
		Renderer(const char* shaders_atlas_filename );

		void initGBuffer();

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...
		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

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