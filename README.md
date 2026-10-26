<p align="center">
    <a href="https://github.com/haylen-org/haylen-shader-compiler" target="_blank" rel="noopener noreferrer"><img width="120" src="extras/images/symbol.svg" alt=""></a>
</p>

<h1 align="center">Haylen Shader Compiler</h1>

<p align="center">
    <a href="https://github.com/haylen-org/haylen-shader-compiler/actions/workflows/ci.yml"><img src="https://github.com/haylen-org/haylen-shader-compiler/actions/workflows/ci.yml/badge.svg" alt="Haylen Shader Compiler - CI"></a>
    <a href="https://github.com/haylen-org/haylen-shader-compiler/releases/latest"><img src="https://img.shields.io/github/v/release/haylen-org/haylen-shader-compiler" alt="Release"></a>
    <a href="https://github.com/haylen-org/haylen-shader-compiler/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
    <a href="https://isocpp.org"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C.svg" alt="C++ 20"></a>
</p>

<p align="center">
One shader source in the Haylen shader language, compiled into Metal, HLSL, GLSL, GLSL ES, WGSL and SPIR-V with everything a renderer needs to bind it.
</p>

<br>

## What it is

The Haylen shader compiler turns one shader source into code for every graphics API that the [Haylen](https://github.com/haylen-org/haylen) engine runs on. You write your vertex and fragment stages once in the Haylen shader language, which is GLSL 4.50 with a few tags, and the command `haylen-shaderc` writes a Haylen shader package with Metal, HLSL, GLSL, GLSL ES, WGSL and SPIR-V code, plus the reflection of every program.

## Features

- 🧩 One source for every target, with stages, programs, shared blocks of code and includes.
- 🎯 Eight targets: Metal for macOS, iOS and the iOS simulator, HLSL for shader model 5.0, GLSL 4.30, GLSL ES 3.00, WGSL and SPIR-V for Vulkan.
- 🔎 Full reflection of every program: vertex attributes, uniform blocks with their std140 layout, textures, samplers, the texture and sampler pairs the stages sample with, and the color outputs.
- 📍 Locations that assign themselves: inputs and outputs need no `layout(location=N)`, and fragment inputs find their vertex outputs by name.
- 🧪 Variants that compile one program again with other defines, such as a lit version of a material.
- 🧭 Errors at the exact file, line and column, also inside included files.
- ⚡ Parallel compilation of every variant and target, with the same bytes for the same source every time.
- 📦 A library, `haylen::shaderc`, and a command line tool, `haylen-shaderc`.

## Releases

Every [release](https://github.com/haylen-org/haylen-shader-compiler/releases) ships `haylen-shaderc` prebuilt, so you only build it yourself to work on the compiler. Each archive holds the executable and the `LICENSE`, and is named `haylen-shaderc-<version>-<platform>`:

| Platform | Archive | Runs on |
| --- | --- | --- |
| `macos-universal` | `.tar.gz` | macOS 14.0 and later, on Apple silicon and Intel Macs. |
| `linux-x64` | `.tar.gz` | 64-bit x86 Linux with glibc 2.28 or later, such as Debian 10, Ubuntu 20.04, RHEL 8 and later. |
| `linux-arm64` | `.tar.gz` | 64-bit ARM Linux with glibc 2.28 or later, such as Debian 10, Ubuntu 20.04, RHEL 8 and later. |
| `windows-x64` | `.zip` | 64-bit x86 Windows 10 and later. |
| `windows-arm64` | `.zip` | 64-bit ARM Windows 11 and later. |

The Linux executables carry their C++ library and the Windows executables carry their C++ runtime, so none of them needs more than the system. The file `SHA256SUMS` of every release lists the SHA-256 of every archive. Check an archive you downloaded next to it with one of these commands:

```bash
sha256sum --check --ignore-missing SHA256SUMS
shasum -a 256 --check --ignore-missing SHA256SUMS
```

The first command works on Linux and the second on macOS. Haylen provides the compiler by itself: its CMake downloads the archive of the release it pins for the system and processor of the machine and checks it against the SHA-256 it pins, and builds the compiler from the source of that release on a machine the release ships no binary for.

## Quick start

To build the compiler yourself, you need CMake, Ninja, Python 3, Git and a C++20 compiler. The first configuration downloads the dependencies.

```bash
git clone https://github.com/haylen-org/haylen-shader-compiler.git
cd haylen-shader-compiler
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The tool is then at `build/haylen-shaderc`.

## A shader

Save this as `sprite.glsl`:

```glsl
@vs sprite_vs
layout(binding=0) uniform sprite_vs_params {
    mat4 view_projection;
};

in vec2 corner;
out vec2 uv;

void main() {
    uv = corner;
    gl_Position = view_projection * vec4(corner, 0.0, 1.0);
}
@end

@fs sprite_fs
layout(binding=1) uniform sprite_fs_params {
    vec4 tint;
};
layout(binding=0) uniform texture2D sprite_texture;
layout(binding=0) uniform sampler sprite_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(sprite_texture, sprite_sampler), uv) * tint;
}
@end

@program sprite sprite_vs sprite_fs
```

Compile it into a package for every target:

```bash
build/haylen-shaderc sprite.glsl --output sprite.shader
```

Or for some targets, with variants that define names for the preprocessor:

```bash
build/haylen-shaderc sprite.glsl --output sprite.shader --target msl-macos --target wgsl --variant plain= --variant lit=LIT
```

## Documentation

- [The Haylen shader language](docs/language.md) explains every tag, how inputs, outputs, uniform blocks, textures and samplers work, variants, errors and the command line.
- [The Haylen shader package format](docs/format.md) describes the `.shader` file field by field, with a complete example.

## Targets

| Target | Language | Translated by |
| --- | --- | --- |
| `msl-macos` | Metal Shading Language 2.0 for macOS | SPIRV-Cross |
| `msl-ios` | Metal Shading Language 2.0 for iOS | SPIRV-Cross |
| `msl-ios-simulator` | Metal Shading Language 2.0 for the iOS simulator | SPIRV-Cross |
| `hlsl5` | HLSL for shader model 5.0 | SPIRV-Cross |
| `glsl430` | GLSL 4.30 | SPIRV-Cross |
| `glsl300es` | GLSL ES 3.00 | SPIRV-Cross |
| `wgsl` | WGSL | Tint |
| `spirv` | SPIR-V 1.0 for Vulkan 1.1 | glslang |

Every stage compiles to SPIR-V with glslang first, and the translators read that SPIR-V. The target `spirv` keeps it, with the descriptor sets and bindings of Vulkan.

## Building on it

The project is a CMake project that other projects can add with `add_subdirectory`. It exports the library `haylen::shaderc`, whose `haylen::shaderc::Compiler` compiles a source into a `haylen::shaderc::Package` and whose `haylen::shaderc::PackageWriter` writes it. Its dependencies, glslang, SPIRV-Cross, Tint from the Dawn project, nlohmann/json and GoogleTest, come through CPM, pinned with the hashes of their archives in `cmake/haylen-shaderc-dependencies.cmake`. The tests check the targets with real compilers and validators, the SPIR-V target with the validator of SPIRV-Tools, which Dawn brings for Tint.

## License

The Haylen shader compiler is free for personal and commercial use, forever, under the [MIT License](LICENSE). Every contribution becomes part of it under the same license.
