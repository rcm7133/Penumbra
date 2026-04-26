#pragma once
#include "../config.h"

namespace ShaderUtils
{
    std::string ReadFileWithIncludes(const std::string& filePath);
    unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType);
    unsigned int MakeShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    unsigned int MakeShaderProgram(const std::string& vertexPath,
                                const std::string& geometryPath,
                                const std::string& fragmentPath);
    unsigned int LoadTexture(const std::string& path);
    unsigned int LoadComputeShader(const std::string& path);

}