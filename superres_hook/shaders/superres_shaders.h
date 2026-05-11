/*
 * 超分Shader代码
 * 包含Bicubic、FSR、NIS三种算法
 */

// ==================== Bicubic插值 ====================
const char* BICUBIC_VERTEX_SHADER = R"(
#version 300 es
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;
uniform mat4 u_mvp;
void main() {
    gl_Position = u_mvp;
    v_texcoord = a_texcoord;
}
)";

const char* BICUBIC_FRAGMENT_SHADER = R"(
#version 300 es
precision highp float;
in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D u_texture;
uniform vec2 u_texel_size;
uniform float u_sharpness;

vec4 cubicKernel(float x) {
    float ax = abs(x);
    if (ax < 1.0) {
        return vec4(
            1.5 * ax * ax * ax - 2.5 * ax * ax + 1.0,
            0.0, 0.0, 0.0
        );
    } else if (ax < 2.0) {
        return vec4(
            -0.5 * ax * ax * ax + 2.5 * ax * ax - 4.0 * ax + 2.0,
            0.0, 0.0, 0.0
        );
    }
    return vec4(0.0);
}

vec4 bicubicSample(sampler2D tex, vec2 uv, vec2 texel_size) {
    vec2 f = fract(uv * textureSize(tex, 0));
    vec2 p = uv - f * texel_size;
    
    vec4 sum = vec4(0.0);
    float total_weight = 0.0;
    
    for (int i = -1; i <= 2; i++) {
        for (int j = -1; j <= 2; j++) {
            vec2 offset = vec2(float(i), float(j)) * texel_size;
            vec4 weight = cubicKernel((vec2(float(i), float(j)) - f));
            float w = weight.r * weight.r; // 简化处理
            sum += texture(tex, p + offset) * w;
            total_weight += w;
        }
    }
    
    return sum / total_weight;
}

void main() {
    fragColor = bicubicSample(u_texture, v_texcoord, u_texel_size);
}
)";

// ==================== FSR (FidelityFX Super Resolution) ====================
const char* FSR_VERTEX_SHADER = R"(
#version 300 es
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;
uniform mat4 u_mvp;
void main() {
    gl_Position = u_mvp;
    v_texcoord = a_texcoord;
}
)";

const char* FSR_FRAGMENT_SHADER = R"(
#version 300 es
precision highp float;
in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D u_texture;
uniform vec2 u_input_size;
uniform vec2 u_output_size;
uniform float u_sharpness;

// FSR EASU (Edge Adaptive Spatial Upsampling) 简化版
vec4 fsrEasu(sampler2D tex, vec2 uv, vec2 input_size, vec2 output_size) {
    vec2 input_texel_size = 1.0 / input_size;
    
    // 计算采样位置
    vec2 pos = uv * input_size - 0.5;
    vec2 f = fract(pos);
    vec2 ipos = floor(pos);
    
    // 获取周围16个像素
    vec4 a = texture(tex, (ipos + vec2(-1.0, -1.0) + f) * input_texel_size);
    vec4 b = texture(tex, (ipos + vec2( 0.0, -1.0) + f) * input_texel_size);
    vec4 c = texture(tex, (ipos + vec2( 1.0, -1.0) + f) * input_texel_size);
    vec4 d = texture(tex, (ipos + vec2( 2.0, -1.0) + f) * input_texel_size);
    
    vec4 e = texture(tex, (ipos + vec2(-1.0,  0.0) + f) * input_texel_size);
    vec4 f_sample = texture(tex, (ipos + vec2( 0.0,  0.0) + f) * input_texel_size);
    vec4 g = texture(tex, (ipos + vec2( 1.0,  0.0) + f) * input_texel_size);
    vec4 h = texture(tex, (ipos + vec2( 2.0,  0.0) + f) * input_texel_size);
    
    vec4 i = texture(tex, (ipos + vec2(-1.0,  1.0) + f) * input_texel_size);
    vec4 j = texture(tex, (ipos + vec2( 0.0,  1.0) + f) * input_texel_size);
    vec4 k = texture(tex, (ipos + vec2( 1.0,  1.0) + f) * input_texel_size);
    vec4 l = texture(tex, (ipos + vec2( 2.0,  1.0) + f) * input_texel_size);
    
    vec4 m = texture(tex, (ipos + vec2(-1.0,  2.0) + f) * input_texel_size);
    vec4 n = texture(tex, (ipos + vec2( 0.0,  2.0) + f) * input_texel_size);
    vec4 o = texture(tex, (ipos + vec2( 1.0,  2.0) + f) * input_texel_size);
    vec4 p = texture(tex, (ipos + vec2( 2.0,  2.0) + f) * input_texel_size);
    
    // 简化的边缘自适应权重计算
    float min1 = min(min(b.r, e.r), min(f_sample.r, g.r));
    float max1 = max(max(b.r, e.r), max(f_sample.r, g.r));
    
    // 双线性插值作为基础
    vec4 bilinear = mix(
        mix(b, g, f.x),
        mix(e, f_sample, f.x),
        f.y
    );
    
    return bilinear;
}

// FSR RCAS (Robust Contrast Adaptive Sharpening)
vec4 fsrRcas(sampler2D tex, vec2 uv, vec2 texel_size, float sharpness) {
    vec4 center = texture(tex, uv);
    vec4 north = texture(tex, uv + vec2(0.0, -texel_size.y));
    vec4 south = texture(tex, uv + vec2(0.0, texel_size.y));
    vec4 east = texture(tex, uv + vec2(texel_size.x, 0.0));
    vec4 west = texture(tex, uv + vec2(-texel_size.x, 0.0));
    
    float min_rgb = min(min(center.r, center.g), center.b);
    float max_rgb = max(max(center.r, center.g), center.b);
    
    float blend = clamp(sharpness, 0.0, 1.0);
    
    vec4 filtered = (north + south + east + west) * 0.25;
    vec4 sharpened = center + (center - filtered) * blend;
    
    return sharpened;
}

void main() {
    vec2 input_texel_size = 1.0 / u_input_size;
    vec2 output_texel_size = 1.0 / u_output_size;
    
    // EASU上采样
    vec4 color = fsrEasu(u_texture, v_texcoord, u_input_size, u_output_size);
    
    // RCAS锐化
    color = fsrRcas(u_texture, v_texcoord, output_texel_size, u_sharpness);
    
    fragColor = color;
}
)";

// ==================== NIS (NVIDIA Image Scaling) ====================
const char* NIS_VERTEX_SHADER = R"(
#version 300 es
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;
uniform mat4 u_mvp;
void main() {
    gl_Position = u_mvp;
    v_texcoord = a_texcoord;
}
)";

const char* NIS_FRAGMENT_SHADER = R"(
#version 300 es
precision highp float;
in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D u_texture;
uniform vec2 u_input_size;
uniform vec2 u_output_size;
uniform float u_sharpness;
uniform float u_scale;

// NIS简化实现
vec4 nisScale(sampler2D tex, vec2 uv, vec2 input_size, vec2 output_size, float scale, float sharpness) {
    vec2 input_texel_size = 1.0 / input_size;
    
    // 映射到输入空间
    vec2 pos = uv * input_size;
    vec2 ipos = floor(pos);
    vec2 f = fract(pos);
    
    // 5-tap滤波器
    vec4 c = texture(tex, (ipos + f) * input_texel_size);
    vec4 l = texture(tex, (ipos + vec2(-1.0, 0.0) + f) * input_texel_size);
    vec4 r = texture(tex, (ipos + vec2(1.0, 0.0) + f) * input_texel_size);
    vec4 t = texture(tex, (ipos + vec2(0.0, -1.0) + f) * input_texel_size);
    vec4 b = texture(tex, (ipos + vec2(0.0, 1.0) + f) * input_texel_size);
    
    // 自适应锐化
    float edge_strength = 0.0;
    edge_strength += abs(c.r - l.r) + abs(c.r - r.r) + abs(c.r - t.r) + abs(c.r - b.r);
    edge_strength += abs(c.g - l.g) + abs(c.g - r.g) + abs(c.g - t.g) + abs(c.g - b.g);
    edge_strength += abs(c.b - l.b) + abs(c.b - r.b) + abs(c.b - t.b) + abs(c.b - b.b);
    
    float sharpness_adj = sharpness * (1.0 - clamp(edge_strength * 0.1, 0.0, 0.8));
    
    vec4 avg = (l + r + t + b) * 0.25;
    vec4 result = c + (c - avg) * sharpness_adj;
    
    return result;
}

void main() {
    fragColor = nisScale(u_texture, v_texcoord, u_input_size, u_output_size, u_scale, u_sharpness);
}
)";

// ==================== Fullscreen Quad Vertex Shader ====================
const char* FULLSCREEN_VERTEX_SHADER = R"(
#version 300 es
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;
void main() {
    gl_Position = a_position;
    v_texcoord = a_texcoord;
}
)";
