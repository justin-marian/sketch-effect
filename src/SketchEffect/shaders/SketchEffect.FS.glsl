#version 410

// Input
layout(location = 0) in vec2 texture_coord;

// Uniform properties
/// CPU Processing
uniform sampler2D originalCPU;
/// GPU Processing
uniform sampler2D originalGPU;
uniform sampler2D horizontalGPU;
uniform sampler2D verticalGPU;
uniform sampler2D gaussianGPU;
uniform sampler2D hatch1GPU;
uniform sampler2D hatch2GPU;
uniform sampler2D hatch3GPU;
uniform sampler2D combinedHatchGPU;
uniform sampler2D finalGPU;
/// Swapping between CPU and GPU processing
uniform int gpuProcessing;
uniform ivec2 screenSize;
/// Parameters Gaussian Binarization + Hatching
uniform float thresholdSobel;
uniform int radius;
//////////////////////////////////////////////
uniform vec3 hatch1Params;
uniform float hatch1Threshold;
uniform vec3 hatch2Params;
uniform float hatch2Threshold;
uniform vec3 hatch3Params;
uniform float hatch3Threshold;
//////////////////////////////////////////////
uniform int outputMode;
uniform int flipVertical;     // 0: No flip, 1: Flip vertically

// Output
layout(location = 0) out vec4 out_color;


vec2 correctedTexCoord(vec2 texCoord, int flip)
{
    return vec2(texCoord.x, flip == 1 ? 1.0 - texCoord.y : texCoord.y);
}

float gray_nuance(vec4 pixel)
{
    return 0.21 * pixel.r + 0.71 * pixel.g + 0.07 * pixel.b;
}

float gaussianWeight(int x, float sigma)
{
    return exp(-float(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * 3.14159) * sigma);
}


vec4 gaussian_horizontal_blur(sampler2D inputTexture, vec2 texCoord, 
    int radius, ivec2 screenSize, float sigma)
{
    vec2 texelSize = 1.0 / vec2(screenSize);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;

    for (int i = -radius; i <= radius; i++)
    {
        float weight = gaussianWeight(i, sigma);
        sum += texture(inputTexture, texCoord + vec2(i, 0) * texelSize) * weight;
        weightSum += weight;
    }

    return sum / weightSum;
}


vec4 gaussian_vertical_blur(sampler2D inputTexture, vec2 texCoord, 
    int radius, ivec2 screenSize, float sigma)
{
    vec2 texelSize = 1.0 / vec2(screenSize);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;

    for (int j = -radius; j <= radius; j++)
    {
        float weight = gaussianWeight(j, sigma);
        sum += texture(inputTexture, texCoord + vec2(0, j) * texelSize) * weight;
        weightSum += weight;
    }

    return sum / weightSum;
}


vec4 binarize_sobel(sampler2D inputTexture, vec2 texCoord, 
    int radius, ivec2 screenSize, float threshold)
{
    vec2 texelSize = 1.0 / vec2(screenSize);
    vec2 gradient = vec2(0.0);

    const mat3 Gx = mat3(
        -1.0, 0.0, 1.0,
        -2.0, 0.0, 2.0,
        -1.0, 0.0, 1.0
    );
    const mat3 Gy = mat3(
        -1.0, -2.0, -1.0,
        0.0, 0.0, 0.0,
        1.0, 2.0, 1.0
    );

    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            vec4 color = texture(inputTexture, texCoord + vec2(i, j) * texelSize);
            float grayValue = gray_nuance(color);
            gradient.x += grayValue * Gx[i + 1][j + 1];
            gradient.y += grayValue * Gy[i + 1][j + 1];
        }
    }

    float magnitude = length(gradient);
    float binary = (magnitude >= threshold) ? 0.0 : 1.0;
    return vec4(vec3(binary), 1.0);
}


vec4 hatching(
    sampler2D inputTexture, vec2 texCoord,
    vec3 hatchParams, float threshold,
    bool invertBackground, ivec2 screenSize)
{
    vec2 texelSize = 1.0 / vec2(screenSize);
    vec2 pixelCoord = texCoord / texelSize;

    vec4 inputColor = texture(inputTexture, texCoord);
    float grayValue = gray_nuance(inputColor);

    float hatchLine = sin(hatchParams.x * pixelCoord.x + hatchParams.y * pixelCoord.y);
    float hatchBackground;

    if (!invertBackground)
    {
        hatchBackground = (grayValue > threshold) ? 1.0 : ((hatchLine > hatchParams.z) ? 1.0 : 0.0);
    }
    else
    {
        hatchBackground = (grayValue < threshold) ? 1.0 : ((hatchLine > hatchParams.z) ? 0.0 : 1.0);
    }

    return vec4(vec3(hatchBackground), 1.0);
}


vec4 combine_textures(
    sampler2D inputTexture, vec2 texCoord,
    sampler2D hatch1GPU, sampler2D hatch2GPU, sampler2D hatch3GPU,
    ivec2 screenSize)
{
    vec2 texelSize = 1.0 / vec2(screenSize);

    vec4 hatch1 = texture(hatch1GPU, texCoord);
    vec4 hatch2 = texture(hatch2GPU, texCoord);
    vec4 hatch3 = texture(hatch3GPU, texCoord);

    float gray1 = gray_nuance(hatch1);
    float gray2 = gray_nuance(hatch2);
    float gray3 = gray_nuance(hatch3);

    float minR = min(min(hatch1.r, hatch2.r), hatch3.r);
    float minG = min(min(hatch1.g, hatch2.g), hatch3.g);
    float minB = min(min(hatch1.b, hatch2.b), hatch3.b);

    vec4 inputColor = texture(inputTexture, texCoord);
    float inputGray = gray_nuance(inputColor);

    float finalGray = min(inputGray, min(minR, min(minG, minB)));
    return vec4(vec3(finalGray), 1.0);
}


void main()
{
    vec2 texCoord = correctedTexCoord(texture_coord, flipVertical);
    float sigma = float(radius) / 2.0;

    if (gpuProcessing == 0) { // 0 - CPU Processing
        if (outputMode >= 0 && outputMode <= 8) {
            out_color = texture(originalCPU, texCoord);
        }
        else { // 9 - DEBUG: Image grayscale (Original texture)
            out_color = vec4(vec3(gray_nuance(texture(originalCPU, texCoord))), 1.0);
        }
        return;
    }

    /// Output mode: 
    /// (
    /// 0: Original image, 
    /// 1: Sobel filter with Binarization, 
    /// 2: Horizontal Gaussian blur, 
    /// 3: Horizontal + Vertical Gaussian blur, 
    /// 4: Hatching 1, 
    /// 5: Hatching 2,
    /// 6: Hatching 3, 
    /// 7: Combine Hatches with Smooth Blending, 
    /// 8: Sobel + Combined Hatches, 
    /// 9: DEBUG: Image grayscale (Original texture)
    /// )

    if (outputMode == 0) { // 0 - Original image
        out_color = texture(originalGPU, texCoord);
    }
    else if (outputMode == 1) { // 1 - Sobel filter with Binarization
        out_color = binarize_sobel(gaussianGPU, texCoord, radius, screenSize, thresholdSobel);
    }
    else if (outputMode == 2) { // 2 - Horizontal Gaussian blur
        out_color = gaussian_horizontal_blur(originalGPU, texCoord, radius, screenSize, sigma);
    }
    else if (outputMode == 3) { // 3 - Horizontal + Vertical Gaussian blur
        out_color = gaussian_vertical_blur(horizontalGPU, texCoord, radius, screenSize, sigma);
    }
	else if (outputMode == 4 ) { // 4 - Hatching 1
        out_color = hatching(verticalGPU, texCoord, hatch1Params, hatch1Threshold, false, screenSize);
    }
	else if (outputMode == 5) { // 5 - Hatching 2
		out_color = hatching(verticalGPU, texCoord, hatch2Params, hatch2Threshold, true, screenSize);
	}
	else if (outputMode == 6) { // 6 - Hatching 3
		out_color = hatching(verticalGPU, texCoord, hatch3Params, hatch3Threshold, true, screenSize);
	}
    else if (outputMode == 7) { // 7 - Combine Hatches with Smooth Blending
        out_color = combine_textures(verticalGPU, texCoord, hatch1GPU, hatch2GPU, hatch3GPU, screenSize);
    }
    else if (outputMode == 8) { // 8 - Sobel + Combined Hatches
        vec4 sobel = binarize_sobel(gaussianGPU, texCoord, radius, screenSize, thresholdSobel);
        vec4 combinedHatches = combine_textures(verticalGPU, texCoord, hatch1GPU, hatch2GPU, hatch3GPU, screenSize);

        float sobelGray = gray_nuance(sobel);
        float hatchesGray = gray_nuance(combinedHatches);

        float finalGray = min(sobelGray, hatchesGray);
        out_color = vec4(vec3(finalGray), 1.0);
    }
    else { // 9 - DEBUG: Image grayscale (Original texture)
        out_color = vec4(vec3(gray_nuance(texture(originalGPU, texCoord))), 1.0);
    }
}
