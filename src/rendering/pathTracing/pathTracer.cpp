#include "pathTracer.h"
#include "../../scene.h"
#include "../../rendering/mesh/meshComponent.h"

PTScene BuildPTScene(std::shared_ptr<Scene> scene)
{
    PTScene pt;
    std::unordered_map<unsigned int, int> textureHandleToIndex;

    for (auto& obj : scene->objects)
    {
        if (!obj->enabled || !obj->isStatic) continue;
        auto mc = obj->GetComponent<MeshComponent>();
        if (!mc || !mc->model) continue;
        if (obj->IsForwardRendered()) continue;

        for (auto& sm : mc->model->subMeshes)
        {
            unsigned int indexOffset = (unsigned int)(pt.allVerts.size() / 3);

            for (int v = 0; v < (int)sm.positions.size(); v++)
            {
                pt.allVerts.push_back(sm.positions[v].x);
                pt.allVerts.push_back(sm.positions[v].y);
                pt.allVerts.push_back(sm.positions[v].z);
                pt.allUVs.push_back(sm.uvs[v]);
            }

            for (auto& idx : sm.indices)
                pt.allIndices.push_back(idx + indexOffset);

            int texIndex = -1;
            if (sm.materialIndex >= 0 && sm.materialIndex < (int)mc->model->materials.size())
            {
                unsigned int glHandle = mc->model->materials[sm.materialIndex].diffuseTexture;

                printf("SubMesh materialIndex=%d glHandle=%u\n", sm.materialIndex, glHandle);

                if (glHandle != 0)
                {
                    auto it = textureHandleToIndex.find(glHandle);
                    if (it != textureHandleToIndex.end()) {
                        texIndex = it->second;
                    } else {
                        texIndex = (int)pt.textures.size();
                        pt.textures.push_back(ReadbackTexture(glHandle));
                        textureHandleToIndex[glHandle] = texIndex;
                    }
                }
            }

            glm::vec3 emissiveColor(0.0f);
            float emissiveIntensity = 0.0f;
            if (sm.materialIndex >= 0 && sm.materialIndex < (int)mc->model->materials.size()) {
                emissiveColor     = mc->model->materials[sm.materialIndex].emissiveColor;
                emissiveIntensity = mc->model->materials[sm.materialIndex].emissiveIntensity;
            }

            int triCount = (int)sm.indices.size() / 3;
            for (int t = 0; t < triCount; t++) {
                pt.triTextureIndex.push_back(texIndex);
                pt.triEmissiveColor.push_back(emissiveColor);
                pt.triEmissiveIntensity.push_back(emissiveIntensity);
            }
        }
    }

    nanort::BVHBuildOptions<float> options;
    nanort::TriangleMesh<float> triMesh(
        pt.allVerts.data(),
        pt.allIndices.data(),
        sizeof(float) * 3
    );
    nanort::TriangleSAHPred<float> pred(
        pt.allVerts.data(),
        pt.allIndices.data(),
        sizeof(float) * 3
    );

    pt.accel.Build((unsigned int)(pt.allIndices.size() / 3), triMesh, pred, options);
    pt.built = true;

    return pt;
}

CPUTexture ReadbackTexture(unsigned int glHandle)
{
    CPUTexture tex;
    if (glHandle == 0) return tex;

    glBindTexture(GL_TEXTURE_2D, glHandle);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &tex.width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex.height);

    if (tex.width <= 0 || tex.height <= 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    GLint internalFormat = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);

    bool hasAlpha = (internalFormat == GL_RGBA || internalFormat == GL_RGBA8 ||
                     internalFormat == GL_RGBA16F || internalFormat == GL_RGBA32F);

    int channels = hasAlpha ? 4 : 3;
    GLenum fmt   = hasAlpha ? GL_RGBA : GL_RGB;

    std::vector<unsigned char> raw(tex.width * tex.height * channels);
    glGetTexImage(GL_TEXTURE_2D, 0, fmt, GL_UNSIGNED_BYTE, raw.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    tex.pixels.resize(tex.width * tex.height);
    for (int i = 0; i < tex.width * tex.height; i++) {
        tex.pixels[i] = glm::vec3(
            std::pow(raw[i * channels + 0] / 255.0f, 2.2f),
            std::pow(raw[i * channels + 1] / 255.0f, 2.2f),
            std::pow(raw[i * channels + 2] / 255.0f, 2.2f)
        );
        // alpha channel ignored, we only care about RGB for path tracing
    }

    if (raw.size() >= 3) {
        printf("Texture %u: size=%dx%d channels=%d first_pixel=(%d,%d,%d) internal=0x%X\n",
               glHandle, tex.width, tex.height, channels,
               raw[0], raw[1], raw[2], internalFormat);
    }

    return tex;
}

std::vector<PTLight> ExtractLights(std::shared_ptr<Scene> scene)
{
    std::vector<PTLight> lights;
    for (auto& obj : scene->objects)
    {
        if (!obj->enabled || !obj->isStatic) continue;
        auto lc = obj->GetComponent<LightComponent>();
        if (!lc) continue;

        PTLight l;
        l.position  = obj->transform.position;
        l.color     = lc->light->color;
        l.intensity = lc->light->intensity;
        l.radius    = lc->light->radius;
        l.type      = static_cast<int>(lc->light->type);
        l.direction = lc->light->direction;
        lights.push_back(l);
    }
    return lights;
}

// Returns a random direction in the hemisphere around normal
static glm::vec3 RandomHemisphere(const glm::vec3& normal, std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float u1 = dist(rng);
    float u2 = dist(rng);

    // Cosine weighted hemisphere sampling
    float r   = std::sqrt(u1);
    float phi = 2.0f * 3.14159265f * u2;
    float x   = r * std::cos(phi);
    float z   = r * std::sin(phi);
    float y   = std::sqrt(std::max(0.0f, 1.0f - u1));

    // Build tangent space around normal
    glm::vec3 up = std::abs(normal.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 tangent   = glm::normalize(glm::cross(up, normal));
    glm::vec3 bitangent = glm::cross(normal, tangent);

    return glm::normalize(tangent * x + normal * y + bitangent * z);
}

static glm::vec3 GetSurfaceNormal(const PTScene& pt, int triIndex, float u, float v)
{
    unsigned int i0 = pt.allIndices[triIndex * 3 + 0];
    unsigned int i1 = pt.allIndices[triIndex * 3 + 1];
    unsigned int i2 = pt.allIndices[triIndex * 3 + 2];

    glm::vec3 p0(pt.allVerts[i0*3], pt.allVerts[i0*3+1], pt.allVerts[i0*3+2]);
    glm::vec3 p1(pt.allVerts[i1*3], pt.allVerts[i1*3+1], pt.allVerts[i1*3+2]);
    glm::vec3 p2(pt.allVerts[i2*3], pt.allVerts[i2*3+1], pt.allVerts[i2*3+2]);

    glm::vec3 edge1 = p1 - p0;
    glm::vec3 edge2 = p2 - p0;
    return glm::normalize(glm::cross(edge1, edge2));
}

static glm::vec2 GetSurfaceUV(const PTScene& pt, int triIndex, float u, float v)
{
    unsigned int i0 = pt.allIndices[triIndex * 3 + 0];
    unsigned int i1 = pt.allIndices[triIndex * 3 + 1];
    unsigned int i2 = pt.allIndices[triIndex * 3 + 2];

    glm::vec2 uv0 = pt.allUVs[i0];
    glm::vec2 uv1 = pt.allUVs[i1];
    glm::vec2 uv2 = pt.allUVs[i2];

    // Barycentric interpolation
    return uv0 * (1.0f - u - v) + uv1 * u + uv2 * v;
}

static glm::vec3 GetSurfaceAlbedo(const PTScene& pt, int triIndex, float u, float v)
{
    glm::vec2 uv = GetSurfaceUV(pt, triIndex, u, v);
    int texIdx = pt.triTextureIndex[triIndex];
    if (texIdx < 0 || texIdx >= (int)pt.textures.size())
        return glm::vec3(0.8f);
    return pt.textures[texIdx].Sample(uv);
}

static float GetLightAttenuation(const PTLight& light, const glm::vec3& hitPos)
{
    if (light.type == 0) return 1.0f; // directional
    float dist = glm::length(light.position - hitPos);
    float atten = 1.0f / (dist * dist);
    if (light.type == 2) // point
        atten *= 1.0f - glm::smoothstep(light.radius * 0.75f, light.radius, dist);
    return atten;
}

static glm::vec3 GetSurfaceEmissive(const PTScene& pt, int triIndex)
{
    if (triIndex >= (int)pt.triEmissiveColor.size()) return glm::vec3(0.0f);
    return pt.triEmissiveColor[triIndex] * pt.triEmissiveIntensity[triIndex];
}

static bool CastShadowRay(const PTScene& pt, const glm::vec3& origin, const glm::vec3& target)
{
    glm::vec3 dir = target - origin;
    float maxDist = glm::length(dir) - 0.01f;
    dir = glm::normalize(dir);

    nanort::Ray<float> ray;
    ray.org[0] = origin.x;
    ray.org[1] = origin.y;
    ray.org[2] = origin.z;
    ray.dir[0] = dir.x;
    ray.dir[1] = dir.y;
    ray.dir[2] = dir.z;
    ray.min_t  = 0.001f;
    ray.max_t  = maxDist;

    nanort::TriangleIntersector<float> intersector(
        pt.allVerts.data(), pt.allIndices.data(), sizeof(float) * 3);
    nanort::TriangleIntersection<float> isect;

    return pt.accel.Traverse(ray, intersector, &isect);
}

glm::vec3 TracePath(
    const PTScene& pt,
    const std::vector<PTLight>& lights,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    int bounces,
    std::mt19937& rng
    )
{
    glm::vec3 throughput = glm::vec3(1.0f);
    glm::vec3 radiance = glm::vec3(0.0f);

    glm::vec3 origin = rayOrigin;
    glm::vec3 dir = rayDir;

    for (int bounce = 0; bounce < bounces; bounce++) {
        nanort::Ray<float> ray;
        ray.org[0] = origin.x;
        ray.org[1] = origin.y;
        ray.org[2] = origin.z;

        ray.dir[0] = dir.x;
        ray.dir[1] = dir.y;
        ray.dir[2] = dir.z;

        ray.min_t  = 0.002f;
        ray.max_t = 1e30f;

        nanort::TriangleIntersector<float> intersector(
            pt.allVerts.data(), pt.allIndices.data(), sizeof(float) * 3
        );

        nanort::TriangleIntersection<float> intersection;

        bool hit = pt.accel.Traverse(ray, intersector, &intersection);

        if (!hit) break; // Ray escaped

        if (intersection.t < 0.001f || intersection.t > 1e29f) break;

        glm::vec3 hitPos = origin + dir * intersection.t;
        glm::vec3 normal = GetSurfaceNormal(pt, intersection.prim_id, intersection.u, intersection.v);
        glm::vec3 albedo = GetSurfaceAlbedo(pt, intersection.prim_id, intersection.u, intersection.v);
        glm::vec3 emissive = GetSurfaceEmissive(pt, intersection.prim_id);

        // Flip normal if backface
        if (glm::dot(normal, -dir) < 0.0f)
            normal = -normal;

        if (glm::length(emissive) > 0.0f)
            radiance += throughput * emissive;

        for (auto& light : lights)
        {
            glm::vec3 toLight = (light.type == 0)
                ? glm::normalize(-light.direction)
                : glm::normalize(light.position - hitPos);

            float NdotL = glm::max(glm::dot(normal, toLight), 0.0f);
            if (NdotL <= 0.0f) continue;

            glm::vec3 shadowOrigin = hitPos + normal * 0.002f;
            glm::vec3 shadowTarget = (light.type == 0)
                ? hitPos + toLight * 1000.0f
                : light.position;

            if (CastShadowRay(pt, shadowOrigin, shadowTarget)) continue;

            float atten = GetLightAttenuation(light, hitPos);

            // Bounce 0 = direct view of lit surface from probe
            // Bounce 1+ = indirect, attenuated by previous surface albedos
            radiance += throughput * albedo * light.color * light.intensity * atten * NdotL;
        }


        // Russian roulette termination after bounce 2
        if (bounce > 2)
        {
            float p = glm::max(throughput.x, glm::max(throughput.y, throughput.z));
            if (p < 0.001f) break;
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) > p) break;
            throughput /= p;
        }

        throughput *= albedo;
        origin = hitPos + normal * 0.001f;
        dir = RandomHemisphere(normal, rng);
    }

    return radiance;
}

glm::vec3 PathTraceProbe(
    const PTScene& pt,
    const std::vector<PTLight>& lights,
    const glm::vec3& probePos,
    const glm::vec3& direction,
    int samplesPerDir,
    int bounces,
    std::mt19937& rng)
{
    glm::vec3 total(0.0f);

    for (int s = 0; s < samplesPerDir; s++)
    {
        glm::vec3 sampleDir = (s == 0)
            ? direction
            : RandomHemisphere(direction, rng);

        total += TracePath(pt, lights, probePos, sampleDir, bounces, rng);
    }

    return total / (float)samplesPerDir;
}

