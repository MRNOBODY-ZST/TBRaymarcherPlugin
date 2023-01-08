

# Volume Rendering (Raymarching) Plugin for Unreal Engine
Allows volume rendering of volumetric data with Unreal Engine.

# UE Version
Plugin is updated for UE 5.1, for older engine versions, check out the appropriately named branch.

# Working project
For a project that works out of the box, see example project at https://github.com/tommybazar/TBRaymarchProject.

If you want to use older UE versions, don't forget to update both the plugin and the project to the same version.

![
](https://github.com/tommybazar/TBRaymarchProject/blob/master/Documents/Render1.png)


![
](https://github.com/tommybazar/TBRaymarchProject/blob/master/Documents/Render2.png)

Both of these renders are using the same CT scan, only difference is windowing and used transfer function.


# Video showcase / deep-dive tutorials
Showcase & Intro (now mostly outdated): https://youtu.be/-HDVXehPolM

# Discord
If you want to ask me anything or (potentially) talk to other people using this plugin, here's a discord server for it : https://discord.gg/zKutZpmFXh

# Features
 * Works out of the box with vanilla UE 5.1 (there is also a branch for 4.27, 4.26, 4.25)
 * Volume raymarching for arbitrary `UVolumeTexture` textures
 * .dcm (DICOM), .mhd and .raw file import into volume textures.
 * Volume illumination implemented in compute shaders using concepts from [Efficient Volume Illumination with Multiple Light Sources through Selective Light Updates](https://ieeexplore.ieee.org/document/7156382) (2015) by Sundén and Ropinski
 * Uses color curves for transfer function definition.
 * Fully integrated and functional within UE editor viewport.
 * Windowing support (google DICOM Window center / Window Width or watch my youtube description for an explanation).
 * Basic menus for manipulating the volume.
 * Basic VR support and example map.

# Limitations
 * Raymarched volume doesn't cast or receive shadows info to/from the scene, it only self-shadows.
 * We use a very simple (but fast) raymarching and illumination algorithm with no specular, refraction or scattering.
 * Algorithm is already a bit dated and implementation leaves a lot to be desired as for efficiency. It is however, good enough for real-time applications with several lights and large (256^3 or more) volumes.

# Example
 * The plugin works out-of-the-box with everything being included in the TBRaymarcherPlugin. There is an example map for Mouse and Keyboard and an example map for VR.
 * You might need to copy the Input Bindings from the example project into your project if controls don't work.

# Using this plugin in your own project
If you want to use this functionality in your own project, you will need to
 * Copy/clone this repo
 * Copy the TBRaymarcherPlugin from the Plugins directory into your project.
 * Enable it in your  ProjectName.uproject file
 ```CSharp
    PublicDependencyModuleNames.AddRange(new string[] { 
        "Core", "CoreUObject", "Engine", "InputCore", // These are default ones you probably already have
        "Raymarcher" // <-- This is the important one ;)
        // ... other dependencies you might have
    });
```
 * Regenerate Visual Studio files (if using VS), compile and run.

# Getting started
1. Copy the plugin into your project or just compile and run the main project after cloning this repo.
2. Enable "Show Plugin Content" in your content browser.
3. Open the TBRaymarcherPlugin/Maps/TBRaymarcherShowcase map, you should see a raymarched volume in front of you.
4. By going into the `RaymarchVolume` category of settings on the RaymarchVolume, you can change various settings, I explain these in detail in my YT tutorial.
5. When you "Play" the level, a basic UI will be spawned (check the level blueprint to see how it's spawned) and you can play with the transfer functions and load a different volume from disk.
6. If you have a VR headset, check out the TBRaymarcherVRShowcase, you can grab and move the volume, clipping plane and lights with the grip button. Right hand also has a widget interactor on it to work with VR menus. This is just quickly hacked together and should probably not be used as a basis for an actual VR app, unless you want to feel some pain down the road.

# Plugin Structure
Most of the functionality is implemented in C++ with the most high-level functionality exposed to blueprints.
If you want to tinker with any low-level stuff, you will need C++ (and also probably HLSL) experience.

Logically, the project can be separated into:
 - Texture toolkit, wrapping the commonly used functionality for creating/working with VolumeTextures and DICOM/MHD loading utilities
 - Actual raymarching materials (shaders)
 - Illumination computation compute shaders (and C++ code wrapping them)
 - C++ classes putting these together to form a useable RaymarchedVolume
 - Labeling shaders  (and C++ code wrapping them) (Coming soon)
 
 We will now describe these individually.
 
## Texture Toolkit & DICOM/MHD loading
Located in the `/VolumeTextureToolkit` and `/VolumeTextureToolkitEditor` plugin modules, these contain various low-level utilities
for loading .raw files into volumes, convenience functions for creating VolumeTextures from said .raw files, conversion functions
to process data when it is imported etc.
I tried to be very generous with comments, so check out `TextureUtilities.h`/`.cpp` and see for yourself.

### Volume loading
All functionality discussed in this section can be found in `VolumeTextureToolkit/Public/VolumeAsset/VolumeAsset.h` and `VolumeTextureToolkit/Public/VolumeAsset/Loaders/VolumeLoader.h` 
and it's inherited classes (MHD and DICOM loaders).

We support loading MHD and DICOM files into a `UVolumeAsset` data asset. 

When loading volumetric files, the user must choose if they want them normalized and converted to G8/G16 (grayscale 8bit or 16bit) format.
This allows the textures to be persistently saved in Unreal, but comes at the cost of being forced to normalize the file to 0-1 range.
To conserve the original values, we keep the minimum and maximum value encountered in the volume when it was loaded, so after normalization,
value of 0 in the texture corresponds to `ImageInfo.MinValue` within the UVolumeAsset and value of 1 in the texture corresponds to `ImageInfo.MaxValue` in the asset.

This is not necessary if you don't mind your loaded assets not being persistent, then you can load the file as a R32Float and keep the original values.
I'm currently investigating how to get around this limitation by making my own `UVolumeTexture`-like asset instead of using the `UVolumeTexture` assets directly.

See `CreateVolumeFromFile` and `CreatePersistentVolumeFromFile` functions respectively for both of these loading types.

We also support drag'n'drop asset import. If you drag a file with a .dcm or .mhd extension into the content browser, a `UVolumeAsset` and a corresponding `UVolumeTexture` will
be created in the current folder. User will be prompted if they want the data to be normalized or converted into float, which results in same behaviour as described previously. 
See `VolumeTextureEditor/Public/VolumeTextureFactory.h` and associated .cpp file for implementation details.


## Actual raymarching materials
We created raymarching materials that repeatedly sample a volume texture along a ray, transfer the read value by a transfer function, take lighting into account and then accumulate the color samples into a final pixel color.

All materials used for raymarching are in `Content/Materials` with `M_Raymarch` and `M_Intensity_Raymarch` being the most interesting. I also have similar materials that support labeled volumes in our old project, but didn't get around to cleaning them up and integrating them in the new project yet.'

The material graphs aren't very complex, as we do all our calculations in code, so the materials are pretty much just wrappers for our custom HLSL code and to pass parameters into the code.

Also, because using Custom nodes in material editor is a pain, we implemented our material functions in .usf files inside our plugin and then include and call these functions from within Custom nodes.

You can see the way we hack the functions we need included into the compiled shader in the `Include` custom node. Credit for this beautiful hack goes here
[http://blog.kiteandlightning.la/ue4-hlsl-shader-development-guide-notes-tips/](http://blog.kiteandlightning.la/ue4-hlsl-shader-development-guide-notes-tips/).

We based our implementation on [Ryan Brucks' raymarching](https://shaderbits.com/blog/creating-volumetric-ray-marcher) materials from way back when Volume Textures weren't a thing in Unreal. We made some modifications, most notably we added support for a cutting plane to cut away parts of the volume. 

Another modification is that we use unit meshes (a cube centered at 0, going from -0.5 to 0.5, so we don't need to care about mesh bounds of the used cube.

We also improved the depth calculations, so that depth-ordering works with arbitrary transforms of the raymarched volume (notably anisotropic scaling didn't work before).

### Other formats
If you have raw Volume Texture data saved on disk and you already know the dimensions,  use `LoadRawArrayIntoNewVolumeTexture` or `LoadRawArrayIntoNewVolumeTexture` to create a uint8* array from them and then use the above mentioned functions to create Volume Texture assets from them.

### Actual raymarching

We tried to be generous with comments, so see `TBRaymarcherPlugin/Shaders/Private/RaymarchMaterialCommon.usf`, `TBRaymarching/Shaders/Private/WindowedSampling.usf` and `TBRaymarching/Shaders/Private/WindowedRaymarchMaterials.usf` for the actual code and explanation. 
The `WindowedSampling.usf` and `WindowedRaymarchMaterials.usf` actually contain the code that is used in the final materials, as the other implementations don't support windowing. They are included for legacy reasons (they are slightly less complicated too, if you don't need the windowing option).

I heartily recommend using "HLSL Tools for Visual Studio" (if you're using VS) or other plugins for syntax highlighting of the .usf files.

The actual raymarching is a 2-step process. First part is getting the entry point and thickness at the current pixel. See `PerformRaymarchCubeSetup()`to see how that's done.
Second part is performing the actual raymarch. See `PerformWindowedLitRaymarch()` for the simplest raymarcher.

We implemented 4 raymarch materials in this plugin.
`PerformWindowedLitRaymarch()` - standard raymarching using Transfer Functions and an Illumination volume.
`PerformWindowedIntensityRaymarch()` - isn't true raymarching, instead when the volume is first hit, the intensity of the volume is directly transformed into a grayscale value depending on the selected window and returned. We used this to be able to show the underlying volumes' data directly. 

 * The following is not implemented in this project yet, I have the code in the old project but didn't have time to clean it yet.
For both of these, there is also a "labeled" version, which also samples a label volume and mixes the colors from the label volume with the colors sampled from the Data volume.
The labels aren't lit, don't cast a shadow and they're not affected by the cutting plane.
See `PerformLitRaymarchLabeled()` and `PerformIntensityRaymarchLabeled()`.

 * Supported (and tested) formats for the textures raymarched are G8, G16 and R32F textures.

### Shared functions
As you can see in `WindowedRaymarchMaterials.usf`, we include common functions from `RaymarcherCommon.usf` and `WindowedSampling.usf`

The latter 2 files contains functions that are used both in material shaders and in compute shaders computing the illumination. Using the same functions assures consistency between opacity perceived when looking at the volume and the strength of shadows cast by the volume.  

A notable function here is the `SampleWindowedVolumeStep()`function. This samples the volume at the given UVW position. Then it transforms the value according to the provided Windowing parameters. These are identical with the windowing parameters found in the DICOM standard - Window Center, Window Width.
In short, these specify a "Window" of values that are interesting to us. A value of [WindowCenter - WindowWidth/2] will map to 0 (sample left side of the transfer function texture) and a value of [WindowCenter + WindowWidth/2] will map to a value of 1 (will sample the right side of transfer function).
 Afterwards, the Transfer function texture is sampled at a position corresponding to the sampled intensity. The alpha of the color sampled from the Transfer Function is then modified by an extinction formula to accomodate for the size of the raymarching step. (Longer step means more opacity, same as in `CorrectForStepSize()` function) 


## Illumination computation compute shaders (and C++ code wrapping them)
As mentioned above, we implement a method for precomputed illumination as in the paper s from the paper "Efficient Volume Illumination with Multiple Light Sources through Selective Light Updates". Unlike them, we contended with a single channel illumination volume (not a RGB light volume to accomodate for colored lights).
We also didn't implement some of the optimizations mentioned in the paper - notably joining passes of different lights that go in the same axis into one pass (this would require a rewrite of the compute shaders and the whole blueprint/C++ hierarchy).

The shaders are declared in `Raymarcher/Rendering/LightingShaders.h`. You can notice that the shaders have an inheritence hierarchy and only the non-abstract shaders are declared and implemented with the UE macro `DECLARE_SHADER_TYPE() / IMPLEMENT_SHADER_TYPE()`

The two shaders that actually calculate the illumination and modify the illumination volume are `FAddDirLightShader` and `FChangeDirLightShader`. 

The .usf files containing the actual HLSL code of the shaders is in `/Shaders/Private/AddDirLightShader.usf` and `ChangeDirLightShader.usf`

It's important to note that a single invocation of the shader only affects one slice of the light volume. The shaders both need to be invoked as many times as there are layers in the current propagation axis. This is because we didn't find a way to synchronize within the whole propagation layer (as compute shaders can only be synchronized within one thread group and the whole slice of the texture doesn't fit in one group). Read the paper for more info on how we use the read/write buffers and propagate the light per-layer.

We implemented the AddDirLight shader and a ChangeDirLight shader. The difference being that the ChangeDirLight shader takes `OldLightParameters` and `NewLightParameters` to change a light in a single pass.

There is a "fast" shader option, implemented in FAddDirLightShader_GPUSync, which does synchronization within the shader, so it's only invoked once per axis, but it still has some synchronization issues and the results are unstable and keep flickering.
You can enable that by toggling the "Fast Shader" toggle on a RaymarchVolume.

A (kind of a) sequence diagram here shows the interplay of blueprints, game thread, render thread and RHI thread.

![
](https://github.com/tommybazar/TBRaymarchProject/blob/master/Documents/sequence.png)

Unlike in the paper, we don't perform the optimization of joining all light sources coming from the same axis into one propagation pass. It would be possible to do this by changing the shaders to use Structured Buffers with an arbitrary number of lights as inputs, but a rewrite of the whole plugin structure would be required for that (instead of static blueprints working on a single light each, C++ code that checks for all the changed lights, queues them up and then invokes the shader passes for the changed axes). I didn't have the time to do this rewrite and the way it is now is a bit more obvious in how everything works.
This will potentially change in the near future, as it's not a terribly complex optimization to make and it would make the shaders a lot faster, especially when using many lights). 

I also have an idea for a completely different approach that would make the whole pass in one compute shader call, but need to implement and test it first, since applying a completely new approach seems wiser than slightly improving a 5-year old method that wasn't originally designed with compute shaders in mind.

## Raymarch volume, Raymarch Light, Raymarch Clip Plane
The class holding all of the above together and providing the full raymarching functionality is `ARaymarchVolume.h`.

Combine this with the `ARaymarchClipPlane` and `ARaymarchLight` to get the full package. Without any lights assigned, the raymarched volume will be pitch-black.
These functions are wrapped in blueprints in `TBRaymarcherPlugin/Content/Blueprints/Actor` that can be drag'n'dropped into the scene.

### Properties exposed to blueprints

All properties that are exposed to blueprints are under `Raymarch Volume` category in the ARaymarchVolume actor. All are editable in the Actor properties, but unless otherwise specified, they can not be directly 
set from blueprints (`BlueprintReadOnly`). Many can be set by blueprints during runtime indirectly by calling the functions described in the next sub-section.

`VolumeAsset` - change this to make the volume display a completely different Volume Asset.

`Lit Raymarch Material Base` - Change this to assign a different base material for lit raymarching

`Intensity Raymarch Material Base` - Change this to assign a different base material for pure Intensity raymarch (no Transfer function)

`Clipping plane` - Assign a clipping plane affecting this volume. Can be null. Is `BlueprintReadWrite`.

`Lights Array` - Add as many RaymarchLights as you feel like that should be affecting the volume. Is `BlueprintReadWrite`

`Lit Raymarch` - Toggle to set if the volume will be rendered with true lighting and volumetric rendering or simply as a cut through CT data.

`RaymarchResources->LightVolumeHalfResolution` - if toggled on, light volume will have half resolution than the original volume. This results in a massive speedup, at the cost of more noticeable artifacts.

`Windowing parameters` - change to modify the windowing function and enable/disable low/high cutoff. 

`Raymarching steps` - lower step count leads to better performance at the cost of visual quality.

### Functions exposed to blueprints
All BP functions use the same `Raymarch Volume` category as above.

`LoadNewFileIntoVolumeTransientR32F()` - Loads a volume file from the given file as a R32F (not-normalized, float32, transient) and assigns it to the volume.

`LoadNewFileIntoVolumeNormalized()` - Loads a volume file from the given file as a G8/G16 normalized volume (transient) and assigns it to the volume.

`SetTFCurve()` - Sets the specified `UCurveLinearColor` as the currently used Transfer Function.

`GetMinMaxValues()` - Returns the Minimum and Maximum value that the volume had before being normalized.

`SetWindowCenter() / SetWindowWidth() / SetLowCutoff() / SetHighCutoff()` - Sets the given windowing parameter.

### In-editor volume editing
When you have a volume set-up with an assigned Volume Asset, we took great care that it would behave very well and be very responsive in-editor.

Firstly, the volume ticks even in-editor, so it can react to changes made while editing it's or it's VolumeAsset's properties.

Also, modifying anything in the assigned VolumeAsset's ImageInfo structure (most notably default windowing parameters) will be immediately visible in all volumes that are currently displaying that MHDAsset. 

Furthermore, changes to the VolumeAsset's Transfer Function `UCurveLinearColor` will be immediately visible on all volumes using the VolumeAsset. This is achieved by hooking onto the `OnGradientChanged` delegate that `UCurveLinearColor` exposes in-editor.

Lastly, lights and clipping planes assigned to the `URaymarchedVolume` will update in-editor just as they would in-game.

This is all achieved through delegates and PostEditChangeProperty() overrides, more interested users are encouraged to peruse the source codes at their leisure.

### Widgets
We provide basic widgets (located in 'TBRaymarcherPlugin/Content/Blueprints/Widgets' that can be used to modify the Raymarched Volume at runtime. These are explained in my youtube tutorial videos, 
only thing especially worth noting is that if you create them yourself, they need the 'SetRaymarchVolume' function to be run on them and 
a volume assigned, otherwise they won't do anything.

# Platforms
Currently only Windows DX11 is supported and tested. DX12 has some synchronization issues that can result in textures being corrupted after startup.
Vulkan crashes on startup immediately and since UE support for it isn't mature yet, I'm not fixing it. If you're an expert who really wants to use Vulkan, feel free to submit a pull-request after you find the reason for crashes.

**Only the master branch is updated with new features, branches dedicated to older UE versions are legacy and won't be updated into the future.' 

# Credits
Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

I'd also like to appreciate my alma mater: [Technical University of Munich](https://www.tum.de/en), [Chair of Computer Aided Medical Procedures](http://campar.in.tum.de/WebHome), where I created most of the original code as my master's thesis, before performing a massive clean-up, reorganization and improvements throughout 2020.

Check out their study programmes and apply, tuition is free for everybody, they have great connections to research and industry and it is one of the leading universities in Europe, as far as Computer Science is concerned.

Our old project can be found at [my supervisor's github](https://github.com/TheHugeManatee/UE4_VolumeRaymarching). Feel free to check out his other UE plugins.

# License 
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Licensed under MIT license.

Both me and Technical University of Munich are copyright holders, as major parts of this software were written as part of my thesis and/or working-student employment for TUM.

See LICENSE file for full license text.

## Example VolumeAsset files
In `TBRaymarcherPlugin/Content/DefaultResources/` are VolumeAsset and volume textures created by importing data from Subset0 of [LUNA2016 grand challenge](https://luna16.grand-challenge.org/download/).
The data is taken from publicly available LIDC/IDRI database and uses [CC Attribution 3.0 Unported License](https://creativecommons.org/licenses/by/3.0/).

## DICOM loading
DICOM loading is utilizing a modified version of VTK DicomParser, made by Matt Turek. See  License.txt in /Source/VolumeTextureToolkit/Public/VolumeAsset/DICOMParser/License.txt for full license text.