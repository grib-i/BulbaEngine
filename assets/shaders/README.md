# Shader modules

The `passes/*.vert` / `passes/*.frag` files are the only entry points; their basenames remain stable embedded shader IDs. Build them with `glslc -Iassets/shaders`; do not compile `.glsl` modules on their own.

- `common/lights.glsl`: shared light types and std430 buffer layout; matches `VulkanGPULight` / `VulkanLightBufferData`.
- `basic3d/interface.glsl`: fragment inputs, material slots, descriptor bindings and push constants.
- `basic3d/textures.glsl`: UV projection, texture transforms and skybox atlas helpers.
- `basic3d/packing.glsl`: packed material decoding.
- `basic3d/lights.glsl`: range attenuation and spot-cone evaluation.
- `basic3d/shadows.glsl`: selected-light visibility, six-face lookup and PCF.
- `basic3d/brdf.glsl`: Fresnel, GGX and geometry terms.
- `basic3d/normal.glsl`: tangent frame and normal mapping.
- `basic3d/emission.glsl`: blackbody color.
- `basic3d/surface.glsl`: packed surface colors.
- `basic3d/fragment.glsl`: material evaluation and light accumulation.
- `basic3d/vertex.glsl`: object, skybox and instanced geometry transforms.
- `basic2d/interface.glsl`: 2D descriptor/push-constant contract.
- `basic2d/lighting.glsl`: pixel/world-range attenuation and spot cones.
- `basic2d/vertex.glsl`, `basic2d/fragment.glsl`: sprite transforms and shading.
- `passes/postprocess.*`: fullscreen postprocess; built from GLSL with the other passes, no checked-in SPIR-V byte array.

Include order in `passes/basic3d.frag` is intentional. Shader formatting inherits LLVM/150 from the project but disables include sorting. CMake tracks transitive includes through glslc depfiles, so changes rebuild the embedded SPIR-V automatically.
