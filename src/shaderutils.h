#pragma once
#include "config.h"

namespace ShaderUtils
{
    unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType);
    unsigned int MakeShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    unsigned int LoadComputeShader(const std::string& path);
}