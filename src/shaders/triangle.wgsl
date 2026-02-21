struct Uniforms {
	resolution: vec2f,
	translation: vec2f,
	color: vec4f,
	extras: vec4f,
};

@group(0) @binding(0) var<uniform> u: Uniforms;

@vertex
fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
	var position = in_vertex_position + u.translation;
	position.x = position.x - u.extras.y;  // Add camera offset X
	position.y = position.y - u.extras.z;  // Add camera offset Y
	let center = u.resolution * 0.5;
	let zoomed = (position - center) * u.extras.x + center;
	let zeroToOne = zoomed / u.resolution;
	let clipSpace = zeroToOne * 2.0 - 1.0;
	return vec4f(clipSpace, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return u.color;
}