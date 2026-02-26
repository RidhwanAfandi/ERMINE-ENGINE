/* Start Header ************************************************************************/
/*!
\file       Shader.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (70%)
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu (30%)
\date       09/03/2025
\brief      This file contains the definition of the Shader system.
            This file is used to load the shader files.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Shader.h"

#include "Matrix3x3.h"
#include "glad/glad.h"

using namespace Ermine::graphics;

/**
 * @brief Get the uniform location of the shader
 * @param name The name of the uniform
 * @return The location of the uniform
 */
GLint Shader::GetUniformLocation(const std::string& name)
{
    if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
        return m_UniformLocationCache[name];

    if (m_RendererID == 0)
    {
        EE_CORE_ERROR("Shader program ID  is invalid!");
        return -1;
    }

    GLint location = glGetUniformLocation(m_RendererID, name.c_str());
    if (location == -1)
        EE_CORE_WARN("Uniform {0} not found in shader", name);

    m_UniformLocationCache[name] = location;
    return location;
}

/**
 * @brief Compile the shader
 * @param type The type of the shader
 * @param source The source of the shader
 * @return The shader ID
 */
GLuint Shader::CompileShader(GLenum type, const std::string& source)
{
    // EE_CORE_TRACE("Compiling shader: {0}", source);
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

        glDeleteShader(shader);

        EE_CORE_ERROR("Shader compilation failed: {0}", infoLog.data());
        return 0;
    }
    
    return shader;
}

/**
 * @brief Load the shader source
 * @param filepath The path of the shader
 * @return The source of the shader
 */
std::string Shader::LoadShaderSource(const std::string& filepath)
{
    std::ifstream file(filepath);
    
    if (!file.is_open())
    {
        EE_CORE_ERROR("Failed to open shader file: {0}", filepath);
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (file.bad())
    {
        EE_CORE_ERROR("Error occurred while reading shader file: {0}", filepath);
        return "";
    }

    file.close();
    return buffer.str();
}

/**
 * @brief Create a shader
 * @param vertexPath The path of the vertex shader
 * @param fragmentPath The path of the fragment shader
 */
Shader::Shader(const std::string& computePath)
{
    m_ShaderKind = ShaderKind::Compute;
    m_ComputePath = computePath;
    Reload();
}

/**
 * @brief Create a shader
 * @param vertexPath The path of the vertex shader
 * @param fragmentPath The path of the fragment shader
 */
Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    m_ShaderKind = ShaderKind::VertexFragment;
    m_VertexPath = vertexPath;
    m_FragmentPath = fragmentPath;
    Reload();
}


/**
 * @brief Create a shader
 * @param vertexPath The path of the vertex shader
 * @param geometryPath The path of the geometry shader
 * @param fragmentPath The path of the fragment shader
 */
Shader::Shader(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath)
{
    m_ShaderKind = ShaderKind::VertexGeometryFragment;
    m_VertexPath = vertexPath;
    m_GeometryPath = geometryPath;
    m_FragmentPath = fragmentPath;
    Reload();
}


/**
 * @brief Destroy the shader
 */
Shader::~Shader()
{
    glDeleteProgram(m_RendererID);
}

/**
 * @brief Check if the shader is valid
 * @return True if the shader is valid, false otherwise
 */
bool Shader::IsValid() const
{
    return m_RendererID != 0;
}

bool Shader::BuildProgram(GLuint& outProgram)
{
    outProgram = 0;

    if (m_ShaderKind == ShaderKind::Compute)
    {
        std::string computeSource = LoadShaderSource(m_ComputePath);
        GLuint computeShader = CompileShader(GL_COMPUTE_SHADER, computeSource);
        if (computeShader == 0)
        {
            return false;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, computeShader);
        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
            glDeleteProgram(program);
            glDeleteShader(computeShader);
            EE_CORE_ERROR("Shader linking failed: {0}", infoLog.data());
            return false;
        }

        glDeleteShader(computeShader);
        outProgram = program;
        return true;
    }

    if (m_ShaderKind == ShaderKind::VertexFragment)
    {
        std::string vertexSource = LoadShaderSource(m_VertexPath);
        std::string fragmentSource = LoadShaderSource(m_FragmentPath);

        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (vertexShader == 0 || fragmentShader == 0)
        {
            if (vertexShader != 0) glDeleteShader(vertexShader);
            if (fragmentShader != 0) glDeleteShader(fragmentShader);
            return false;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
            glDeleteProgram(program);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            EE_CORE_ERROR("Shader linking failed: {0}", infoLog.data());
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        outProgram = program;
        return true;
    }

    if (m_ShaderKind == ShaderKind::VertexGeometryFragment)
    {
        std::string vertexSource = LoadShaderSource(m_VertexPath);
        std::string geometrySource = LoadShaderSource(m_GeometryPath);
        std::string fragmentSource = LoadShaderSource(m_FragmentPath);

        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint geometryShader = CompileShader(GL_GEOMETRY_SHADER, geometrySource);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (vertexShader == 0 || geometryShader == 0 || fragmentShader == 0)
        {
            if (vertexShader != 0) glDeleteShader(vertexShader);
            if (geometryShader != 0) glDeleteShader(geometryShader);
            if (fragmentShader != 0) glDeleteShader(fragmentShader);
            return false;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, geometryShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
            glDeleteProgram(program);
            glDeleteShader(vertexShader);
            glDeleteShader(geometryShader);
            glDeleteShader(fragmentShader);
            EE_CORE_ERROR("Shader linking failed: {0}", infoLog.data());
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteShader(fragmentShader);
        outProgram = program;
        return true;
    }

    return false;
}

bool Shader::Reload()
{
    GLuint newProgram = 0;
    if (!BuildProgram(newProgram))
    {
        return false;
    }

    if (m_RendererID != 0)
    {
        glDeleteProgram(m_RendererID);
    }
    m_RendererID = newProgram;
    m_UniformLocationCache.clear();
    return true;
}

/**
 * @brief Bind the shader
 */
void Shader::Bind() const
{
    glUseProgram(m_RendererID);
}

/**
 * @brief Unbind the shader
 */
void Shader::Unbind() const
{
    glUseProgram(0);
}

void Shader::SetUniform1ui(const std::string& name, unsigned int value)
{
	glUniform1ui(GetUniformLocation(name), value);
}

/**
 * @brief Set the uniform value of the shader
 * @param name The name of the uniform
 * @param value The value to set
 */
void Shader::SetUniform1i(const std::string& name, int value)
{
    glUniform1i(GetUniformLocation(name), value);
}

/**
 * @brief Sets a 2-component integer uniform in the shader.
 * @param name The uniform name.
 * @param x The first integer component.
 * @param y The second integer component.
 */
void Shader::SetUniform2i(const std::string& name, int x, int y)
{
    glUniform2i(GetUniformLocation(name), x, y);
}

/**
 * @brief Sets a 3-component integer uniform in the shader.
 * @param name The uniform name.
 * @param x The first integer component.
 * @param y The second integer component.
 * @param z The third integer component.
 */
void Shader::SetUniform3i(const std::string& name, int x, int y, int z)
{
    glUniform3i(GetUniformLocation(name), x, y, z);
}

/**
 * @brief Sets a 4-component integer uniform in the shader.
 * @param name The uniform name.
 * @param x The first integer component.
 * @param y The second integer component.
 * @param z The third integer component.
 * @param w The fourth integer component.
 */
void Shader::SetUniform4i(const std::string& name, int x, int y, int z, int w)
{
    glUniform4i(GetUniformLocation(name), x, y, z, w);
}

/**
 * @brief Set the uniform value of the shader
 * @param name The name of the uniform
 * @param value The value to set
 */
void Shader::SetUniform1f(const std::string& name, float value)
{
    glUniform1f(GetUniformLocation(name), value);
}

/**
 * @brief Sets a 2-component float uniform in the shader.
 * @param name The uniform name.
 * @param x The first float component.
 * @param y The second float component.
 */
void Shader::SetUniform2f(const std::string& name, float x, float y)
{
    glUniform2f(GetUniformLocation(name), x, y);
}

/**
 * @brief Set float vec3 uniform
 * @param name The name of the uniform
 * @param x First component
 * @param y Second component
 * @param z Third component
 */
void Shader::SetUniform3f(const std::string& name, float x, float y, float z)
{
	glUniform3f(GetUniformLocation(name), x, y, z);
}

/**
 * @brief Set float vec4 uniform
 * @param name The name of the uniform
 * @param x First component
 * @param y Second component
 * @param z Third component
 * @param w Fourth component
 */
void Shader::SetUniform4f(const std::string& name, float x, float y, float z, float w)
{
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

/**
 * @brief Sets a 2-component float uniform in the shader.
 * @param name The uniform name.
 * @param value The `glm::vec2` containing the float components.
 */
void Shader::SetUniform2f(const std::string& name, const glm::vec2& value)
{
    glUniform2f(GetUniformLocation(name), value.x, value.y);
}

/**
 * @brief Set the uniform value of the shader
 * @param name The name of the uniform
 * @param value The value to set
 */
void Shader::SetUniform3f(const std::string& name, const glm::vec3& value)
{
    glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
}

/**
 * @brief Set the uniform value of the shader
 * @param name The name of the uniform
 * @param value The value to set
 */
void Shader::SetUniform4f(const std::string& name, const glm::vec4& value)
{
    glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
}

/**
 * @brief Sets a 2-component float uniform in the shader.
 * @param name The uniform name.
 * @param count The number of elements.
 * @param value Pointer to the float values.
 */
void Shader::SetUniformMatrix2fv(const std::string& name, const glm::mat2& value)
{
    glUniformMatrix2fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

/**
 * @brief Set the uniform value of the shader
 * @param name The name of the uniform
 * @param value The value to set
 */
void Shader::SetUniformMatrix3fv(const std::string& name, const Mtx33& value)
{
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, &value.m2[0][0]);
}

/**
 * @brief Set the uniform value of the shader
 * @param name The name of the uniform
 * @param matrix The value to set
 */
void Shader::SetUniformMatrix4fv(const std::string& name, const Mtx44& matrix)
{
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &matrix.m2[0][0]);
}

/**
 * @brief Get the renderer ID of the shader
 * @return The renderer ID of the shader
 */
GLuint Shader::GetRendererID() const
{
    return m_RendererID;
}

/**
 * @brief Sets a 1-component float uniform in the shader.
 * @param name The uniform name.
 * @param count The number of elements.
 * @param value Pointer to the float values.
 */
void Shader::SetUniform1fv(const std::string& name, GLsizei count, const float* value)
{
    glUniform1fv(GetUniformLocation(name), count, value);
}

/**
 * @brief Sets a 2-component float uniform in the shader.
 * @param name The uniform name.
 * @param count The number of elements.
 * @param value Pointer to the float values.
 */
void Shader::SetUniform2fv(const std::string& name, GLsizei count, const float* value)
{
    glUniform2fv(GetUniformLocation(name), count, value);
}

/**
 * @brief Sets a 3-component float uniform in the shader.
 * @param name The uniform name.
 * @param count The number of elements.
 * @param value Pointer to the float values.
 */
void Shader::SetUniform3fv(const std::string& name, GLsizei count, const float* value)
{
    glUniform3fv(GetUniformLocation(name), count, value);
}

/**
 * @brief Sets a 4-component float uniform in the shader.
 * @param name The uniform name.
 * @param count The number of elements.
 * @param value Pointer to the float values.
 */
void Shader::SetUniform4fv(const std::string& name, GLsizei count, const float* value)
{
    glUniform4fv(GetUniformLocation(name), count, value);
}

/**
 * @brief Sets a 1-component int uniform in the shader.
 * @param name The uniform name.
 * @param count The number of elements.
 * @param value Pointer to the int values.
 */
void Shader::SetUniform1iv(const std::string& name, GLsizei count, const int* value)
{
    glUniform1iv(GetUniformLocation(name), count, value);
}

/**
 * @brief Sets a boolean uniform in the shader.
 * @param name The uniform name.
 * @param value The boolean value (true/false).
 */
void Shader::SetUniformBool(const std::string& name, bool value)
{
    glUniform1i(GetUniformLocation(name), value ? 1 : 0);
}

/**
 * @brief Checks if a uniform exists in the shader.
 * @param name The uniform name.
 * @return `true` if uniform exists, `false` otherwise.
 */
bool Shader::HasUniform(const std::string& name)
{
    return GetUniformLocation(name) != -1;
}

/**
 * @brief Retrieves all active uniform names in the shader.
 * @return A vector of uniform names.
 */
std::vector<std::string> Shader::GetActiveUniforms() const
{
    std::vector<std::string> uniforms;

    if (m_RendererID == 0) return uniforms;

    GLint numUniforms;
    glGetProgramiv(m_RendererID, GL_ACTIVE_UNIFORMS, &numUniforms);

    GLint maxLength;
    glGetProgramiv(m_RendererID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);

    std::vector<GLchar> nameBuffer(maxLength);

    for (GLint i = 0; i < numUniforms; ++i)
    {
        GLsizei length;
        GLint size;
        GLenum type;

        glGetActiveUniform(m_RendererID, i, maxLength, &length, &size, &type, nameBuffer.data());

        uniforms.emplace_back(nameBuffer.data(), length);
    }

    return uniforms;
}

/**
 * @brief Prints active uniform names and their locations.
 * @note Logs a warning if the shader is invalid.
 */
void Shader::PrintActiveUniforms() const
{
    if (m_RendererID == 0)
    {
        EE_CORE_WARN("Cannot print uniforms: Invalid shader program");
        return;
    }

    auto uniforms = GetActiveUniforms();
    EE_CORE_INFO("Active uniforms for shader program {0}:", m_RendererID);

    for (const auto& uniform : uniforms)
    {
        GLint location = glGetUniformLocation(m_RendererID, uniform.c_str());
        EE_CORE_INFO("  {0} (location: {1})", uniform, location);
    }
}

/**
 * @brief Set mat3 uniform (GLM)
 * @param name The name of the uniform
 * @param value The mat3 value to set
 */
void Shader::SetUniformMatrix3fv(const std::string& name, const glm::mat3& value)
{
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

/**
 * @brief Set mat4 uniform (GLM)
 * @param name The name of the uniform
 * @param matrix The mat4 value to set
 */
void Shader::SetUniformMatrix4fv(const std::string& name, const glm::mat4& matrix)
{
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
}

/**
 * @brief Set mat4 uniform from raw float pointer
 * @param name The name of the uniform
 * @param matrix Pointer to 16 floats representing the matrix
 */
void Shader::SetUniformMatrix4fv(const std::string& name, const float* matrix)
{
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, matrix);
}
