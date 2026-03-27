#pragma once
#include "config.h"
#include "gameobject.h"

class Scene
{
public:
    std::vector<std::shared_ptr<GameObject>> objects;

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
            if (obj->enabled)
                obj->Update(dt);
        }
    }

    void Render(unsigned int shader, int modelLoc, int shininessLoc) const {
        // Lights
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> colors;
        std::vector<float>     intensities;

        for (const std::shared_ptr<GameObject>& obj : objects) {
            if (!obj->enabled || !obj->light) continue;
            positions.push_back(obj->transform.position);
            colors.push_back(obj->light->color);
            intensities.push_back(obj->light->intensity);
        }

        int lightCount = positions.size();
        glUniform1i(glGetUniformLocation(shader, "lightCount"), lightCount);

        for (int i = 0; i < lightCount; i++) {
            std::string base = "lightPos[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shader, base.c_str()), 1,
                glm::value_ptr(positions[i]));

            base = "lightColor[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shader, base.c_str()), 1,
                glm::value_ptr(colors[i]));

            base = "lightIntensity[" + std::to_string(i) + "]";
            glUniform1f(glGetUniformLocation(shader, base.c_str()), intensities[i]);
        }

        for (const std::shared_ptr<GameObject>& obj : objects) {
            if (!obj->enabled || !obj->mesh) continue;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE,
                glm::value_ptr(obj->transform.GetMatrix()));
            glUniform1f(shininessLoc, obj->mesh->GetShininess());
            obj->mesh->Draw();
        }
    }

    void RenderGeometry(unsigned int gBufferShader, int modelLoc, int shininessLoc) const {
        for (const auto& obj : objects) {
            if (!obj->enabled || !obj->mesh) continue;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE,
                glm::value_ptr(obj->transform.GetMatrix()));
            glUniform1f(shininessLoc, obj->mesh->GetShininess());
            obj->mesh->Draw();
        }
    }

    void UploadLights(unsigned int lightingShader) const {
        std::vector<glm::vec3> positions, colors;
        std::vector<float> intensities;

        for (const auto& obj : objects) {
            if (!obj->enabled || !obj->light) continue;
            positions.push_back(obj->transform.position);
            colors.push_back(obj->light->color);
            intensities.push_back(obj->light->intensity);
        }

        int count = positions.size();
        glUniform1i(glGetUniformLocation(lightingShader, "lightCount"), count);
        for (int i = 0; i < count; i++) {
            std::string base = "lightPos[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(lightingShader, base.c_str()), 1,
                glm::value_ptr(positions[i]));
            base = "lightColor[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(lightingShader, base.c_str()), 1,
                glm::value_ptr(colors[i]));
            base = "lightIntensity[" + std::to_string(i) + "]";
            glUniform1f(glGetUniformLocation(lightingShader, base.c_str()),
                intensities[i]);
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