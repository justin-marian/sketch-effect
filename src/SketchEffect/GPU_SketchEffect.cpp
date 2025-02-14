#include "GPU_SketchEffect.h"

#include <iostream>
#include <glm/gtc/type_ptr.hpp>

using namespace std;


GPU_SketchEffect::GPU_SketchEffect(
    glm::ivec2& resolutionRef,
    unordered_map<string, GLuint>& framebuffersRef,
    unordered_map<string, GLuint>& texturesRef,
    unordered_map<string, Shader*>& shadersRef,
    unordered_map<string, Mesh*>& meshesRef)
    : resolution(resolutionRef),
    framebuffers(framebuffersRef),
    textures(texturesRef),
    shaders(shadersRef),
    meshes(meshesRef) {
}

GPU_SketchEffect::~GPU_SketchEffect() {}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: RenderOriginal
// Description: Render the original image to the screen using the specified shader.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPU_SketchEffect::RenderOriginal(
    const string& fboName,
    const string& textureName,
    const string& shaderName,
    const glm::mat4& modelMatrix,
    int flipVertical)
{
    if (framebuffers.find(fboName) == framebuffers.end() ||
        shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Missing framebuffer or shader." << endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[fboName]);

    auto shader = shaders[shaderName];
    shader->Use();

    glUniform1i(shader->GetUniformLocation("flipVertical"), flipVertical);
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[textureName]);

    RenderMesh(meshes["quad"], shader, modelMatrix);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: EdgeBinarize
// Description: Apply edge detection and binarization to the original texture, based on the Sobel operator.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPU_SketchEffect::EdgeBinarize(
    const string& fboName,
    const string& inputTextureName,
    const string& shaderName,
    float threshold)
{
    if (framebuffers.find(fboName) == framebuffers.end() || 
        shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Missing framebuffer or shader." << endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[fboName]);

    auto shader = shaders[shaderName];
    shader->Use();

    glUniform1f(shader->GetUniformLocation("thresholdSobel"), threshold);
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[inputTextureName]);

    RenderMesh(meshes["quad"], shader, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Horizontal
// Description: Apply horizontal gaussian blur to the input texture, based on gaussian distribution.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPU_SketchEffect::Horizontal(
    const string& fboName,
    const string& textureName,
    const string& shaderName,
    int radiusSize,
    float sigma)
{
    if (framebuffers.find(fboName) == framebuffers.end() || 
        shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Missing framebuffer or shader." << endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[fboName]);

    auto shader = shaders[shaderName];
    shader->Use();

    glUniform1i(shader->GetUniformLocation("radius"), radiusSize);
    glUniform1f(shader->GetUniformLocation("sigma"), sigma);
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[textureName]);

    RenderMesh(meshes["quad"], shader, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Vertical
// Description: Apply vertical gaussian blur to the input texture, based on gaussian distribution.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPU_SketchEffect::Vertical(
    const string& fboName,
    const string& textureName,
    const string& shaderName,
    int radiusSize,
    float sigma)
{
    if (framebuffers.find(fboName) == framebuffers.end() || 
        shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Missing framebuffer or shader." << endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[fboName]);

    auto shader = shaders[shaderName];
    shader->Use();

    glUniform1i(shader->GetUniformLocation("radius"), radiusSize);
    glUniform1f(shader->GetUniformLocation("sigma"), sigma);
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[textureName]);

    RenderMesh(meshes["quad"], shader, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Hatching
// Description: Apply hatching effect to the input texture (it adds cross-hatching lines to the image).
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPU_SketchEffect::Hatching(
    const string& fboName,
    const string& inputTextureName,
    const string& shaderName,
    const glm::vec3& hatchParams,
    float hatchTreshold,
    int hatchIndex,
    bool invertBackground)
{
    if (framebuffers.find(fboName) == framebuffers.end() ||
        shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Missing framebuffer or shader." << endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[fboName]);

    auto shader = shaders[shaderName];
    shader->Use();

    string hatchParamsU;
    string hatchThresholdU;

    switch (hatchIndex)
    {
    case 1:
        hatchParamsU = "hatch1Params";
        hatchThresholdU = "hatch1Threshold";
        break;
    case 2:
        hatchParamsU = "hatch2Params";
        hatchThresholdU = "hatch2Threshold";
        break;
    case 3:
        hatchParamsU = "hatch3Params";
        hatchThresholdU = "hatch3Threshold";
        break;
    default:
        cerr << "[Error]: Invalid hatch index " << hatchIndex << "." << endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glUniform3fv(shader->GetUniformLocation(hatchParamsU.c_str()), 1, glm::value_ptr(hatchParams));
    glUniform1f(shader->GetUniformLocation(hatchThresholdU.c_str()), hatchTreshold);
    glUniform1i(shader->GetUniformLocation("invertBackground"), invertBackground);
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[inputTextureName]);

    RenderMesh(meshes["quad"], shader, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Combine
// Description: Combine multiple textures using the specified shader.
// The textures are combined in the order they are passed in the textureNames vector.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPU_SketchEffect::Combine(
    const string& fboName,
    const string& shaderName,
    const vector<string>& textureNames)
{
    if (framebuffers.find(fboName) == framebuffers.end() || 
        shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Missing framebuffer or shader." << endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[fboName]);

    auto shader = shaders[shaderName];
    shader->Use();

    for (size_t i = 0; i < textureNames.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        auto textureIt = textures.find(textureNames[i]);
        if (textureIt != textures.end())
        {
            glBindTexture(GL_TEXTURE_2D, textureIt->second);
            glUniform1i(shader->GetUniformLocation(textureNames[i].c_str()), i);
        }
        else
        {
            cerr << "[Error]: Missing texture " << textureNames[i] << endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
    }
    
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    RenderMesh(meshes["quad"], shader, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
