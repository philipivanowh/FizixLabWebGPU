struct Uniforms {
	resolution: vec2f,
	translation: vec2f,
	color: vec4f,
	extras: vec4f,
};

@group(0) @binding(0) var<uniform> u: Uniforms;

@vertex
fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
	let position = in_vertex_position + u.translation;
	let center = u.resolution * 0.5;
	let zoomed = (position - center) * u.extras.x + center;
	// convert the position from pixels to 0.0 to 1.0
	let zeroToOne = zoomed / u.resolution;
	// convert to 0-2 then -1-+1
	let clipSpace = zeroToOne * 2.0 - 1.0;
	return vec4f(clipSpace, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return u.color;
}
