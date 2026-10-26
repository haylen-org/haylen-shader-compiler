# The Haylen shader package format

A Haylen shader package, a `.shader` file, holds everything a renderer needs to make the programs of one shader source on any graphics API: the source of every stage for every target, the reflection of every program and the binding of every resource for every target. The command `haylen-shaderc` writes it from a source of [the Haylen shader language](language.md).

The package is one JSON object in UTF-8, written compactly without spaces or line breaks, with its keys in the order this guide lists them. The format version that this guide describes is 2. The same source and the same options always give the same bytes, so packages can be compared and cached by their contents.

## A complete example

The source below declares one program with a uniform block in each stage, one texture and one sampler.

```glsl
@vs sprite_vs
layout(binding=0) uniform sprite_vs_params {
    mat4 view_projection;
    float premultiply;
};

in vec2 corner;
in vec4 color0;
out vec2 uv;
out vec4 color;

void main() {
    uv = corner;
    color = color0;
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
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(sprite_texture, sprite_sampler), uv) * color * tint;
}
@end

@program sprite sprite_vs sprite_fs
```

The command `haylen-shaderc sprite.glsl --output sprite.shader` writes the package below, shown here with spaces and line breaks and with the twelve stage sources shortened to their start.

```json
{
  "format": "haylen-shader",
  "version": 2,
  "name": "sprite",
  "sources": [
    "#include <metal_stdlib> ... the Metal source of sprite_vs",
    "#include <metal_stdlib> ... the Metal source of sprite_fs",
    "cbuffer sprite_vs_params : register(b0) ... the HLSL source of sprite_vs",
    "cbuffer sprite_fs_params : register(b0) ... the HLSL source of sprite_fs",
    "#version 430 ... the GLSL 4.30 source of sprite_vs",
    "#version 430 ... the GLSL 4.30 source of sprite_fs",
    "#version 300 es ... the GLSL ES 3.00 source of sprite_vs",
    "#version 300 es ... the GLSL ES 3.00 source of sprite_fs",
    "diagnostic(off, derivative_uniformity); ... the WGSL source of sprite_vs",
    "diagnostic(off, derivative_uniformity); ... the WGSL source of sprite_fs",
    "AwIjBwAAAQALAAgAKgAAAAAAAAARAAIAAQAAAAsABgABAAAAR0xTTC5zdGQu ... the SPIR-V module of sprite_vs",
    "AwIjBwAAAQALAAgAJgAAAAAAAAARAAIAAQAAAAsABgABAAAAR0xTTC5zdGQu ... the SPIR-V module of sprite_fs"
  ],
  "programs": {
    "sprite": {
      "attributes": [
        {"location": 0, "name": "corner", "type": "vec2"},
        {"location": 1, "name": "color0", "type": "vec4"}
      ],
      "blocks": [
        {
          "name": "sprite_vs_params",
          "stage": "vertex",
          "slot": 0,
          "size": 80,
          "members": [
            {"name": "view_projection", "type": "mat4", "count": 1, "offset": 0},
            {"name": "premultiply", "type": "float", "count": 1, "offset": 64}
          ]
        },
        {
          "name": "sprite_fs_params",
          "stage": "fragment",
          "slot": 1,
          "size": 16,
          "members": [
            {"name": "tint", "type": "vec4", "count": 1, "offset": 0}
          ]
        }
      ],
      "textures": [
        {"name": "sprite_texture", "stage": "fragment", "slot": 0, "type": "2d", "sampleType": "float"}
      ],
      "samplers": [
        {"name": "sprite_sampler", "stage": "fragment", "slot": 0, "type": "filtering"}
      ],
      "pairs": [
        {"stage": "fragment", "texture": 0, "sampler": 0, "name": "sprite_texture_sprite_sampler"}
      ],
      "outputs": 1,
      "targets": {
        "msl-macos": {
          "vertex": {"source": 0, "entry": "main0"},
          "fragment": {"source": 1, "entry": "main0"},
          "blocks": {"0": 0, "1": 0},
          "textures": {"0": 0},
          "samplers": {"0": 0}
        },
        "msl-ios": {
          "vertex": {"source": 0, "entry": "main0"},
          "fragment": {"source": 1, "entry": "main0"},
          "blocks": {"0": 0, "1": 0},
          "textures": {"0": 0},
          "samplers": {"0": 0}
        },
        "msl-ios-simulator": {
          "vertex": {"source": 0, "entry": "main0"},
          "fragment": {"source": 1, "entry": "main0"},
          "blocks": {"0": 0, "1": 0},
          "textures": {"0": 0},
          "samplers": {"0": 0}
        },
        "hlsl5": {
          "vertex": {"source": 2, "entry": "main", "profile": "vs_5_0"},
          "fragment": {"source": 3, "entry": "main", "profile": "ps_5_0"},
          "blocks": {"0": 0, "1": 0},
          "textures": {"0": 0},
          "samplers": {"0": 0}
        },
        "glsl430": {
          "vertex": {"source": 4, "entry": "main"},
          "fragment": {"source": 5, "entry": "main"},
          "blocks": {
            "0": {"name": "sprite_vs_params", "type": "vec4", "count": 5},
            "1": {"name": "sprite_fs_params", "type": "vec4", "count": 1}
          }
        },
        "glsl300es": {
          "vertex": {"source": 6, "entry": "main"},
          "fragment": {"source": 7, "entry": "main"},
          "blocks": {
            "0": {"name": "sprite_vs_params", "type": "vec4", "count": 5},
            "1": {"name": "sprite_fs_params", "type": "vec4", "count": 1}
          }
        },
        "wgsl": {
          "vertex": {"source": 8, "entry": "main"},
          "fragment": {"source": 9, "entry": "main"},
          "blocks": {"0": 0, "1": 1},
          "textures": {"0": 0},
          "samplers": {"0": 64}
        },
        "spirv": {
          "vertex": {"source": 10, "entry": "main"},
          "fragment": {"source": 11, "entry": "main"},
          "blocks": {"0": {"set": 0, "binding": 0}, "1": {"set": 0, "binding": 1}},
          "textures": {"0": {"set": 1, "binding": 0}},
          "samplers": {"0": {"set": 1, "binding": 64}}
        }
      }
    }
  }
}
```

The iOS targets share the sources of the macOS target here, since the three Metal targets translate this program into the same text, and the package lists every distinct text once.

## The package

| Key | Type | Meaning |
| --- | --- | --- |
| `format` | String | Always `haylen-shader`. |
| `version` | Number | The version of the format, 2. |
| `name` | String | The name of the source file without its folder and its extension, such as `sprite` for `shaders/sprite.glsl`. |
| `sources` | Array of strings | The source of every stage for every target, each distinct text once, in the order that the programs and their targets first use them. The SPIR-V modules are base64 text, as [Modules of SPIR-V](#modules-of-spir-v) describes. |
| `programs` | Object | Every program, under its name. |

The keys of `programs` are the names of the variants when the command compiles variants, in the order of the command line, and the names that `@program` gives otherwise, in the order of the source.

## A program

| Key | Type | Meaning |
| --- | --- | --- |
| `attributes` | Array | The inputs of the vertex stage, in the order of their locations. |
| `blocks` | Array | The uniform blocks of both stages, in the order of their slots. |
| `textures` | Array | The textures of both stages, in the order of their slots. |
| `samplers` | Array | The samplers of both stages, in the order of their slots. |
| `pairs` | Array | The texture and sampler pairs that the stages sample with, the vertex stage first, then in the order of their texture slots and sampler slots. |
| `outputs` | Number | The number of color outputs of the fragment stage, which is its highest output location plus one. |
| `targets` | Object | The code of the program for every target it compiles for, under the name of the target, in the order of the target table below. |

Every declared uniform block, texture and sampler appears in the reflection, also when the stages never read it, so the layout of a program stays the same while its code changes. A pair appears only when a stage samples with it.

### Attributes

| Key | Type | Meaning |
| --- | --- | --- |
| `location` | Number | The location of the input, which the vertex layout of a pipeline feeds. |
| `name` | String | The name of the input, which the GLSL targets keep. |
| `type` | String | The GLSL type of the input: `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4`, `uint`, `uvec2`, `uvec3` or `uvec4`. |

### Blocks

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | String | The name of the block. |
| `stage` | String | The stage that declares the block, `vertex` or `fragment`. |
| `slot` | Number | The binding of the block in the source, from 0 to 7. |
| `size` | Number | The size of the block in bytes, a multiple of 16. |
| `members` | Array | The members of the block in the order the source declares them. |

Every member has these keys:

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | String | The name of the member. |
| `type` | String | The GLSL type of the member or of its elements: `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4` or `mat4`. |
| `count` | Number | The number of elements of an array, or 1 for a member that is not an array. |
| `offset` | Number | The offset of the member in bytes from the start of the block, by the std140 rules, where every element of an array starts on 16 bytes. |

A renderer fills the bytes of a block by these offsets and hands all `size` bytes to every target.

### Textures

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | String | The name of the texture. |
| `stage` | String | The stage that declares the texture. |
| `slot` | Number | The binding of the texture in the source, from 0 to 31. |
| `type` | String | `2d`, `2d_array`, `3d` or `cube`. |
| `sampleType` | String | `float`, `unfilterable_float`, `depth`, `sint` or `uint`, as [the language guide](language.md#sample-types) describes. |

### Samplers

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | String | The name of the sampler. |
| `stage` | String | The stage that declares the sampler. |
| `slot` | Number | The binding of the sampler in the source, from 0 to 11. |
| `type` | String | `filtering`, `nonfiltering` or `comparison`. |

### Pairs

| Key | Type | Meaning |
| --- | --- | --- |
| `stage` | String | The stage that samples with the pair. |
| `texture` | Number | The slot of the texture. |
| `sampler` | Number | The slot of the sampler. |
| `name` | String | The name of the combined sampler of the GLSL targets, the name of the texture and the name of the sampler joined by an underscore. |

## Targets

| Target | Language | Keys |
| --- | --- | --- |
| `msl-macos` | Metal Shading Language 2.0 for macOS | `vertex`, `fragment`, `blocks`, `textures`, `samplers` |
| `msl-ios` | Metal Shading Language 2.0 for iOS | `vertex`, `fragment`, `blocks`, `textures`, `samplers` |
| `msl-ios-simulator` | Metal Shading Language 2.0 for the iOS simulator | `vertex`, `fragment`, `blocks`, `textures`, `samplers` |
| `hlsl5` | HLSL for shader model 5.0 | `vertex`, `fragment`, `blocks`, `textures`, `samplers` |
| `glsl430` | GLSL 4.30 | `vertex`, `fragment`, `blocks` |
| `glsl300es` | GLSL ES 3.00 | `vertex`, `fragment`, `blocks` |
| `wgsl` | WGSL | `vertex`, `fragment`, `blocks`, `textures`, `samplers` |
| `spirv` | SPIR-V 1.0 for Vulkan 1.1 | `vertex`, `fragment`, `blocks`, `textures`, `samplers` |

### Stages

The keys `vertex` and `fragment` describe the two stages of the program for the target:

| Key | Type | Meaning |
| --- | --- | --- |
| `source` | Number | The index of the source of the stage in `sources`. |
| `entry` | String | The name of the entry point of the stage in its source, exactly as the translator writes it, such as `main0` for Metal and `main` for the other targets, SPIR-V included. |
| `profile` | String | Only for `hlsl5`: the profile that compiles the stage, `vs_5_0` or `ps_5_0`. |

### Bindings of Metal, HLSL, WGSL and SPIR-V

For the Metal, HLSL, WGSL and SPIR-V targets, `blocks`, `textures` and `samplers` map every slot of the reflection, as a string key in the order of the slots, to the binding of the resource in the source of the target:

| Target | `blocks` | `textures` | `samplers` |
| --- | --- | --- | --- |
| Metal | The index of `[[buffer(n)]]`, counted from 0 in each stage in the order of the slots. | The index of `[[texture(n)]]`, counted from 0 in each stage in the order of the slots. | The index of `[[sampler(n)]]`, counted from 0 in each stage in the order of the slots. |
| `hlsl5` | The number of the register `b`, counted from 0 in each stage in the order of the slots. | The number of the register `t`, which is the slot. | The number of the register `s`, which is the slot. |
| `wgsl` | The binding in `@group(0)`, which is the slot. | The binding in `@group(1)`, which is the slot. | The binding in `@group(1)`, which is 64 plus the slot. |
| `spirv` | `{"set": 0, "binding": slot}` | `{"set": 1, "binding": slot}` | `{"set": 1, "binding": 64 + slot}` |

The Metal targets number textures and samplers compactly, since the oldest Apple GPUs that Metal runs on, the A8 and A10X, bind textures only from 0 to 30 and samplers only from 0 to 15. The Metal, HLSL and WGSL targets give every binding as a number. The SPIR-V target gives every binding as an object with the keys `set` and `binding`, the `DescriptorSet` and `Binding` decorations of the variable in the module, so a Vulkan device builds the layout of descriptor set 0 from the uniform blocks and the layout of set 1 from the textures and the samplers. Every slot belongs to one stage, which the `stage` of the reflection names, so a device makes every binding visible to that stage alone.

The Metal sources read the vertex inputs through `[[stage_in]]`, with `[[attribute(n)]]` at the location of every attribute, and the HLSL sources give every vertex input the semantic `TEXCOORD` with its location as the index, so the attributes of the reflection describe the inputs of both.

### Uniform blocks of GLSL

The GLSL targets hold every uniform block as one array of vectors, so their `blocks` map every slot to the array that holds it:

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | String | The name of the uniform array, which is the name of the block. |
| `type` | String | `vec4` for a block of floats, or `ivec4` for a block of integers. |
| `count` | Number | The number of vectors of the array, the size of the block divided by 16. |

A renderer uploads the bytes of a block to its array as `count` vectors. The GLSL sources bind textures through the combined samplers that `pairs` names, give the vertex inputs the names of `attributes`, and map the depth of clip space to the range of OpenGL.

### Modules of SPIR-V

The sources of the `spirv` target are SPIR-V 1.0 modules, which every Vulkan 1.1 device accepts. Each one is written as text in the standard base64 alphabet of RFC 4648 (`A` to `Z`, `a` to `z`, `0` to `9`, `+` and `/`), with `=` padding to a multiple of four characters and without line breaks. The text holds the 32-bit words of the module, each word written as four bytes from the lowest to the highest, so every module starts with the characters `AwIjBw`, the magic number `0x07230203` in that order. A renderer decodes the text into bytes, reads every four bytes as one little-endian word, and hands the words to `vkCreateShaderModule`.

The modules are the SPIR-V that glslang writes for the stages, with only the descriptor sets and bindings of the table above put in. They keep the names of their variables for the debuggers of the graphics APIs, and they carry no source text and no paths. The vertex inputs keep the locations of `attributes`, the inputs and outputs between the stages keep the locations that the compiler gives them, and each fragment output keeps its location, which is the index of its color attachment. The entry point of both stages is `main`. The vertex stage writes clip space as the source writes it, with y pointing up as in Metal and Direct3D, so a Vulkan device flips it with a viewport of negative height, which Vulkan 1.1 allows, and the depth of clip space goes from 0 to 1 as Vulkan expects.
