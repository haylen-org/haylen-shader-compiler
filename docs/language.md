# The Haylen shader language

The Haylen shader language is GLSL 4.50 with a few tags around it. You write the vertex stage and the fragment stage of your shaders once, in the GLSL dialect that SPIR-V compilers read, with separate `texture2D` and `sampler` objects and a `layout(binding=N)` on every uniform block, texture and sampler. The tags split one file into stages, pair the stages into programs, share code between stages and files, and describe textures and samplers. The command `haylen-shaderc` compiles a source into a Haylen shader package with code for Metal, HLSL, desktop GLSL, GLSL ES, WGSL and SPIR-V, which [the format guide](format.md) describes.

## A first shader

The file below holds a whole shader. It draws a textured quad tinted by a color.

```glsl
// Draws a textured quad tinted by a color.
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

Compile it with this command:

```bash
haylen-shaderc sprite.glsl --output sprite.shader
```

## Tags

A tag is a line whose first character outside comments is `@`. The tag is the first word of the line, and the words after it are its arguments. A `//` comment may follow the arguments, and tags inside comments are never read, so you can comment a tag out like any other line.

```glsl
@vs sprite_vs // The vertex stage of the sprite.
// @fs old_fs is a comment, not a tag.
```

The compiler reads the tags before the C preprocessor of GLSL runs, so `#if` and `#ifdef` never hide a tag. They apply to the code between the tags as usual.

The language has these tags, and any other word that starts with `@` at the start of a line is an error:

| Tag | Where | What it does |
| --- | --- | --- |
| `@vs <name>` | Outside sections | Starts a vertex stage. |
| `@fs <name>` | Outside sections | Starts a fragment stage. |
| `@block <name>` | Outside sections | Starts a block of code that stages and other blocks include. |
| `@end` | Inside a section | Ends the stage or block that is open. |
| `@include <path>` | Outside sections | Reads the tags of another file. |
| `@include_block <name>` | Inside a section | Pastes a block where the tag is. |
| `@program <name> <vertex stage> <fragment stage>` | Outside sections | Pairs a vertex stage and a fragment stage into a program. |
| `@sample_type <texture> <type>` | Inside a section | Sets the sample type of a texture. |
| `@sampler_type <sampler> <type>` | Inside a section | Sets the type of a sampler. |

Stages and blocks are the sections of a file. Outside sections a file holds only tags, comments and empty lines, so code outside a section is an error, since it would never compile.

Names of stages, blocks and programs are identifiers: they start with a letter or an underscore and go on with letters, digits and underscores. Stages share one set of names, blocks have another and programs a third, so a block may take the name of a stage.

## Stages

The tags `@vs` and `@fs` start a vertex stage and a fragment stage, and `@end` closes them. Everything between the tags is the GLSL of the stage, with a `void main()` function as its entry point. The compiler puts `#version 450` in front of every stage itself, so a stage never starts with a `#version` directive.

```glsl
@vs fullscreen_vs
out vec2 uv;

void main() {
    uv = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1) * 2.0;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
@end
```

A stage compiles only when a program uses it, so a file may hold stages that only some programs, or none, use.

## Programs

The tag `@program` pairs a vertex stage with a fragment stage under a name, which is the name the program takes in the package. A file declares at least one program, and the stages may come before or after the tag. One stage may serve several programs.

```glsl
@program composite fullscreen_vs composite_fs
@program filter fullscreen_vs filter_fs
```

## Blocks

A block is code that stages and other blocks paste in with `@include_block`. Blocks hold the functions, uniform blocks, textures, inputs and outputs that several stages share, and a block may include other blocks. A block that includes itself, directly or through other blocks, is an error.

```glsl
@block lighting
float lighting_lambert(vec3 normal, vec3 light) {
    return max(dot(normalize(normal), normalize(light)), 0.0);
}
@end

@fs lit_fs
@include_block lighting
in vec3 normal;
out vec4 frag_color;

void main() {
    frag_color = vec4(vec3(lighting_lambert(normal, vec3(0.0, 0.0, 1.0))), 1.0);
}
@end
```

## Including files

The tag `@include` reads another file and makes its stages, blocks and programs part of the source. The path is the rest of the line, so it may hold spaces. A relative path resolves against the folder of the file that includes it first, and then against every folder that `--include` names, in the order the command line gives them. An absolute path is used as it is.

```glsl
@include haylen/material.glsl
@include common.glsl
```

Every file is read once, however many files include it, so two files may include the same library without declaring its blocks twice. The tag `@include` is only allowed outside sections, and the stages and blocks of a file always end in that file.

## Inputs and outputs

Inputs and outputs need no locations in the source, since the compiler gives them their locations:

- The inputs of the vertex stage, which are the vertex attributes, take locations from 0 in the order the stage declares them.
- The outputs of the vertex stage take locations from 0 in the order the stage declares them.
- Every input of the fragment stage takes the location of the output of the vertex stage with the same name, so the fragment stage may declare its inputs in any order and may leave outputs of the vertex stage out.
- The outputs of the fragment stage, which are its color outputs, take locations from 0 in the order the stage declares them.

A declaration may still give its own location with `layout(location=N)`, and the declarations without one take the first free locations after it. An input of the fragment stage with a location needs the same location as the output it reads.

```glsl
@vs lines_vs
in vec2 position;
in vec4 color0;
out vec4 color;
out float fade;

void main() {
    color = color0;
    fade = position.y;
    gl_Position = vec4(position, 0.0, 1.0);
}
@end

@fs lines_fs
in float fade;
in vec4 color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = color * fade;
}
@end
```

Here `position` takes location 0 and `color0` location 1, `color` takes location 0 and `fade` location 1 in both stages, and `frag_color` writes the first color output.

An output of the vertex stage that the fragment stage leaves out stays inside the vertex stage as a private variable. Every target then passes only the values that the fragment stage declares, and both stages of the HLSL target list the same values in the same order, which Direct3D needs to link them.

Vertex inputs are `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4`, `uint`, `uvec2`, `uvec3` or `uvec4`. Every input of the fragment stage has the type of the output with the same name, and an input without such an output is an error. Input and output blocks are not supported, so every input and output is a variable of its own.

## Uniform blocks

A uniform block holds the values that the program reads for a draw. Every uniform block has a binding from 0 to 7, which is its slot, and no two uniform blocks of a program share a slot, whichever stage declares them. The members of a block follow the std140 layout, and the block takes whole vectors of 16 bytes, so the size of a block is the end of its last member rounded up to a multiple of 16.

```glsl
layout(binding=2) uniform water_params {
    vec4 deep_color;
    vec2 flow;
    float time;
    float weights[3];
};
```

Members are `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4` or `mat4`, or arrays of them. In the block above, `deep_color` starts at byte 0, `flow` at byte 16, `time` at byte 24 and `weights` at byte 32, where every element of an array takes 16 bytes, and the block takes 80 bytes.

The GLSL targets hold every uniform block as one array of vectors named after the block, so a block holds either floats or integers. A block that mixes `float` members with `int` members is an error when the compilation includes a GLSL target, and it compiles for the other targets.

## Textures and samplers

Textures and samplers are separate objects, which the stage combines where it samples:

```glsl
layout(binding=0) uniform texture2D color_texture;
layout(binding=0) uniform sampler color_sampler;

vec4 color_at(vec2 uv) {
    return texture(sampler2D(color_texture, color_sampler), uv);
}
```

Textures are `texture2D`, `texture2DArray`, `texture3D` or `textureCube`, and an `i` or a `u` in front makes them hold signed or unsigned integers, such as `itexture2D`. Samplers are `sampler`, or `samplerShadow` for samplers that compare depths. Textures bind from 0 to 31 and samplers from 0 to 11, and like uniform blocks no two textures, or two samplers, of a program share a slot. Every pair of a texture and a sampler that a stage combines becomes a texture sampler pair of the program, named after the texture and the sampler joined by an underscore, such as `color_texture_color_sampler`.

### Sample types

The sample type of a texture tells the graphics API how the program reads it:

| Sample type | Meaning |
| --- | --- |
| `float` | Floats that samplers may filter. |
| `unfilterable_float` | Floats that only samplers without filtering read, such as the texels of a 32-bit float texture. |
| `depth` | Depths. |
| `sint` | Signed integers. |
| `uint` | Unsigned integers. |

A texture that holds floats has the sample type `float`, an `itexture` has `sint` and a `utexture` has `uint`. The tag `@sample_type` gives a texture that holds floats the sample type `unfilterable_float` or `depth` instead:

```glsl
@fs light_fs
@sample_type shadow_texture unfilterable_float
@sampler_type shadow_sampler nonfiltering
layout(binding=3) uniform texture2D shadow_texture;
layout(binding=2) uniform sampler shadow_sampler;
// ...
@end
```

### Sampler types

| Sampler type | Meaning |
| --- | --- |
| `filtering` | A sampler that may filter. |
| `nonfiltering` | A sampler that never filters, for textures whose sample type is `unfilterable_float`. |
| `comparison` | A sampler that compares depths. |

A `sampler` has the type `filtering`, and the tag `@sampler_type` gives it the type `nonfiltering` instead. A `samplerShadow` has the type `comparison`.

### Where the tags go

The tags `@sample_type` and `@sampler_type` go inside a stage or a block. A tag belongs to the stage it is written in, and a tag in a block belongs to every stage that includes the block, so a library can keep a texture and its sample type together. A tag that names a texture or a sampler that its stage never declares, in any variant, is an error, and so is a tag that gives a texture or a sampler a type that its declaration does not allow, or a second tag that gives the same texture or sampler another type.

## Variants

A variant compiles the one program of a file once more with some names defined for the C preprocessor, and names that program in the package. The option `--variant` takes the name of the variant and, after an `=`, the defines separated by commas, and every define is defined as `1`:

```bash
haylen-shaderc sprite.glsl --output sprite.shader --include shaders/include \
    --variant sprite= \
    --variant sprite_lit=HAYLEN_LIT \
    --variant batch=HAYLEN_BATCH \
    --variant batch_lit=HAYLEN_BATCH,HAYLEN_LIT
```

The stages test the defines with the preprocessor:

```glsl
#ifdef HAYLEN_LIT
layout(binding=1) uniform texture2D normal_texture;
#endif
```

Variants need a file with exactly one `@program`, and the package then holds one program for every variant, under the name of the variant and in the order of the command line. Without variants the package holds every program of the file under its own name, compiled without defines. Variant names are made of letters, digits, underscores and dashes, and defines are identifiers.

## Targets

The compiler translates every program for every target, or for the targets that `--target` names:

| Target | Language | Uniform blocks | Textures | Samplers |
| --- | --- | --- | --- | --- |
| `msl-macos` | Metal Shading Language 2.0 for macOS | `[[buffer(n)]]`, with `n` counted from 0 in each stage in the order of the slots | `[[texture(n)]]`, with `n` counted from 0 in each stage in the order of the slots | `[[sampler(n)]]`, with `n` counted from 0 in each stage in the order of the slots |
| `msl-ios` | Metal Shading Language 2.0 for iOS | The same as `msl-macos` | The same as `msl-macos` | The same as `msl-macos` |
| `msl-ios-simulator` | Metal Shading Language 2.0 for the iOS simulator | The same as `msl-macos` | The same as `msl-macos` | The same as `msl-macos` |
| `hlsl5` | HLSL for shader model 5.0 | `register(bn)`, with `n` counted from 0 in each stage in the order of the slots | `register(tslot)` | `register(sslot)` |
| `glsl430` | GLSL 4.30 | One `uniform vec4 <block>[N]` or `uniform ivec4 <block>[N]` array | One `sampler2D` or other combined sampler named `<texture>_<sampler>` for each pair | Combined with the textures |
| `glsl300es` | GLSL ES 3.00 | The same as `glsl430` | The same as `glsl430` | Combined with the textures |
| `wgsl` | WGSL | `@group(0) @binding(slot)` | `@group(1) @binding(slot)` | `@group(1) @binding(64 + slot)` |
| `spirv` | SPIR-V 1.0 for Vulkan 1.1 | `layout(set=0, binding=slot)` | `layout(set=1, binding=slot)` | `layout(set=1, binding=64 + slot)` |

The Metal targets read vertex inputs through `[[stage_in]]` with `[[attribute(location)]]`, so the vertex buffers bind wherever the vertex layout of the pipeline puts them. The HLSL target gives every vertex input the semantic `TEXCOORD` with its location as the index. Its `dFdx`, `dFdy` and `fwidth`, and their fine forms, take the fine derivatives of Direct3D, `ddx_fine` and `ddy_fine`, since the plain derivatives of shader model 5.0 are coarse while the other graphics APIs compute them for every pixel. The coarse forms stay coarse. The GLSL targets keep the names of the vertex inputs and map the depth of clip space to the range of OpenGL, so every target shares one clip space. The WGSL target lets derivatives run in control flow that differs between invocations, as GLSL does. Its `dFdx`, `dFdy` and `fwidth`, and their fine forms, become `dpdxFine`, `dpdyFine` and `fwidthFine`, since the plain derivatives of WebGPU may be coarse, and the coarse forms stay coarse. The SPIR-V target holds the modules that glslang writes, with the vertex inputs at their locations and the clip space of the source, which a Vulkan device draws upright with a viewport of negative height.

A vertex stage of a program that draws points writes their size to `gl_PointSize`, which Metal, Vulkan and OpenGL read. The HLSL and WGSL targets leave it out, since Direct3D and WebGPU draw every point one pixel wide.

## Errors

The compiler reports every error as a line with the file, the line and the column where it is, followed by a sentence that says what is wrong:

```text
lib/common.glsl:4:20: error: Undeclared identifier ("undefined_name").
shader.glsl:15:18: error: The stage "main_fs" is a fragment stage, but the second name of "@program" is the vertex stage.
```

The place is always the one of the file that holds the code, also for code that a block pastes into a stage from another file. Errors of GLSL come from glslang, whose messages the compiler writes as sentences with the token they are about in parentheses. Errors that belong to a whole file, such as a file without programs or a file that cannot be read, name only the file.

The compiler reports the errors of the tags of every file first, then the errors of every stage, and then the errors of the targets, and it writes no package while there are errors.

## What the language leaves out

The language covers what a program of a 2D renderer reads, so these parts of GLSL are errors:

- Storage buffers, storage images and push constants.
- Combined samplers such as `sampler2D` in a declaration, since textures and samplers are separate objects that the stage combines where it samples.
- Arrays of textures or samplers, multisampled textures, cube map arrays and 1D textures.
- Descriptor sets, since bindings alone place every resource.
- Input and output blocks.
- Uniform block members of other types than the ones above, such as `mat3`, `uint` or structures.

## The command line

```text
haylen-shaderc <input.glsl> --output <file.shader> [--include <dir>]... [--variant <name>[=DEFINE[,DEFINE...]]]... [--target <name>]...
```

| Option | Meaning |
| --- | --- |
| `--output <file>` | The package to write. |
| `--include <dir>` | A folder where `@include` looks for files after the folder of the file that includes them. Repeat it for more folders. |
| `--variant <name>[=DEFINE,...]` | Compiles the one program of the source as the program `<name>` with the defines given. Repeat it for more variants. |
| `--target <name>` | Compiles for this target only. Repeat it for more targets. Without it, every target compiles. |
| `--help` | Shows the usage. |
| `--version` | Shows the version. |

The command compiles the programs and the targets in parallel and writes the package to a temporary file next to the output, which then replaces the output at once, so a reader never sees half a package. The same source and the same options always give the same bytes. The command exits with 0 when it writes the package, with 1 when the source does not compile or the package cannot be written, and with 2 when the arguments are wrong.
