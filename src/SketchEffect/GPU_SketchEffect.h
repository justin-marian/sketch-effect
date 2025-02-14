#pragma once

#ifndef GPU_SKETCHEFFECT_H
#define GPU_SKETCHEFFECT_H

#include <string>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <unordered_map>

#include "components/simple_scene.h"
#include "core/gpu/frame_buffer.h"

#include <glm/glm.hpp>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))


class GPU_SketchEffect : public gfxc::SimpleScene
{
public:
    GPU_SketchEffect(
        glm::ivec2& resolutionRef,
        std::unordered_map<std::string, GLuint>& framebuffersRef,
        std::unordered_map<std::string, GLuint>& texturesRef,
        std::unordered_map<std::string, Shader*>& shadersRef,
        std::unordered_map<std::string, Mesh*>& meshesRef);
    ~GPU_SketchEffect();

	// Render the original image to the screen using the specified shader.
    void RenderOriginal(
        const std::string& fboName, 
        const std::string& textureName, 
        const std::string& shaderName, 
        const glm::mat4& modelMatrix, 
        int flipVertical);
	// Apply edge detection and binarization to the original texture.
    void EdgeBinarize(
        const std::string & fboName, 
        const std::string & textureName,
        const std::string& shaderName,
        float threshold);
	// Apply horizontal gaussian blur to the input texture.
    void Hatching(
        const std::string& fboName,
        const std::string& inputTextureName,
        const std::string& shaderName,
        const glm::vec3& hatchParams,
        float hatchTreshold, int hatchIndex,
        bool invertBackground);
	// Apply horizontal gaussian blur to the input texture.
    void Horizontal(
        const std::string& fboName, 
        const std::string& textureName, 
        const std::string& shaderName, 
        int radiusSize, float sigma);
	// Apply vertical gaussian blur to the input texture.
    void Vertical(
        const std::string& fboName, 
        const std::string& textureName, 
        const std::string& shaderName, 
        int radiusSize, float sigma);
	// Combine multiple textures using the specified shader.
    void Combine(
        const std::string& fboName,
        const std::string& shaderName,
        const std::vector<std::string>& textureNames);

private:
    glm::ivec2& resolution;
    std::unordered_map<std::string, GLuint>& framebuffers;
    std::unordered_map<std::string, GLuint>& textures;
    std::unordered_map<std::string, Mesh*>& meshes;
    std::unordered_map<std::string, Shader*>& shaders;
};

#endif // GPU_SKETCHEFFECT_H
