#include "renderer.h"

using namespace SCN;

void Renderer::parseNodes(SCN::Node* node, Camera* cam) {
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
        //        Store Children Prefab Entities

        // Store Lights
        // ...
    }
    
}
