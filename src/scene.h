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
    	int hasNormalMapLoc = glGetUniformLocation(gBufferShader, "hasNormalMap");

        for (const auto& obj : objects) {
            if (!obj->enabled || !obj->mesh) continue;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE,
                glm::value_ptr(obj->transform.GetMatrix()));
            glUniform1f(shininessLoc, obj->mesh->GetShininess());
        	glUniform1i(hasNormalMapLoc, obj->mesh->material.hasNormalMap ? 1 : 0);
            obj->mesh->Draw();
        }
    }

	void UploadLights(unsigned int shader) const {
    	// Shadow casters first, then non-shadow lights
    	std::vector<std::shared_ptr<GameObject>> lightObjects;

    	for (const auto& obj : objects)
    		if (obj->enabled && obj->light && obj->light->castsShadow)
    			lightObjects.push_back(obj);

    	for (const auto& obj : objects)
    		if (obj->enabled && obj->light && !obj->light->castsShadow)
    			lightObjects.push_back(obj);

    	int count = static_cast<int>(lightObjects.size());
    	glUniform1i(glGetUniformLocation(shader, "lightCount"), count);

    	for (int i = 0; i < count; i++) {
    		auto& obj = lightObjects[i];
    		auto& light = obj->light;
    		std::string p = "lights[" + std::to_string(i) + "].";

    		glUniform3fv(glGetUniformLocation(shader, (p + "position").c_str()),    1, glm::value_ptr(obj->transform.position));
    		glUniform3fv(glGetUniformLocation(shader, (p + "color").c_str()),       1, glm::value_ptr(light->color));
    		glUniform3fv(glGetUniformLocation(shader, (p + "direction").c_str()),   1, glm::value_ptr(light->direction));
    		glUniform1f (glGetUniformLocation(shader, (p + "intensity").c_str()),   light->intensity);
    		glUniform1f (glGetUniformLocation(shader, (p + "innerCutoff").c_str()), light->innerCutoff);
    		glUniform1f (glGetUniformLocation(shader, (p + "outerCutoff").c_str()), light->outerCutoff);
    		glUniform1i (glGetUniformLocation(shader, (p + "type").c_str()),        static_cast<int>(light->type));
    	    glUniform1f(glGetUniformLocation(shader, (p + "radius").c_str()), light->radius);
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