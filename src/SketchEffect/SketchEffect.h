#pragma once

#include "CPU_SketchEffect.h"
#include "GPU_SketchEffect.h"

#include "components/simple_scene.h"
#include "core/gpu/frame_buffer.h"

#include <vector>
#include <unordered_map>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif


class SketchEffect : public gfxc::SimpleScene
{
    public:
    SketchEffect();
    ~SketchEffect();
    void Init() override;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string CWD()
    {
        char buffer[FILENAME_MAX];
#ifdef _WIN32
        if (_getcwd(buffer, FILENAME_MAX) == NULL)
        {
            perror("_getcwd error");
            return "";
        }
#else
        if (getcwd(buffer, sizeof(buffer)) == NULL)
        {
            perror("getcwd error");
            return "";
        }
#endif
        return std::string(buffer);
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private:
    void FrameStart() override;
    void Update(float deltaTimeSeconds) override;
    void FrameEnd() override;

    void OnInputUpdate(float deltaTime, int mods) override;
    void OnKeyPress(int key, int mods) override;
    void OnKeyRelease(int key, int mods) override;
    void OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY) override;
    void OnMouseBtnPress(int mouseX, int mouseY, int button, int mods) override;
    void OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods) override;
    void OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY) override;
    void OnWindowResize(int width, int height) override;

protected:
	// CPU and GPU sketch effect objects for processing the image
    CPU_SketchEffect cpuSketchEffect;
	GPU_SketchEffect gpuSketchEffect;

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Used to open a file dialog to select an image
    void OpenDialog();
	// Select a file from the dialog box to load the image into the application
    void OnFileSelected(const std::string& fileName);
	// Save the processed image to a file on disk (PNG/JPG/JPEG/BMP) format
    void SaveImage(const std::string& fileName);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Initialize the framebuffers and textures used for processing the image
    void InitTexBuffers();
	// Create a framebuffer object
    GLuint CreateTexBuffer(const std::string& name);
	// Create a texture object
    void ResizeTexBuffers() const;
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    glm::ivec2 resolution;

	bool gpuProcessing;
	bool onlyExecuteOnce;
    bool saveScreenToImage;
	bool gaussian2Steps;

    int outputMode;

    int radiusSize;
    float sigmaSize;

    float thresholdSobel;
    float thresholdHatch1;
	float thresholdHatch2;
	float thresholdHatch3;

    Texture2D* originalImage;
    Texture2D* processedImage;

    std::unordered_map<std::string, GLuint> framebuffers;
    std::unordered_map<std::string, GLuint> textures;
};
