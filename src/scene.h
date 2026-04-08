#pragma once
#include "config.h"
#include "gameobject.h"
#include "components/particleSystemComponent.h"
#include "components/fogVolumeComponent.h"
#include "components/meshComponent.h"
#include "components/lightComponent.h"
#include "rendering/skybox.h"

class Scene
{
public:
    std::vector<std::shared_ptr<GameObject>> objects;
    std::shared_ptr<Skybox> skybox;

    void LoadSkybox(const std::string& directory) {
        std::vector<std::string> faces = {
            directory + "/right.jpg",
            directory + "/left.jpg",
            directory + "/top.jpg",
            directory + "/bottom.jpg",
            directory + "/front.jpg",
            directory + "/back.jpg",
        };
        skybox = std::make_shared<Skybox>();
        skybox->Load(faces);
    }

    void Add(const std::shared_ptr<GameObject>& obj) {
        objects.push_back(obj);
    }

    void Start() const {
        for (const std::shared_ptr<GameObject>& obj : objects) {
            if (obj->enabled)
                obj->Start();
        }
    }

    void Update(float dt) const {
        for (const std::shared_ptr<GameObject>& obj : objects) {
            if (!obj->enabled) continue;
            obj->Update(dt);

            auto ps = obj->GetComponent<ParticleSystemComponent>();
            if (ps)
                ps->system->Update(dt, obj->transform.position);
        }
    }

    void RenderGeometry(unsigned int gBufferShader, int modelLoc, int shininessLoc) const {
        int hasNormalMapLoc = glGetUniformLocation(gBufferShader, "hasNormalMap");

        for (const auto& obj : objects) {
            if (!obj->enabled) continue;
            auto mc = obj->GetComponent<MeshComponent>();
            if (!mc) continue;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE,
                glm::value_ptr(obj->transform.GetMatrix()));
            glUniform1f(shininessLoc, mc->mesh->GetShininess());
            glUniform1i(hasNormalMapLoc, mc->mesh->material.hasNormalMap ? 1 : 0);
            mc->mesh->Draw();
        }
    }

    void UploadLights(unsigned int shader) const {
        std::vector<std::shared_ptr<GameObject>> lightObjects;

        for (const auto& obj : objects) {
            auto lc = obj->GetComponent<LightComponent>();
            if (obj->enabled && lc && lc->light->castsShadow)
                lightObjects.push_back(obj);
        }
        for (const auto& obj : objects) {
            auto lc = obj->GetComponent<LightComponent>();
            if (obj->enabled && lc && !lc->light->castsShadow)
                lightObjects.push_back(obj);
        }

        int count = static_cast<int>(lightObjects.size());
        glUniform1i(glGetUniformLocation(shader, "lightCount"), count);

        for (int i = 0; i < count; i++) {
            auto& obj = lightObjects[i];
            auto light = obj->GetComponent<LightComponent>()->light;
            std::string p = "lights[" + std::to_string(i) + "].";

            glUniform3fv(glGetUniformLocation(shader, (p + "position").c_str()),    1, glm::value_ptr(obj->transform.position));
            glUniform3fv(glGetUniformLocation(shader, (p + "color").c_str()),       1, glm::value_ptr(light->color));
            glUniform3fv(glGetUniformLocation(shader, (p + "direction").c_str()),   1, glm::value_ptr(light->direction));
            glUniform1f (glGetUniformLocation(shader, (p + "intensity").c_str()),   light->intensity);
            glUniform1f (glGetUniformLocation(shader, (p + "innerCutoff").c_str()), light->innerCutoff);
            glUniform1f (glGetUniformLocation(shader, (p + "outerCutoff").c_str()), light->outerCutoff);
            glUniform1i (glGetUniformLocation(shader, (p + "type").c_str()),        static_cast<int>(light->type));
            glUniform1f (glGetUniformLocation(shader, (p + "radius").c_str()),      light->radius);
        }
    }


    void UploadFogVolumes(unsigned int shader) const {
        std::vector<std::shared_ptr<GameObject>> fogObjects;

        for (const auto& obj : objects) {
            auto fv = obj->GetComponent<FogVolumeComponent>();
            if (obj->enabled && fv)
                fogObjects.push_back(obj);
        }

        int count = static_cast<int>(fogObjects.size());
        glUniform1i(glGetUniformLocation(shader, "fogVolumeCount"), count);

        for (int i = 0; i < count; i++) {
            auto fv = fogObjects[i]->GetComponent<FogVolumeComponent>();
            glm::vec3 offset = fogObjects[i]->transform.position;
            glm::vec3 worldMin = fv->volume->boundsMin + offset;
            glm::vec3 worldMax = fv->volume->boundsMax + offset;
            std::string p = "fogVolumes[" + std::to_string(i) + "].";

            glUniform3fv(glGetUniformLocation(shader, (p + "boundsMin").c_str()), 1, glm::value_ptr(worldMin));
            glUniform3fv(glGetUniformLocation(shader, (p + "boundsMax").c_str()), 1, glm::value_ptr(worldMax));
            glUniform1f(glGetUniformLocation(shader,  (p + "density").c_str()),     fv->volume->density);
            glUniform1f(glGetUniformLocation(shader,  (p + "scale").c_str()),       fv->volume->scale);
            glUniform1f(glGetUniformLocation(shader,  (p + "scrollSpeed").c_str()), fv->volume->scrollSpeed);
        }
    }

    std::shared_ptr<GameObject> GetObject(const std::string& name) {
        for (const auto& obj : objects) {
            if (obj->enabled && obj->name == name) return obj;
        }
        return nullptr;
    }

    ~Scene() {

    }
};