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
	    std::vector<glm::vec3> positions, colors, directions;
	    std::vector<float> intensities, innerCutoffs, outerCutoffs;
	    std::vector<int> types;

	    // Shadow casters
	    for (const auto& obj : objects) {
		    if (!obj->enabled || !obj->light || !obj->light->castsShadow) continue;
		    positions.push_back(obj->transform.position);
		    colors.push_back(obj->light->color);
		    intensities.push_back(obj->light->intensity);
		    directions.push_back(obj->light->direction);
		    innerCutoffs.push_back(obj->light->innerCutoff);
		    outerCutoffs.push_back(obj->light->outerCutoff);
		    types.push_back(static_cast<int>(obj->light->type));
	    }
	    // Non shadow lights
	    for (const auto& obj : objects) {
		    if (!obj->enabled || !obj->light || obj->light->castsShadow) continue;
		    positions.push_back(obj->transform.position);
		    colors.push_back(obj->light->color);
		    intensities.push_back(obj->light->intensity);
		    directions.push_back(obj->light->direction);
		    innerCutoffs.push_back(obj->light->innerCutoff);
		    outerCutoffs.push_back(obj->light->outerCutoff);
		    types.push_back(static_cast<int>(obj->light->type));
	    }

	    int count = positions.size();
	    glUniform1i(glGetUniformLocation(shader, "lightCount"), count);
	    for (int i = 0; i < count; i++) {
		    std::string idx = std::to_string(i);
		    glUniform3fv(glGetUniformLocation(shader, ("lightPos["      + idx + "]").c_str()), 1, glm::value_ptr(positions[i]));
		    glUniform3fv(glGetUniformLocation(shader, ("lightColor["    + idx + "]").c_str()), 1, glm::value_ptr(colors[i]));
		    glUniform1f (glGetUniformLocation(shader, ("lightIntensity["+ idx + "]").c_str()), intensities[i]);
		    glUniform3fv(glGetUniformLocation(shader, ("lightDir["      + idx + "]").c_str()), 1, glm::value_ptr(directions[i]));
		    glUniform1f (glGetUniformLocation(shader, ("innerCutoff["   + idx + "]").c_str()), innerCutoffs[i]);
		    glUniform1f (glGetUniformLocation(shader, ("outerCutoff["   + idx + "]").c_str()), outerCutoffs[i]);
		    glUniform1i (glGetUniformLocation(shader, ("lightType["     + idx + "]").c_str()), types[i]);
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