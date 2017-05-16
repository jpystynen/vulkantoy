vulkantoy
=========

Vulkantoy is a small test project that can be used as a shadertoy test app for image shaders
on Windows. It compiles shaders and updates image samplers on the fly.
Shader inputs are common shadertoy uniform variables and four image sampler2Ds. Shadertoy-shaders with other input resources (like buffers) do not work.

The app has only been tested with Nvidia GTX 970. Debug has Vulkan validation layer enabled.

![alt text](screenshots/vulkantoy.png?raw=true)
![alt text](screenshots/default.png?raw=true)

Test shaders in the first screenshot:
* [Seascape](https://www.shadertoy.com/view/Ms2SD1) 
* [BloodCells](https://www.shadertoy.com/view/4ttXzj) 
* [Snails](https://www.shadertoy.com/view/ld3Gz2) 
* [Tribute - Journey!](https://www.shadertoy.com/view/ldlcRf) 

The second screenshot shows the default appearance with default textures.
* [McGuire Graphics Data][image_williams]: Default images.

Use Without Building
--------------------
(e.g. with Windows PowerShell)

```sh
git clone https://github.com/jpystynen/vulkantoy.git
vulkantoy> .\bin\vulkantoy.exe
vulkantoy> .\bin\vulkantoy_debug.exe
```
Modify shaders in shaders dir, and modify textures in textures dir. You must be in the root dir when you run the exe. Debug has Vulkan debug report and "VK_LAYER_LUNARG_standard_validation" enabled.

Building
--------

The application uses Window classes for window creation and file directory watching. Does
not compile on other platforms.

### Dependencies

* [Vulkan SDK][vksdk]: LunarG Vulkan SDK for vulkan.
* [CMake][cmake]: For generating compilation targets.
* [Visual Studio][vstudio]: For compiling (tested with community).
* [glslang][glsl]: For shader compiling on the fly. (Precompiled libs in external/lib.)
* [stbimage][stb]: For image loading.
(Single header file in src/external/stb/stb_image.h)

### Build steps

```sh
git clone https://github.com/jpystynen/vulkantoy.git
cd external
git clone https://github.com/KhronosGroup/glslang.git
(Refer to glslang wiki if you want to build glslang. Precompiled libs are in external/lib)
```
Run cmake.
Build with VS.

Remember to set the working directory in VS to vulkantoy root.

## Shaders

Compiled spir-v files need to be present in shaders directory when the app boots.
 When the app is running, you can paste your mainImage-function to toy.frag shader
 between the following comment lines. GLSL shader is recompiled when you save the file.
 Some shadertoy-shaders might need additional defines for different uniform variable names.

```C
///////////////////////////////////////////////////////////////////////////////
// SHADERTOY SHADER HERE

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}

// SHADERTOY SHADER ENDS HERE
///////////////////////////////////////////////////////////////////////////////
```
## Samplers (images)

There are four uniform sampler2Ds created from textures. At boot you need to
have four png files (channel[0-3].png) in the textures directory. When the app is running
you can replace the png images with any formats that stb_image supports. Searched image names
are channel[0-3] and if there are new images they are updated on the fly.
E.g. rename channel0.png and copy channel0.tga to textures directory.

Credits
--------

* [stb_image][stb]: Single header public domain image loader.
* [glslang][glsl]: An OpenGL and OpenGL ES shader front end and validator.
* [McGuire Graphics Data][image_williams]: Default images channel[0-3].png.

License
--------

MIT. See LICENSE

[vksdk]: https://www.lunarg.com/vulkan-sdk/
[cmake]: https://cmake.org/
[vstudio]: https://www.visualstudio.com/vs/community/
[glsl]: https://github.com/KhronosGroup/glslang
[stb]: https://github.com/nothings/stb
[image_williams]: http://graphics.cs.williams.edu/data/images.xml
