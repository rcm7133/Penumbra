#include "shaderutils.h"

namespace ShaderUtils {
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
        std::string src = ReadFileWithIncludes(path);
        if (src.empty()) return 0;

        const char* srcPtr = src.c_str();

        unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &srcPtr, NULL);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[1024];
            glGetShaderInfoLog(shader, 1024, NULL, log);
            std::cerr << "Compute shader error (" << path << "):\n" << log << std::endl;
        }

        unsigned int program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        glDeleteShader(shader);
        return program;
    }

    unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType)
    {
        std::string src = ReadFileWithIncludes(filePath);
        if (src.empty()) return 0;

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
            std::cerr << "Shader compilation error (" << filePath << "):\n" << log << std::endl;
        }
        return shader;
    }

    std::string ReadFileWithIncludes(const std::string& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open shader file: " << filePath << std::endl;
            return "";
        }

        // Get the directory of the current file for relative includes
        std::string dir = filePath.substr(0, filePath.find_last_of("/\\") + 1);

        std::stringstream result;
        std::string line;
        while (std::getline(file, line))
        {
            // Check for #include "filename"
            if (line.find("#include") == 0)
            {
                size_t start = line.find('"');
                size_t end = line.rfind('"');
                if (start != std::string::npos && end != start)
                {
                    std::string includePath = dir + line.substr(start + 1, end - start - 1);
                    std::string included = ReadFileWithIncludes(includePath); // recursive
                    result << included << "\n";
                }
                else
                {
                    std::cerr << "Malformed #include: " << line << std::endl;
                }
            }
            else
            {
                result << line << "\n";
            }
        }
        return result.str();
    }
}

