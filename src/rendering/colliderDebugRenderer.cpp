#include "colliderDebugRenderer.h"
#include "shaderutils.h"

static const char* debugVertSrc = R"(
#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 viewProjection;
void main() {
    vColor = aColor;
    gl_Position = viewProjection * vec4(aPos, 1.0);
}
)";

static const char* debugFragSrc = R"(
#version 430 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

void ColliderDebugRenderer::Init()
{
    // Compile shader
    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &debugVertSrc, nullptr);
    glCompileShader(vert);

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &debugFragSrc, nullptr);
    glCompileShader(frag);

    shader = glCreateProgram();
    glAttachShader(shader, vert);
    glAttachShader(shader, frag);
    glLinkProgram(shader);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void ColliderDebugRenderer::AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color)
{
    vertices.push_back({ a, color });
    vertices.push_back({ b, color });
}

void ColliderDebugRenderer::AddBox(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& half, const glm::vec3& color)
{
    // 8 corners in local space
    glm::vec3 corners[8] = {
        {-half.x, -half.y, -half.z}, { half.x, -half.y, -half.z},
        { half.x,  half.y, -half.z}, {-half.x,  half.y, -half.z},
        {-half.x, -half.y,  half.z}, { half.x, -half.y,  half.z},
        { half.x,  half.y,  half.z}, {-half.x,  half.y,  half.z}
    };

    // Transform to world space
    for (auto& c : corners)
        c = pos + rot * c;

    // 12 edges
    int edges[] = { 0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7 };
    for (int i = 0; i < 24; i += 2)
        AddLine(corners[edges[i]], corners[edges[i+1]], color);
}

void ColliderDebugRenderer::AddSphere(const glm::vec3& pos, float radius, const glm::vec3& color, int seg)
{
    // Three orthogonal circles
    for (int axis = 0; axis < 3; axis++)
    {
        for (int i = 0; i < seg; i++)
        {
            float a1 = glm::two_pi<float>() * i / seg;
            float a2 = glm::two_pi<float>() * (i + 1) / seg;
            glm::vec3 p1, p2;

            if (axis == 0)      { p1 = {0, cos(a1), sin(a1)}; p2 = {0, cos(a2), sin(a2)}; }
            else if (axis == 1) { p1 = {cos(a1), 0, sin(a1)}; p2 = {cos(a2), 0, sin(a2)}; }
            else                { p1 = {cos(a1), sin(a1), 0}; p2 = {cos(a2), sin(a2), 0}; }

            AddLine(pos + p1 * radius, pos + p2 * radius, color);
        }
    }
}

void ColliderDebugRenderer::AddCapsule(const glm::vec3& pos, const glm::quat& rot,
    float halfHeight, float radius, const glm::vec3& color, int seg)
{
    glm::vec3 up = rot * glm::vec3(0, 1, 0);
    glm::vec3 top = pos + up * halfHeight;
    glm::vec3 bot = pos - up * halfHeight;

    // Vertical lines
    glm::vec3 right = rot * glm::vec3(1, 0, 0);
    glm::vec3 fwd   = rot * glm::vec3(0, 0, 1);
    AddLine(top + right * radius, bot + right * radius, color);
    AddLine(top - right * radius, bot - right * radius, color);
    AddLine(top + fwd * radius,   bot + fwd * radius, color);
    AddLine(top - fwd * radius,   bot - fwd * radius, color);

    // Rings at top and bottom
    for (int i = 0; i < seg; i++)
    {
        float a1 = glm::two_pi<float>() * i / seg;
        float a2 = glm::two_pi<float>() * (i + 1) / seg;
        glm::vec3 p1 = right * cos(a1) * radius + fwd * sin(a1) * radius;
        glm::vec3 p2 = right * cos(a2) * radius + fwd * sin(a2) * radius;
        AddLine(top + p1, top + p2, color);
        AddLine(bot + p1, bot + p2, color);
    }

    // Hemisphere arcs on top and bottom
    for (int i = 0; i < seg / 2; i++)
    {
        float a1 = glm::half_pi<float>() * i / (seg / 2);
        float a2 = glm::half_pi<float>() * (i + 1) / (seg / 2);

        // Top hemisphere (two arcs)
        glm::vec3 t1r = top + up * sin(a1) * radius + right * cos(a1) * radius;
        glm::vec3 t2r = top + up * sin(a2) * radius + right * cos(a2) * radius;
        AddLine(t1r, t2r, color);

        glm::vec3 t1f = top + up * sin(a1) * radius + fwd * cos(a1) * radius;
        glm::vec3 t2f = top + up * sin(a2) * radius + fwd * cos(a2) * radius;
        AddLine(t1f, t2f, color);

        // Bottom hemisphere
        glm::vec3 b1r = bot - up * sin(a1) * radius + right * cos(a1) * radius;
        glm::vec3 b2r = bot - up * sin(a2) * radius + right * cos(a2) * radius;
        AddLine(b1r, b2r, color);

        glm::vec3 b1f = bot - up * sin(a1) * radius + fwd * cos(a1) * radius;
        glm::vec3 b2f = bot - up * sin(a2) * radius + fwd * cos(a2) * radius;
        AddLine(b1f, b2f, color);
    }
}

void ColliderDebugRenderer::Render(const glm::mat4& view, const glm::mat4& projection)
{
    if (vertices.empty()) return;

    glUseProgram(shader);
    glm::mat4 vp = projection * view;
    glUniformMatrix4fv(glGetUniformLocation(shader, "viewProjection"), 1, GL_FALSE, glm::value_ptr(vp));

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, static_cast<int>(vertices.size()));
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    vertices.clear();
}