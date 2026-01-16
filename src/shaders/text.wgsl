struct VertexInput {
    @location(0) vertex: vec4f, // <vec2 pos, vec2 tex>
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoords: vec2f,
}

struct Uniforms {
    projection: mat4x4f,
    textColor: vec3f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var textSampler: sampler;
@group(0) @binding(2) var textTexture: texture_2d<f32>;

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = uniforms.projection * vec4f(input.vertex.xy, 0.0, 1.0);
    output.texCoords = input.vertex.zw;
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4f {
    let sampled = vec4f(1.0, 1.0, 1.0, textureSample(textTexture, textSampler, input.texCoords).r);
    let color = vec4f(uniforms.textColor, 1.0) * sampled;
    return color;
}