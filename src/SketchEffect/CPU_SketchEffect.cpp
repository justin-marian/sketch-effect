#include "CPU_SketchEffect.h"

#include <thread>
#include <iostream>

using namespace std;


CPU_SketchEffect::CPU_SketchEffect(
    glm::ivec2& resolutionRef,
    unordered_map<string, GLuint>& framebuffersRef,
    unordered_map<string, GLuint>& texturesRef,
    unordered_map<string, Shader*>& shadersRef,
    unordered_map<string, Mesh*>& meshesRef,
    size_t threadCount)
    : resolution(resolutionRef),
    framebuffers(framebuffersRef), textures(texturesRef),
    shaders(shadersRef), meshes(meshesRef),
    pool(threadCount) {}

CPU_SketchEffect::~CPU_SketchEffect() {}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: RenderOriginal
// Description: Renders the original input texture without applying any processing.
// Parameters:
//   - inputTextureName: Name of the input texture.
//   - outputTextureName: Name of the output texture.
//   - resolution: Resolution of the input texture.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPU_SketchEffect::RenderOriginal(
    const string& fboName,
    const string& textureName,
    const string& shaderName,
    const glm::mat4& modelMatrix,
    int flipVertical,
    glm::ivec2 resolution)
{
    if (framebuffers.find(fboName) == framebuffers.end())
    {
        cerr << "[Error]: Framebuffer '" << fboName << "' not found." << endl;
        return;
    }

    GLuint framebuffer = framebuffers.find(fboName)->second;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    if (shaders.find(shaderName) == shaders.end())
    {
        cerr << "[Error]: Shader '" << shaderName << "' not found." << endl;
        return;
    }

    auto shader = shaders.find(shaderName)->second;
    shader->Use();

    glUniform1i(shader->GetUniformLocation("flipVertical"), flipVertical);
    glUniform2i(shader->GetUniformLocation("screenSize"), resolution.x, resolution.y);

    GLuint texture = textures.find(textureName)->second;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures.find(textureName)->second);
    glUniform1i(shader->GetUniformLocation(textureName.c_str()), 0);

    RenderMesh(meshes["quad"], shader, modelMatrix);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: GrayNuance
// Description: Computes the gray nuance of a pixel.
// Parameters:
//   - in: Input pixel data.
//   - index: Index of the pixel data.
// Returns:
//   - The gray nuance of the pixel.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CPU_SketchEffect::GrayNuance(const vector<unsigned char>& in, int index) const
{
    float r = in[index + 0] / 255.0f;
    float g = in[index + 1] / 255.0f;
    float b = in[index + 2] / 255.0f;
    return 0.21f * r + 0.71f * g + 0.07f * b;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: EdgeBinarize
// Description: Applies an edge binarization effect to the input texture.
// Parameters:
//   - inputTextureName: Name of the input texture.
//   - outputTextureName: Name of the output texture.
//   - resolution: Resolution of the input texture.
//   - threshold: Threshold for the edge binarization effect.
//   - startRow: Start row for the processing.
//   - endRow: End row for the processing.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPU_SketchEffect::EdgeBinarize(
    const string& inputTextureName,
    const string& outputTextureName,
    glm::ivec2 resolution,
    float threshold,
    int startRow, int endRow)
{
    const int kernelSize = 3;
    const float Gx[kernelSize][kernelSize] =
    {
        {-1,  0,  1},
        {-2,  0,  2},
        {-1,  0,  1},
    };
    const float Gy[kernelSize][kernelSize] =
    {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1},
    };

    vector<unsigned char> in(resolution.x * resolution.y * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[inputTextureName]);
    glReadPixels(0, 0, resolution.x, resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, in.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    vector<unsigned char> out(resolution.x * resolution.y * 4, 0);

    auto SOBEL_BINARY_EDGE = [&](int start, int end)
    {
        for (int y = start; y < end; ++y)
        {
            for (int x = 0; x < resolution.x; ++x)
            {
                float gradX = 0.0f;
                float gradY = 0.0f;

                for (int j = -1; j <= 1; ++j)
                {
                    for (int i = -1; i <= 1; ++i)
                    {
                        int nx = glm::clamp(x + i, 0, resolution.x - 1);
                        int ny = glm::clamp(y + j, 0, resolution.y - 1);
                        int index = (ny * resolution.x + nx) * 4;

						float gray = GrayNuance(in, index);

                        gradX += gray * Gx[j + 1][i + 1];
                        gradY += gray * Gy[j + 1][i + 1];
                    }
                }

                float magnitude = sqrt(gradX * gradX + gradY * gradY);
                unsigned char binary = (magnitude >= threshold) ? 0 : 255;

                int idx = (y * resolution.x + x) * 4;
                out[idx + 0] = binary;
                out[idx + 1] = binary;
                out[idx + 2] = binary;
                out[idx + 3] = 255;
            }
        }
    };

	// Multithreaded applied on the rows !(better performance)
    int rows = (endRow - startRow) / pool.workers.size();
    for (int t = 0; t < pool.workers.size(); ++t)
    {
        int start = startRow + t * rows;
        int end = (t == pool.workers.size() - 1) ? endRow : start + rows;
        pool.Add_Task([=] { SOBEL_BINARY_EDGE(start, end); }, "SOBEL_BINARY_EDGE");
    }

    pool.Free_Resource();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[outputTextureName]);
    glBindTexture(GL_TEXTURE_2D, textures[outputTextureName]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Weight
// Description: Computes the weight of a Gaussian kernel.
// Parameters:
//   - mu: Distance from the center of the kernel.
//   - sigma: Standard deviation of the Gaussian kernel.
// Returns:
//   - The weight of the Gaussian kernel.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CPU_SketchEffect::Weight(int mu, float sigma) const
{
    return exp(-float(mu * mu) / (2.0f * sigma * sigma)) / (sqrt(2.0f * glm::pi<float>()) * sigma);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Horizontal
// Description: Applies a horizontal blur effect to the input texture.
// Parameters:
//   - inputTextureName: Name of the input texture.
//   - outputTextureName: Name of the output texture.
//   - resolution: Resolution of the input texture.
//   - radius: Radius of the Gaussian kernel.
//   - sigma: Standard deviation of the Gaussian kernel.
//   - startRow: Start row for the processing.
//   - endRow: End row for the processing.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPU_SketchEffect::Horizontal(
    const string& inputTextureName,
    const string& outputTextureName,
    glm::ivec2 resolution,
    int radius, float sigma,
    int startRow, int endRow)
{
    vector<unsigned char> in(resolution.x * resolution.y * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[inputTextureName]);
    glReadPixels(0, 0, resolution.x, resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, in.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    vector<unsigned char> out(resolution.x * resolution.y * 4, 0);
    vector<float> weights(2 * radius + 1);
    float sum = 0.0f;

    for (int i = -radius; i <= radius; ++i)
    {
        weights[i + radius] = Weight(i, sigma);
        sum += weights[i + radius];
    }

    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= sum;
    }

    auto HORIZONTAL_BLUR = [&](int start, int end)
    {
        for (int y = start; y < end; ++y)
        {
            for (int x = 0; x < resolution.x; ++x)
            {
                float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;

                for (int i = -radius; i <= radius; ++i)
                {
                    int nx = glm::clamp(x + i, 0, resolution.x - 1);
                    int index = (y * resolution.x + nx) * 4;

                    float r = in[index] / 255.0f;
                    float g = in[index + 1] / 255.0f;
                    float b = in[index + 2] / 255.0f;

                    float weight = weights[i + radius];
                    sumR += r * weight;
                    sumG += g * weight;
                    sumB += b * weight;
                }

                int idx = (y * resolution.x + x) * 4;
                out[idx + 0] = static_cast<unsigned char>(sumR * 255.0f);
                out[idx + 1] = static_cast<unsigned char>(sumG * 255.0f);
                out[idx + 2] = static_cast<unsigned char>(sumB * 255.0f);
                out[idx + 3] = 255;
            }
        }
    };

	// Multithreaded applied on the rows
    int rows = (endRow - startRow) / pool.workers.size();
    for (int t = 0; t < pool.workers.size(); ++t)
    {
        int start = startRow + t * rows;
        int end = (t == pool.workers.size() - 1) ? endRow : start + rows;
        pool.Add_Task([=] { HORIZONTAL_BLUR(start, end); }, "HORIZONTAL_BLUR");
    }

    pool.Free_Resource();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[outputTextureName]);
    glBindTexture(GL_TEXTURE_2D, textures[outputTextureName]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Vertical
// Description: Applies a vertical blur effect to the input texture.
// Parameters:
//   - inputTextureName: Name of the input texture.
//   - outputTextureName: Name of the output texture.
//   - resolution: Resolution of the input texture.
//   - radius: Radius of the Gaussian kernel.
//   - sigma: Standard deviation of the Gaussian kernel.
//   - startRow: Start row for the processing.
//   - endRow: End row for the processing.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPU_SketchEffect::Vertical(
    const string& inputTextureName,
    const string& outputTextureName,
    glm::ivec2 resolution,
    int radius, float sigma,
    int startRow, int endRow)
{
    vector<unsigned char> in(resolution.x * resolution.y * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[inputTextureName]);
    glReadPixels(0, 0, resolution.x, resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, in.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    vector<unsigned char> out(resolution.x * resolution.y * 4, 0);
    vector<float> weights(2 * radius + 1);
    float sum = 0.0f;

    for (int i = -radius; i <= radius; ++i)
    {
        weights[i + radius] = Weight(i, sigma);
        sum += weights[i + radius];
    }

    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= sum;
    }

    auto VERTICAL_BLUR = [&](int start, int end)
    {
        for (int x = start; x < end; ++x)
        {
            for (int y = 0; y < resolution.y; ++y)
            {
                float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;

                for (int i = -radius; i <= radius; ++i)
                {
                    int ny = glm::clamp(y + i, 0, resolution.y - 1);
                    int index = (ny * resolution.x + x) * 4;

                    float r = in[index] / 255.0f;
                    float g = in[index + 1] / 255.0f;
                    float b = in[index + 2] / 255.0f;

                    float weight = weights[i + radius];
                    sumR += r * weight;
                    sumG += g * weight;
                    sumB += b * weight;
                }

                int idx = (y * resolution.x + x) * 4;
                out[idx + 0] = static_cast<unsigned char>(sumR * 255.0f);
                out[idx + 1] = static_cast<unsigned char>(sumG * 255.0f);
                out[idx + 2] = static_cast<unsigned char>(sumB * 255.0f);
                out[idx + 3] = 255;
            }
        }
    };

	// Multithreaded applied on the columns
    int columns = resolution.x / pool.workers.size();
    for (int t = 0; t < pool.workers.size(); ++t)
    {
        int start = t * columns;
        int end = (t == pool.workers.size() - 1) ? resolution.x : start + columns;
        pool.Add_Task([=] { VERTICAL_BLUR(start, end); }, "VERTICAL_BLUR");
    }

    pool.Free_Resource();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[outputTextureName]);
    glBindTexture(GL_TEXTURE_2D, textures[outputTextureName]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Hatching
// Description: Applies a hatching effect to the input texture.
// Parameters:
//   - inputTextureName: Name of the input texture.
//   - outputTextureName: Name of the output texture.
//   - resolution: Resolution of the input texture.
//   - hatchParams: Parameters of the hatching effect (a, b, c).
//   - threshold: Threshold for the hatching effect (black/white).
//   - invertBackground: Flag to invert the background.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPU_SketchEffect::Hatching(
    const string& inputTextureName,
    const string& outputTextureName,
    glm::ivec2 resolution,
    glm::vec3 hatchParams,
    float threshold,
    bool invertBackground)
{
    vector<unsigned char> in(resolution.x * resolution.y * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[inputTextureName]);
    glReadPixels(0, 0, resolution.x, resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, in.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    vector<unsigned char> out(resolution.x * resolution.y * 4);

    auto HATCHING = [&](int start, int end)
    {
        for (int i = start; i < end; ++i)
        {
            int y = i / resolution.x;
            int x = i % resolution.x;
            int index = (y * resolution.x + x) * 4;

            float u = static_cast<float>(x) / resolution.x;
            float v = static_cast<float>(y) / resolution.y;

			float gray = GrayNuance(in, index);

            float hatchLine = sin(hatchParams.x * u + hatchParams.y * v);
            float hatchBackground;

            if (!invertBackground)
            {
                // Black background with white hatching lines
				hatchBackground = (gray > threshold) ? 1.0f : ((hatchLine > hatchParams.z) ? 1.0f : 0.0f);
            }
            else
            {
                // White background with black hatching lines
				hatchBackground = (gray < threshold) ? 1.0f : ((hatchLine > hatchParams.z) ? 0.0f : 1.0f);
            }

            unsigned char value = static_cast<unsigned char>(hatchBackground * 255.0f);
            out[index + 0] = value;
            out[index + 1] = value;
            out[index + 2] = value;
            out[index + 3] = 255;
        }
    };

	// Multithreaded applied on the pixels (rows * columns)
    int nr_pixels = resolution.x * resolution.y;
    int pixels = nr_pixels / pool.workers.size();
    for (int t = 0; t < pool.workers.size(); ++t)
    {
        int start = t * pixels;
        int end = (t == pool.workers.size() - 1) ? nr_pixels : start + pixels;
        pool.Add_Task([=] { HATCHING(start, end); }, "HATCHING");
    }

    pool.Free_Resource();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[outputTextureName]);
    glBindTexture(GL_TEXTURE_2D, textures[outputTextureName]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: Combine
// Description: Combines multiple textures into a single texture.
// Parameters:
//   - inputTextureNames: Names of the input textures.
//   - outputTextureName: Name of the output texture.
//   - resolution: Resolution of the input textures.
//   - weights: Weights for the input textures.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPU_SketchEffect::Combine(
    const vector<string>& inputTextureNames,
    const string& outputTextureName,
    glm::ivec2 resolution)
{
    vector<vector<unsigned char>> in(inputTextureNames.size());
    vector<unsigned char> combined(resolution.x * resolution.y * 4, 0);

    for (size_t t = 0; t < inputTextureNames.size(); ++t)
    {
        in[t].resize(resolution.x * resolution.y * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[inputTextureNames[t]]);
        glReadPixels(0, 0, resolution.x, resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, in[t].data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    auto COMBINE_IMAGES = [&](int start, int end)
    {
        for (int i = start * 4; i < end * 4; i += 4)
        {
            float finalR = 1.0f;
            float finalG = 1.0f;
            float finalB = 1.0f;

            for (size_t t = 0; t < inputTextureNames.size(); ++t)
            {
                float r = in[t][i] / 255.0f;
                float g = in[t][i + 1] / 255.0f;
                float b = in[t][i + 2] / 255.0f;

                finalR = min(finalR, r);
                finalG = min(finalG, g);
                finalB = min(finalB, b);
            }

            combined[i + 0] = static_cast<unsigned char>(finalR * 255.0f);
            combined[i + 1] = static_cast<unsigned char>(finalG * 255.0f);
            combined[i + 2] = static_cast<unsigned char>(finalB * 255.0f);
            combined[i + 3] = 255;
        }
    };

	// Multithreaded applied on the pixels (rows * columns)
    int nr_pixels = resolution.x * resolution.y;
    int pixels = nr_pixels / pool.workers.size();
    for (int t = 0; t < pool.workers.size(); ++t)
    {
        int start = t * pixels;
        int end = (t == pool.workers.size() - 1) ? nr_pixels : start + pixels;
        pool.Add_Task([=] { COMBINE_IMAGES(start, end); }, "COMBINE_IMAGES");
    }

    pool.Free_Resource();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[outputTextureName]);
    glBindTexture(GL_TEXTURE_2D, textures[outputTextureName]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, combined.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
