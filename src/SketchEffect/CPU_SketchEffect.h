#pragma once

#ifndef CPU_SKETCHEFFECT_H
#define CPU_SKETCHEFFECT_H

#include "ThreadPool.h"
#include "components/simple_scene.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>

#include <glm/glm.hpp>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))


class CPU_SketchEffect : public gfxc::SimpleScene
{
public:
    CPU_SketchEffect(
        glm::ivec2& resolutionRef,
        std::unordered_map<std::string, GLuint>& framebuffersRef,
        std::unordered_map<std::string, GLuint>& texturesRef,
        std::unordered_map<std::string, Shader*>& shadersRef,
        std::unordered_map<std::string, Mesh*>& meshesRef,
        size_t threadCount = std::thread::hardware_concurrency());
    ~CPU_SketchEffect();

	// Render the original image to the screen using the specified shader.
    void RenderOriginal(
        const std::string& fboName,
        const std::string& textureName,
        const std::string& shaderName,
        const glm::mat4& modelMatrix,
        int flipVertical,
        glm::ivec2 resolution);
	// Apply edge detection and binarization to the original texture.
    void EdgeBinarize(
        const std::string& inputTextureName,
        const std::string& outputTextureName,
        glm::ivec2 resolution,
        float threshold,
        int startRow, int endRow);
	// Apply horizontal gaussian blur to the input texture.
    void Horizontal(
        const std::string& inputTextureName,
        const std::string& outputTextureName,
        glm::ivec2 resolution,
        int radius, float sigma,
        int startRow, int endRow);
	// Apply vertical gaussian blur to the input texture.
    void Vertical(
        const std::string& inputTextureName,
        const std::string& outputTextureName,
        glm::ivec2 resolution,
        int radius, float sigma,
        int startRow, int endRow);
	// Apply hatching to the input texture using the specified parameters.
    void Hatching(
        const std::string& inputTextureName,
        const std::string& outputTextureName,
        glm::ivec2 resolution, 
        glm::vec3 hatchParams, 
        float threshold, bool invertBackground);
	// Combine the input textures using the specified weights.
    void Combine(
        const std::vector<std::string>& inputTextureNames,
        const std::string& outputTextureName,
        glm::ivec2 resolution);

private:
	// Compute the weight of the pixel at the specified position.
    float Weight(int mu, float sigma) const;
	// Compute the gray nuance of the pixel at the specified index.
    float GrayNuance(const std::vector<unsigned char>& in, int index) const;

private:
    glm::ivec2& resolution;
    std::unordered_map<std::string, GLuint>& framebuffers;
    std::unordered_map<std::string, GLuint>& textures;
    std::unordered_map<std::string, Mesh*>& meshes;
    std::unordered_map<std::string, Shader*>& shaders;
    ThreadPool pool;
};

#endif // CPU_SKETCHEFFECT_H
