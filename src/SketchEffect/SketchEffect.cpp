#include "SketchEffect/SketchEffect.h"

#include "pfd/portable-file-dialogs.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#include <iostream>

using namespace std;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SketchEffect::SketchEffect() : 
	gpuSketchEffect(resolution, framebuffers, textures, shaders, meshes),
	cpuSketchEffect(resolution, framebuffers, textures, shaders, meshes),
	originalImage(nullptr), processedImage(nullptr) /// processedImage -> only use the resolution
	/// Rewrite original image with the new image processed selected by the user
{
    vector<string> textureNames = 
    {
		/// CPU
		"originalCPU",     // STAGE 0 - Original image
        "gaussianCPU",     // STAGE 1 - Gaussian Edge Binarization
        "horizontalCPU",   // STAGE 2 - Horizontal Blur Gaussian
		"verticalCPU",     // STAGE 3 - Vertical Blur Gaussian
		"hatch1CPU",       // STAGE 4 - Hatch 1
		"hatch2CPU",       // STAGE 5 - Hatch 2
		"hatch3CPU",       // STAGE 6 - Hatch 3
		"combinedHatchCPU",// STAGE 7 - Combined Hatch
		"finalCPU",        // STAGE 8 - Final Image
		/// GPU
        "originalGPU",     // STAGE 0 - Original image
        "gaussianGPU",     // STAGE 1 - Gaussian Edge Binarization
        "horizontalGPU",   // STAGE 2 - Horizontal Blur Gaussian
        "verticalGPU",     // STAGE 3 - Vertical Blur Gaussian
        "hatch1GPU",       // STAGE 4 - Hatch 1
        "hatch2GPU",       // STAGE 5 - Hatch 2
        "hatch3GPU",       // STAGE 6 - Hatch 3
        "combinedHatchGPU",// STAGE 7 - Combined Hatch
        "finalGPU"         // STAGE 8 - Final Image
    };

    for (const auto& name : textureNames) 
    {
        framebuffers[name] = -1;
        textures[name] = -1;
    }

	gpuProcessing = false;   /// true - GPU / false - CPU Multi-Threading
	onlyExecuteOnce = true;  /// true - Execute only once / false - Execute every frame
	gaussian2Steps = false;  /// true - Gaussian 2 steps / false - Gaussian 1 step

    outputMode = 0;
    saveScreenToImage = false;
	resolution = window->GetResolution();

	radiusSize = 12;
	sigmaSize = float(radiusSize) / 2.0f;

    thresholdSobel = 0.3;
	thresholdHatch1 = 0.10;
	thresholdHatch2 = 0.25;
	thresholdHatch3 = 0.30;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SketchEffect::~SketchEffect()
{
    for (const auto& pair : textures) 
    {
        if (pair.second != -1) 
        {
            glDeleteTextures(1, &pair.second);
        }
    }

    for (const auto& pair : framebuffers) 
    {
        if (pair.second != -1) 
        {
            glDeleteFramebuffers(1, &pair.second);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: CreateTexBuffer
// Description: Create a texture buffer.
// Basic texturing for image processing, a fbo with a texture attached per stage.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GLuint SketchEffect::CreateTexBuffer(const string& name)
{
    GLuint framebuffer, textureID;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        cerr << "[Error]: Create " << name << " framebuffer!" << endl;
    }

    framebuffers[name] = framebuffer;
    textures[name] = textureID;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return framebuffer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: InitTexBuffers
// Description: Initialize the textures buffers.
// All textures have the same resolution at the beginning and are resized when the window is resized.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::InitTexBuffers()
{
    {
        originalImage = TextureManager::LoadTexture(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::TEXTURES,
            "cube", "pos_x.png"), nullptr, "image", true, true);
        processedImage = TextureManager::LoadTexture(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::TEXTURES,
            "cube", "pos_x.png"), nullptr, "newImage", true, true);
    }

	/// Original Image loading directly from the texture    
    CreateTexBuffer("originalCPU");
    textures["originalCPU"] = originalImage->GetTextureID();

    CreateTexBuffer("horizontalCPU");
    CreateTexBuffer("verticalCPU");
    CreateTexBuffer("gaussianCPU");
    CreateTexBuffer("hatch1CPU");
    CreateTexBuffer("hatch2CPU");
    CreateTexBuffer("hatch3CPU");
    CreateTexBuffer("combinedHatchCPU");
    CreateTexBuffer("finalCPU");

    /// GPU
	CreateTexBuffer("originalGPU");
    textures["originalGPU"] = originalImage->GetTextureID();

	CreateTexBuffer("horizontalGPU");
	CreateTexBuffer("verticalGPU");
	CreateTexBuffer("gaussianGPU");
	CreateTexBuffer("hatch1GPU");
	CreateTexBuffer("hatch2GPU");
	CreateTexBuffer("hatch3GPU");
	CreateTexBuffer("combinedHatchGPU");
	CreateTexBuffer("finalGPU");
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: ResizeTexBuffers
// Description: Resize the textures buffers, all of them have the same resolution.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::ResizeTexBuffers() const
{
    for (const auto& pair : framebuffers)
    {
        const string& name = pair.first;
        GLuint framebuffer = pair.second;
        GLuint textureID = textures.at(name);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::Init()
{
    {
        Mesh* mesh = new Mesh("quad");
        mesh->LoadMesh(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::MODELS, "primitives"), "quad.obj");
        mesh->UseMaterials(false);
        meshes[mesh->GetMeshID()] = mesh;
    }

    string shaderPath = PATH_JOIN(window->props.selfDir, SOURCE_PATH::PATH_PROJECT, "SketchEffect", "shaders");

    {
        Shader* shader = new Shader("ImageProcessing");
        shader->AddShader(PATH_JOIN(shaderPath, "SketchEffect.VS.glsl"), GL_VERTEX_SHADER);
        shader->AddShader(PATH_JOIN(shaderPath, "SketchEffect.FS.glsl"), GL_FRAGMENT_SHADER);

        shader->CreateAndLink();
        shaders[shader->GetName()] = shader;
    }

    InitTexBuffers();

    cout << endl;
    cout << "!!!RECOMMEND BEFORE SWAPPING BETWEEN CPU AND GPU PIPELINES" << endl;
    cout << "\tTO GO BACK TO INITIAL IMAGE OUTPUT MODE 0!!!" << endl;

    cout << endl;
    cout << "!!!WITH GPU DON'T OBTAIN THE SAME CORRECT RESULTS AS ON CPU MULTI-THREADING!!!" << endl;
    cout << "\tON GPU IT LOOKS MORE LIKE A BLACK AND WHITE IMAGE WITH SOME LINES," << endl;
    cout << "\tIT CAN BE SEEN SOME `HATCHES` ON THE WHITE PARTS OF THE FINAL IMAGE" << endl;

    cout << endl;
    cout << "GPU Processing: " << (gpuProcessing ? "ON" : "OFF") << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::Update(float deltaTimeSeconds)
{
    ClearScreen();

    resolution = window->GetResolution();
    glViewport(0, 0, resolution.x, resolution.y);

    float aspectRatio = static_cast<float>(resolution.x) / resolution.y;
    glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(aspectRatio, 1.0f, 1.0f));

    if (saveScreenToImage)
    {
        window->SetSize(originalImage->GetWidth(), originalImage->GetHeight());
    }

	//////////////////////////////////////////////////////////CPU MULTI-THREADING PIPELINE///////////////////////////////////////////////////////////
    if (onlyExecuteOnce)
    {
        if (!gpuProcessing)
        {
            // Zero Pass: Backup original image
            cpuSketchEffect.RenderOriginal("originalCPU", "originalCPU", "ImageProcessing", modelMatrix, 0, resolution);
            // First Pass: Horizontal Blur
            cpuSketchEffect.Horizontal("originalCPU", "horizontalCPU", resolution, radiusSize, sigmaSize, 0, resolution.y);
            // Second Pass: Vertical Blur
            cpuSketchEffect.Vertical("horizontalCPU", "verticalCPU", resolution, radiusSize, sigmaSize, 0, resolution.y);
            // Third Pass: Gaussian Blur
            cpuSketchEffect.EdgeBinarize("originalCPU", "gaussianCPU", resolution, thresholdSobel, 0, resolution.y);
            // Fourth Pass: Hatching 1
            cpuSketchEffect.Hatching("verticalCPU", "hatch1CPU", resolution, glm::vec3(400.0f, 0.0f, 0.99f), thresholdHatch1, false);
            // Fifth Pass: Hatching 2
            cpuSketchEffect.Hatching("verticalCPU", "hatch2CPU", resolution, glm::vec3(200.0f, 200.0f, 0.95f), thresholdHatch2, true);
            // Sixth Pass: Hatching 3
            cpuSketchEffect.Hatching("verticalCPU", "hatch3CPU", resolution, glm::vec3(250.0f, -250.0f, 0.90f), thresholdHatch3, true);
            // Seventh Pass: Combine Hatches
            cpuSketchEffect.Combine({ "hatch1CPU", "hatch2CPU", "hatch3CPU" }, "combinedHatchCPU", resolution);
            // Eight Pass: Sobel + Combined Hatches
            cpuSketchEffect.Combine({ "gaussianCPU", "combinedHatchCPU" }, "finalCPU", resolution);
        }
		else /// 4 GPU IT DOESN'T APPLY THE HORIZONTAL AND VERTICAL BLUR CORRECT AND THE COMBINE FUNCTION SAME
        {
            // Zero Pass: Backup original image
			gpuSketchEffect.RenderOriginal("originalGPU", "originalGPU", "ImageProcessing", modelMatrix, 0);
            // First Pass: Horizontal Blur
			gpuSketchEffect.Horizontal("horizontalGPU", "originalGPU", "ImageProcessing", radiusSize, sigmaSize);
            // Second Pass: Vertical Blur
			gpuSketchEffect.Vertical("verticalGPU", "horizontalGPU", "ImageProcessing", radiusSize, sigmaSize);
            // Third Pass: Gaussian Blur
			gpuSketchEffect.EdgeBinarize("gaussianGPU", "originalGPU", "ImageProcessing", thresholdSobel);
            // Fourth Pass: Hatching 1
			gpuSketchEffect.Hatching("hatch1GPU", "verticalGPU", "ImageProcessing", glm::vec3(400.0f, 0.0f, 0.99f), thresholdHatch1, 1, false);
            // Fifth Pass: Hatching 2
			gpuSketchEffect.Hatching("hatch2GPU", "verticalGPU", "ImageProcessing", glm::vec3(200.0f, 200.0f, 0.95f), thresholdHatch2, 2, true);
            // Sixth Pass: Hatching 3
			gpuSketchEffect.Hatching("hatch3GPU", "verticalGPU", "ImageProcessing", glm::vec3(250.0f, -250.0f, 0.90f), thresholdHatch3, 3, true);
            // Seventh Pass: Combine Hatches
			gpuSketchEffect.Combine("combinedHatchGPU", "ImageProcessing", { "hatch1GPU", "hatch2GPU", "hatch3GPU" });
            // Eight Pass: Sobel + Combined Hatches
			gpuSketchEffect.Combine("finalGPU", "ImageProcessing", { "gaussianGPU", "combinedHatchGPU" });
        }
        onlyExecuteOnce = false;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    auto finalShader = shaders["ImageProcessing"];
    finalShader->Use();
    glUniform1i(finalShader->GetUniformLocation("gpuProcessing"), gpuProcessing ? 1 : 0);
    glUniform1i(finalShader->GetUniformLocation("outputMode"), outputMode);
    glUniform1i(finalShader->GetUniformLocation("flipVertical"), 1);
    glActiveTexture(GL_TEXTURE0);

    if (!gpuProcessing)
    {
        switch (outputMode)
        {
        case 0: glBindTexture(GL_TEXTURE_2D, textures["originalCPU"]); break;
        case 1: glBindTexture(GL_TEXTURE_2D, textures["gaussianCPU"]); break;
        case 2: glBindTexture(GL_TEXTURE_2D, textures["horizontalCPU"]); break;
        case 3: glBindTexture(GL_TEXTURE_2D, textures["verticalCPU"]); break;
        case 4: glBindTexture(GL_TEXTURE_2D, textures["hatch1CPU"]); break;
        case 5: glBindTexture(GL_TEXTURE_2D, textures["hatch2CPU"]); break;
        case 6: glBindTexture(GL_TEXTURE_2D, textures["hatch3CPU"]); break;
        case 7: glBindTexture(GL_TEXTURE_2D, textures["combinedHatchCPU"]); break;
        case 8: glBindTexture(GL_TEXTURE_2D, textures["finalCPU"]); break;
        default: glBindTexture(GL_TEXTURE_2D, textures["originalCPU"]); break; /// Original image
        }
    }
    else
    {
        switch (outputMode)
        {
        case 0: glBindTexture(GL_TEXTURE_2D, textures["originalGPU"]); break;
        case 1: glBindTexture(GL_TEXTURE_2D, textures["gaussianGPU"]); break;
        case 2: glBindTexture(GL_TEXTURE_2D, textures["horizontalGPU"]); break;
        case 3: glBindTexture(GL_TEXTURE_2D, textures["verticalGPU"]); break;
        case 4: glBindTexture(GL_TEXTURE_2D, textures["hatch1GPU"]); break;
        case 5: glBindTexture(GL_TEXTURE_2D, textures["hatch2GPU"]); break;
        case 6: glBindTexture(GL_TEXTURE_2D, textures["hatch3GPU"]); break;
        case 7: glBindTexture(GL_TEXTURE_2D, textures["combinedHatchGPU"]); break;
        case 8: glBindTexture(GL_TEXTURE_2D, textures["finalGPU"]); break;
        default: glBindTexture(GL_TEXTURE_2D, textures["originalGPU"]); break; /// Original image
        }
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderMesh(meshes["quad"], finalShader, modelMatrix);

    if (saveScreenToImage)
    {
        saveScreenToImage = false;
		SaveImage("shader_processing_" + to_string(outputMode) + "_CPU");
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: SaveImage
// Description: Save the current image to a file depending on the output mode and the processing mode.
// in this moment it saves the CPU image correctly but the GPU image is not saved correctly 
//              (it saves the originalGPU image, because of the GPU Pipeline)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::SaveImage(const string& fileName)
{
    string outMode;

    if (!gpuProcessing)
    {
        switch (outputMode)
        {
        case 0: outMode = "originalCPU"; break;
        case 1: outMode = "horizontalCPU"; break;
        case 2: outMode = "verticalCPU"; break;
        case 3: outMode = "gaussianCPU"; break;
        case 4: outMode = "hatch1CPU"; break;
        case 5: outMode = "hatch2CPU"; break;
        case 6: outMode = "hatch3CPU"; break;
        case 7: outMode = "combinedHatchCPU"; break;
        case 8: outMode = "finalCPU"; break;
        default: cerr << "[Error]: Invalid outputMode. No texture to save." << endl; return;
        }
    }
    else
    {
        switch (outputMode)
        {
        case 0: outMode = "originalGPU"; break;
        case 1: outMode = "horizontalGPU"; break;
        case 2: outMode = "verticalGPU"; break;
        case 3: outMode = "gaussianGPU"; break;
        case 4: outMode = "hatch1GPU"; break;
        case 5: outMode = "hatch2GPU"; break;
        case 6: outMode = "hatch3GPU"; break;
        case 7: outMode = "combinedHatchGPU"; break;
        case 8: outMode = "finalGPU"; break;
        default: cerr << "[Error]: Invalid outputMode. No texture to save." << endl; return;
        }
		cout << "IT DOESNT SAVE THE GPU IMAGE CORRECTLY" << endl;
    }

    if (textures.find(outMode) == textures.end())
    {
        cerr << "[Error]: Texture not found: " << outMode << endl;
        return;
    }

    GLuint tex_save = textures[outMode];
    GLuint framebuffer = framebuffers[outMode];
    GLenum format = GL_RGBA;
    int channels = 4;

    vector<unsigned char> pixel_data(resolution.x * resolution.y * channels);

    const GLint pixelFormat[5] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };

    {
        glBindTexture(GL_TEXTURE_2D, tex_save);
        glGetTexImage(GL_TEXTURE_2D, 0, pixelFormat[channels], GL_UNSIGNED_BYTE, pixel_data.data());
    }

    string original_name = TextureManager::GetNameTexture(originalImage);
    size_t pos_last_slash = original_name.find_last_of("/\\");
    string baseName = (pos_last_slash == string::npos) ? original_name : original_name.substr(pos_last_slash + 1);
    size_t pos_last_dot = baseName.find_last_of('.');
    baseName = baseName.substr(0, pos_last_dot);

    string extension = (pos_last_dot != string::npos) ? original_name.substr(pos_last_dot + 1) : "png";
    string full_name = fileName + "_" + baseName + "." + extension;

    string cwd = CWD();
    string abspath = cwd + "/" + full_name;
    cout << "Saving image to: " << abspath << endl;

    bool success = false;
    if (extension == "png")
        success = stbi_write_png(abspath.c_str(), resolution.x, resolution.y, channels, pixel_data.data(), resolution.x * channels);
    else if (extension == "jpg" || extension == "jpeg")
        success = stbi_write_jpg(abspath.c_str(), resolution.x, resolution.y, channels, pixel_data.data(), 100);
    else if (extension == "bmp")
        success = stbi_write_bmp(abspath.c_str(), resolution.x, resolution.y, channels, pixel_data.data());
    else
    {
        cerr << "[Error]: Unsupported image format: " << extension << endl;
        return;
    }

    if (success)
        cout << "[Done]: Image successfully saved to: " << abspath << endl;
    else
        cerr << "[Error]: Failed to save image to: " << abspath << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: OnFileSelected
// Description: Load the selected image file and update the resolution.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::OnFileSelected(const string& fileName)
{
	/// If a new image is selected, load it into the textures and update the resolution ///
    if (!fileName.empty())
    {
		onlyExecuteOnce = true;
        cout << "Image loaded: " << fileName << endl;
        processedImage = TextureManager::LoadTexture(fileName, nullptr, "newImage", true, true);
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        resolution = glm::ivec2(processedImage->GetWidth(), processedImage->GetHeight());
        ResizeTexBuffers();
        window->SetSize(resolution.x, resolution.y);
        glViewport(0, 0, resolution.x, resolution.y);
        cout << "Window resized to match the image resolution: " << resolution.x << "x" << resolution.y << endl;
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[gpuProcessing ? "originalGPU" : "originalCPU"]);
        glBindTexture(GL_TEXTURE_2D, textures[gpuProcessing ? "originalGPU" : "originalCPU"]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[gpuProcessing ? "originalGPU" : "originalCPU"], 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        originalImage = TextureManager::LoadTexture(fileName, nullptr, "image", true, true);
        float aspectRatio = static_cast<float>(resolution.x) / resolution.y;
        glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(aspectRatio, 1.0f, 1.0f));
		/// Depending on the processing mode, render the image using the CPU or GPU pipeline
        if (gpuProcessing)
        {
			textures["originalGPU"] = originalImage->GetTextureID();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            gpuSketchEffect.RenderOriginal("originalGPU", "originalGPU", "ImageProcessing", modelMatrix, 0);
        }
        else
        {
			textures["originalCPU"] = originalImage->GetTextureID();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cpuSketchEffect.RenderOriginal("originalCPU", "originalCPU", "ImageProcessing", modelMatrix, 0, resolution);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::OnKeyPress(int key, int mods)
{
    /** Add key press event */
    if (key == GLFW_KEY_F || key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) OpenDialog();
    if (key - GLFW_KEY_0 >= 0 && key <= GLFW_KEY_9)
    {
        outputMode = key - GLFW_KEY_0;
        switch (outputMode)
        {
		case 0: { cout << "Key 0 - Original image;" << endl; break;                                                                         }
		case 1: { cout << "Key 1 - Result of horizontal smoothing filter;" << endl; break;                                                  }
        case 2: { cout << "Key 2 - Result of smoothing filter - horizontal and vertical (smoothing filter result);" << endl; break;         }
		case 3: { cout << "Key 3 - Result of Sobel filter + binarization;" << endl; break;                                                  }
		case 4: { cout << "Key 4 - Result of smoothing filter + hatching filter 1;" << endl; break;                                         }
		case 5: { cout << "Key 5 - Result of smoothing filter + hatching filter 2;" << endl; break;                                         }       
		case 6: { cout << "Key 6 - Result of smoothing filter + hatching filter 3;" << endl; break;                                         }   
		case 7: { cout << "Key 7 - Result of smoothing filter + all three hatching filters applied;" << endl; break;                        }
		case 8: { cout << "Key 8 - The final image result of the sketch effect pipeline." << endl; break;                                   }
        default: { cout << "KEY 9 - DEBUG MODE!" << endl; break;                                                                            }
        }
    }
    if (key == GLFW_KEY_S && (mods & GLFW_MOD_CONTROL)) { saveScreenToImage = true; }
	if (key == GLFW_KEY_G) 
    { 
        gpuProcessing = !gpuProcessing; 
        onlyExecuteOnce = true;
        cout << "GPU Processing: " << (gpuProcessing ? "ON" : "OFF") << endl;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: OpenDialog
// Description: Open a file dialog to select an image file.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::OpenDialog()
{
    vector<string> filters =
    {
        "Image Files", "*.png *.jpg *.jpeg *.bmp",
        "All Files", "*"
    };

    auto selection = pfd::open_file("Select a file", ".", filters).result();
    if (!selection.empty())
    {
        cout << "User selected file " << selection[0] << "\n";
        OnFileSelected(selection[0]);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::FrameStart()
{
    glClearColor(0.2, 0.2, 0.2, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::FrameEnd()
{
#if 1 // 1 - ACTIVE / 0 - INACTIVE
    DrawCoordinateSystem();
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SketchEffect::OnWindowResize(int width, int height) { /** Treat window resize event */ }
void SketchEffect::OnKeyRelease(int key, int mods) { /** Add key release event */ }
void SketchEffect::OnInputUpdate(float deltaTime, int mods) { /** Treat continous update based on inpute */ }
void SketchEffect::OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY) { /** Add on mouse move */ }
void SketchEffect::OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY) { /** Add on mouse scroll */ }
void SketchEffect::OnMouseBtnPress(int mouseX, int mouseY, int button, int mods) { /** Add on mouse btn press */ }
void SketchEffect::OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods) { /** Add on mouse btn release */ }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
