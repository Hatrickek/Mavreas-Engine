# Oxylus Engine
![Logo](https://cdn.discordapp.com/attachments/1012357737256058924/1109482685388312677/OXLogoBanner.png)     
[![CI](https://img.shields.io/github/actions/workflow/status/Hatrickek/OxylusEngine/cmake.yml?&style=for-the-badge&logo=cmake&logoColor=orange&labelColor=black)](https://github.com/Hatrickek/OxylusEngine/actions/workflows/cmake.yml)
## About   
This is my hobby project that I work on in my spare time to learn more about graphics programming and engine architectures. Especially Vulkan and C++.
Also to produce some games including my dream game in the future.

Currently I'm developing [Oxrena](https://github.com/Hatrickek/Oxrena) -a quake-like movement shooter- using Oxylus!

Windows, Linux and Mac (with MoltenVK) is supported.

## Features:     
- Modular Vulkan renderer built with Vuk.
- Modern rendering features:
	- Clustered Forward IBL PBR
	- GPU Culling
	- GTAO
	- SSR
	- PCF Cascaded Directional, Spot and Point Light Shadows
	- Physically Based Bloom
	- Depth Of Field 
	- HDR
	- Tonemapping 
	- Chromatic Aberration
	- Film Grain
	- Vignette
	- Sharpen
	- and many more various post-processing effects.
- Multithreaded physics with Jolt.   
- C++ and Lua scripting API with ECS events and ECS systems. Which has been used to
build multiple games to test the API and engine in general.   
 More about that: [Oxrena](https://github.com/Hatrickek/Oxrena), [(Creating a Game With Oxylus)](https://hatrickek.github.io/blog/oxylus-first-game)
- Editor with features such as Projects, Serializable Scenes, ContentBrowser, Prefabs, Shader Hot Reloading, Entity managing, Prefabs, Inspector Panel,
Asset Manager, Material System&Editor, In-Editor Console, and a lot more QOL
features...
- 3D Audio with miniaudio

## Showcase
![911](https://cdn.discordapp.com/attachments/1012357737256058924/1164661835610464387/image.png?ex=654406db&is=653191db&hm=78980e1510de7ca9cd4fc352faa71fda54c4ac920a4e6384fca7dcf3886eea16&)
(New Editor UI)
![PostProcessing](https://cdn.discordapp.com/attachments/882355531463938078/1101916100414931066/image.png)
PBR Testing scene with Depth Of Field, SSR, SSAO, Bloom, Vignette, Film Grain, Chromatic Aberration, Sharpen
![911_2](https://media.discordapp.net/attachments/1012357737256058924/1123353179455750325/image_1.png?width=1051&height=586)
![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1093471555679432815/image.png)
[![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1095085960858976387/image.png)](https://youtu.be/nu4_uiTNB5Q)    
Sponza scene with IBL PBR, SSAO, SSR and Directional Shadows

## Building
Currently supported and tested compilers are only: MSVC and Clang
- Install [Vulkan SDK](https://vulkan.lunarg.com/sdk/home).
- Run the root CMake script with this command to generate for MSVC:       
`cmake -S . -B ./build/ -G "Visual Studio 17 2022" -A x64`      
For clang:  
`cmake -B ./build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release`
- Install `directx-shader-compiler` package if required. (Only required for UNIX)
- Then run this command to build it with CMake:   
`cmake --build ./build --config Release`

## Dependencies
- [vuk](https://github.com/martty/vuk)
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)
- [GLFW](https://github.com/glfw/glfw)
- [entt](https://github.com/skypjack/entt)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [GLM](https://github.com/g-truc/glm)
- [Jolt](https://github.com/jrouwe/JoltPhysics)
- [MiniAudio](https://github.com/mackron/miniaudio)
- [Tracy](https://github.com/wolfpld/tracy)
- [tomlplusplus](https://github.com/marzer/tomlplusplus)
- [lua](https://github.com/walterschell/Lua)
- [sol2](https://github.com/ThePhD/sol2)
- [RapidJson](https://github.com/Tencent/rapidjson/tree/master)
- [NFD](https://github.com/btzy/nativefiledialog-extended)

## Special Mentions and Thanks To
- [Cem Yuksel](https://www.youtube.com/@cem_yuksel/videos) for his great videos about graphics programming.
- Jason Gregory for his [Game Engine Architecture](https://www.gameenginebook.com/) book.
- [SaschaWillems](https://github.com/SaschaWillems/Vulkan) for his Vulkan examples and help.  
