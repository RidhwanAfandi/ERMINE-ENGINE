/* Start Header ************************************************************************/
/*!
\file       Shader.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (70%)
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu (30%)
\date       09/03/2025
\brief      This file contains the declaration of the Shader class.
            It will Compile, link and bind the shader to the program.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

#include "Matrix3x3.h"
#include "Matrix4x4.h"

namespace Ermine::graphics
{
    /**
     * @brief Shader class with uniform support for materials
     */
    class Shader
    {
        GLuint m_RendererID = 0;
        std::unordered_map<std::string, GLint> m_UniformLocationCache;
        enum class ShaderKind
        {
            None,
            Compute,
            VertexFragment,
            VertexGeometryFragment
        };
        ShaderKind m_ShaderKind = ShaderKind::None;
        std::string m_ComputePath;
        std::string m_VertexPath;
        std::string m_GeometryPath;
        std::string m_FragmentPath;

        /**
         * @brief Get the uniform location of the shader
         * @param name The name of the uniform
         * @return The location of the uniform
         */
        GLint GetUniformLocation(const std::string& name);
        /**
         * @brief Compile the shader
         * @param type The type of the shader
         * @param source The source of the shader
         * @return The shader ID
         */
        GLuint CompileShader(GLenum type, const std::string& source);
        /**
         * @brief Rebuilds the program from stored shader paths.
         * @param outProgram Newly linked program on success.
         * @return true when a new program was created successfully.
         */
        bool BuildProgram(GLuint& outProgram);
        /**
         * @brief Load the shader source
         * @param filepath The path of the shader
         * @return The source of the shader
         */
        std::string LoadShaderSource(const std::string& filepath);
    public:
        Shader() = default;

        /**
         * @brief Create a shader
         * @param vertexPath The path of the vertex shader
         * @param fragmentPath The path of the fragment shader
         */
        Shader(const std::string& computePath);

        /**
         * @brief Create a shader
         * @param vertexPath The path of the vertex shader
         * @param fragmentPath The path of the fragment shader
         */
        Shader(const std::string& vertexPath, const std::string& fragmentPath);

        /**
         * @brief Create a shader
         * @param vertexPath The path of the vertex shader
         * @param geometryPath The path of the geometry shader
         * @param fragmentPath The path of the fragment shader
         */
        Shader(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath);

        /**
         * @brief Destroy the shader
         */
        ~Shader();

        /**
         * @brief Check if the shader is valid
         * @return True if the shader is valid, false otherwise
         */
        bool IsValid() const;
        /**
         * @brief Reload the shader from its original source files.
         * @return True if recompilation succeeded, false otherwise.
         */
        bool Reload();

        /**
        * @brief Bind the shader
        */
        void Bind() const;

        /**
         * @brief Unbind the shader
         */
        void Unbind() const;

        /**
		 * @brief Set unsigned integer uniform
         * @param name The name of the uniform
         * @param value The value to set
         */
        void SetUniform1ui(const std::string& name, unsigned int value);

        /**
         * @brief Set integer uniform
         * @param name The name of the uniform
         * @param value The value to set
         */
        void SetUniform1i(const std::string& name, int value);

        /**
         * @brief Set integer vec2 uniform
         * @param name The name of the uniform
         * @param x First component
         * @param y Second component
         */
        void SetUniform2i(const std::string& name, int x, int y);

        /**
         * @brief Set integer vec3 uniform
         * @param name The name of the uniform
         * @param x First component
         * @param y Second component
         * @param z Third component
         */
        void SetUniform3i(const std::string& name, int x, int y, int z);

        /**
         * @brief Set integer vec4 uniform
         * @param name The name of the uniform
         * @param x First component
         * @param y Second component
         * @param z Third component
         * @param w Fourth component
         */
        void SetUniform4i(const std::string& name, int x, int y, int z, int w);

        /**
         * @brief Set float uniform
         * @param name The name of the uniform
         * @param value The value to set
         */
        void SetUniform1f(const std::string& name, float value);

        /**
         * @brief Set float vec2 uniform
         * @param name The name of the uniform
         * @param x First component
         * @param y Second component
         */
        void SetUniform2f(const std::string& name, float x, float y);

        /**
         * @brief Set float vec3 uniform
         * @param name The name of the uniform
         * @param x First component
         * @param y Second component
         * @param z Third component
         */
        void SetUniform3f(const std::string& name, float x, float y, float z);

        /**
         * @brief Set float vec4 uniform
         * @param name The name of the uniform
         * @param x First component
         * @param y Second component
         * @param z Third component
         * @param w Fourth component
         */
        void SetUniform4f(const std::string& name, float x, float y, float z, float w);

        /**
         * @brief Set vec2 uniform
         * @param name The name of the uniform
         * @param value The vec2 value to set
         */
        void SetUniform2f(const std::string& name, const glm::vec2& value);

        /**
         * @brief Set vec3 uniform
         * @param name The name of the uniform
         * @param value The vec3 value to set
         */
        void SetUniform3f(const std::string& name, const glm::vec3& value);

        /**
         * @brief Set vec4 uniform
         * @param name The name of the uniform
         * @param value The vec4 value to set
         */
        void SetUniform4f(const std::string& name, const glm::vec4& value);

        /**
         * @brief Set float array uniform
         * @param name The name of the uniform
         * @param count Number of elements in the array
         * @param value Pointer to the array data
         */
        void SetUniform1fv(const std::string& name, GLsizei count, const float* value);

        /**
         * @brief Set vec2 array uniform
         * @param name The name of the uniform
         * @param count Number of vec2 elements in the array
         * @param value Pointer to the array data
         */
        void SetUniform2fv(const std::string& name, GLsizei count, const float* value);

        /**
         * @brief Set vec3 array uniform
         * @param name The name of the uniform
         * @param count Number of vec3 elements in the array
         * @param value Pointer to the array data
         */
        void SetUniform3fv(const std::string& name, GLsizei count, const float* value);

        /**
         * @brief Set vec4 array uniform
         * @param name The name of the uniform
         * @param count Number of vec4 elements in the array
         * @param value Pointer to the array data
         */
        void SetUniform4fv(const std::string& name, GLsizei count, const float* value);

        /**
         * @brief Set integer array uniform
         * @param name The name of the uniform
         * @param count Number of elements in the array
         * @param value Pointer to the array data
         */
        void SetUniform1iv(const std::string& name, GLsizei count, const int* value);

        /**
         * @brief Set mat2 uniform
         * @param name The name of the uniform
         * @param value The mat2 value to set
         */
        void SetUniformMatrix2fv(const std::string& name, const glm::mat2& value);

        /**
         * @brief Set mat3 uniform (custom matrix type)
         * @param name The name of the uniform
         * @param value The mat3 value to set
         */
        void SetUniformMatrix3fv(const std::string& name, const Mtx33& value);

        /**
         * @brief Set mat3 uniform (GLM)
         * @param name The name of the uniform
         * @param value The mat3 value to set
         */
        void SetUniformMatrix3fv(const std::string& name, const glm::mat3& value);

        /**
         * @brief Set mat4 uniform (custom matrix type)
         * @param name The name of the uniform
         * @param matrix The mat4 value to set
         */
        void SetUniformMatrix4fv(const std::string& name, const Mtx44& matrix);

        /**
         * @brief Set mat4 uniform (GLM)
         * @param name The name of the uniform
         * @param matrix The mat4 value to set
         */
        void SetUniformMatrix4fv(const std::string& name, const glm::mat4& matrix);

        /**
         * @brief Set mat4 uniform from raw float pointer
         * @param name The name of the uniform
         * @param matrix Pointer to 16 floats representing the matrix
         */
        void SetUniformMatrix4fv(const std::string& name, const float* matrix);

        /**
         * @brief Set boolean uniform (converted to int)
         * @param name The name of the uniform
         * @param value The boolean value to set
         */
        void SetUniformBool(const std::string& name, bool value);

        /**
         * @brief Get the renderer ID
         * @return The OpenGL program ID
         */
        GLuint GetRendererID() const;

        /**
         * @brief Check if a uniform exists in the shader
         * @param name The name of the uniform to check
         * @return True if the uniform exists, false otherwise
         */
        bool HasUniform(const std::string& name);

        /**
         * @brief Get all active uniform names from the shader
         * @return Vector of uniform names
         */
        std::vector<std::string> GetActiveUniforms() const;

        /**
         * @brief Print all active uniforms (debug helper)
         */
        void PrintActiveUniforms() const;
    };
}
