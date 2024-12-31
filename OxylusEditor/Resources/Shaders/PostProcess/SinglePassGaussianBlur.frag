#version 450
#pragma shader_stage(fragment)

layout(binding = 0) uniform sampler2D color_sampler;

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;

const float Pi = 6.28318530718; // Pi*2

void main() {
    
    float directions = 16.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
    float quality = 3.0; // BLUR QUALITY (Default 4.0 - More is better but slower)
    float size = 8.0; // BLUR SIZE (Radius)
    
    vec2 resolution = textureSize(color_sampler, 0);
    
    vec2 radius = size / resolution;
    
    vec2 uv = tex_coord;
    vec4 color = texture(color_sampler, uv);
  
    for(float d = 0.0; d < Pi; d += Pi / directions) {
        for(float i = 1.0 / quality; i <= 1.0; i += 1.0 / quality) {
	        color += texture(color_sampler, uv + vec2(cos(d), sin(d)) * radius * i);		
        }
    }
  
    color /= quality * directions - 15.0;
    frag_color = color;
}