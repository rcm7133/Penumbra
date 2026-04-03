#include "shaderutils.h"

namespace ShaderUtils {
    unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open shader file: " << filePath << std::endl;
            return 0;
        }

        std::stringstream ss;
        std::string line;
        while (std::getline(file, line))
            ss << line << "\n";

        std::string src = ss.str();
        const char* srcPtr = src.c_str();

        unsigned int shader = glCreateShader(moduleType);
        glShaderSource(shader, 1, &srcPtr, NULL);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[1024];
            glGetShaderInfoLog(shader, 1024, NULL, log);
            std::cerr << "Shader compilation error:\n" << log << std::endl;
        }
        return shader;
    }

    unsigned int MakeShaderProgram(const std::string& vertexPath, const std::string& fragmentPath)
    {
        unsigned int vert = MakeShaderModule(vertexPath, GL_VERTEX_SHADER);
        unsigned int frag = MakeShaderModule(fragmentPath, GL_FRAGMENT_SHADER);

        unsigned int program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);

        int success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char log[1024];
            glGetProgramInfoLog(program, 1024, NULL, log);
            std::cerr << "Program link error:\n" << log << std::endl;
        }

        glDeleteShader(vert);
        glDeleteShader(frag);
        return program;
    }

    unsigned int LoadComputeShader(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();
        const char* srcPtr = src.c_str();

        unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &srcPtr, NULL);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[1024];
            glGetShaderInfoLog(shader, 1024, NULL, log);
            std::cerr << "Compute shader error:\n" << log << std::endl;
        }

        unsigned int program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        glDeleteShader(shader);
        return program;
    }
}