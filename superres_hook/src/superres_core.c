/*
 * 超分渲染核心实现
 * 管理FBO、Shader、纹理等资源
 */

#include "superres_hook.h"
#include "shaders/superres_shaders.h"
#include <android/log.h>
#include <cstring>
#include <cstdio>

#define LOG_TAG "SuperRes"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 全局状态
static struct {
    SuperResConfig config;
    int initialized;
    
    // OpenGL资源
    GLuint fbo;
    GLuint texture;
    GLuint vao;
    GLuint vbo;
    GLuint program;
    
    // Uniform位置
    GLint u_texture_loc;
    GLint u_texel_size_loc;
    GLint u_input_size_loc;
    GLint u_output_size_loc;
    GLint u_sharpness_loc;
    GLint u_scale_loc;
    GLint u_mvp_loc;
    
    // 表面信息
    int surface_width;
    int surface_height;
    int target_width;
    int target_height;
    
    // 当前使用的shader类型
    int current_shader_type;
} g_state = {0};

// 编译Shader
static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("Shader compile error: %s", log);
        return 0;
    }
    return shader;
}

// 创建Program
static GLuint create_program(const char* vs, const char* fs) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fs);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        LOGE("Program link error: %s", log);
        return 0;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

// 初始化超分资源
int superres_init(SuperResConfig *config) {
    memset(&g_state, 0, sizeof(g_state));
    
    if (config) {
        g_state.config = *config;
    } else {
        g_state.config.scale_factor = 2;
        g_state.config.enable_superres = 1;
        g_state.config.shader_type = 1; // FSR默认
        g_state.config.sharpness = 0.5f;
    }
    
    g_state.initialized = 0;
    g_state.fbo = 0;
    g_state.texture = 0;
    g_state.vao = 0;
    g_state.vbo = 0;
    g_state.program = 0;
    
    LOGI("SuperRes initialized with scale=%d, shader=%d", 
         g_state.config.scale_factor, g_state.config.shader_type);
    
    return 1;
}

// 清理资源
void superres_cleanup(void) {
    if (g_state.program) {
        glDeleteProgram(g_state.program);
        g_state.program = 0;
    }
    if (g_state.fbo) {
        glDeleteFramebuffers(1, &g_state.fbo);
        g_state.fbo = 0;
    }
    if (g_state.texture) {
        glDeleteTextures(1, &g_state.texture);
        g_state.texture = 0;
    }
    if (g_state.vao) {
        glDeleteVertexArrays(1, &g_state.vao);
        g_state.vao = 0;
    }
    if (g_state.vbo) {
        glDeleteBuffers(1, &g_state.vbo);
        g_state.vbo = 0;
    }
    
    g_state.initialized = 0;
    LOGI("SuperRes cleanup completed");
}

// 确保资源已创建
static int ensure_resources(int width, int height) {
    if (g_state.initialized && 
        g_state.surface_width == width && 
        g_state.surface_height == height) {
        return 1;
    }
    
    // 清理旧资源
    superres_cleanup();
    
    g_state.surface_width = width;
    g_state.surface_height = height;
    g_state.target_width = width * g_state.config.scale_factor;
    g_state.target_height = height * g_state.config.scale_factor;
    
    LOGI("Creating resources: %dx%d -> %dx%d", 
         width, height, g_state.target_width, g_state.target_height);
    
    // 创建全屏Quad顶点数据
    float vertices[] = {
        // position       // texcoord
        -1.0f, -1.0f,     0.0f, 1.0f,
         1.0f, -1.0f,     1.0f, 1.0f,
         1.0f,  1.0f,     1.0f, 0.0f,
        -1.0f, -1.0f,     0.0f, 1.0f,
         1.0f,  1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,     0.0f, 0.0f,
    };
    
    glGenVertexArrays(1, &g_state.vao);
    glGenBuffers(1, &g_state.vbo);
    
    glBindVertexArray(g_state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_state.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    // 创建目标纹理
    glGenTextures(1, &g_state.texture);
    glBindTexture(GL_TEXTURE_2D, g_state.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_state.target_width, g_state.target_height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // 创建FBO
    glGenFramebuffers(1, &g_state.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_state.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D, g_state.texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer incomplete!");
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // 根据配置选择Shader
    const char* vs = FULLSCREEN_VERTEX_SHADER;
    const char* fs = NULL;
    
    switch (g_state.config.shader_type) {
        case 0: // Bicubic
            fs = BICUBIC_FRAGMENT_SHADER;
            break;
        case 1: // FSR
            fs = FSR_FRAGMENT_SHADER;
            break;
        case 2: // NIS
            fs = NIS_FRAGMENT_SHADER;
            break;
        default:
            fs = BICUBIC_FRAGMENT_SHADER;
    }
    
    g_state.program = create_program(vs, fs);
    if (!g_state.program) {
        LOGE("Failed to create program");
        return 0;
    }
    
    // 获取Uniform位置
    g_state.u_texture_loc = glGetUniformLocation(g_state.program, "u_texture");
    g_state.u_texel_size_loc = glGetUniformLocation(g_state.program, "u_texel_size");
    g_state.u_input_size_loc = glGetUniformLocation(g_state.program, "u_input_size");
    g_state.u_output_size_loc = glGetUniformLocation(g_state.program, "u_output_size");
    g_state.u_sharpness_loc = glGetUniformLocation(g_state.program, "u_sharpness");
    g_state.u_scale_loc = glGetUniformLocation(g_state.program, "u_scale");
    g_state.u_mvp_loc = glGetUniformLocation(g_state.program, "u_mvp");
    
    g_state.current_shader_type = g_state.config.shader_type;
    g_state.initialized = 1;
    
    LOGI("Resources created successfully");
    return 1;
}

// 获取Surface尺寸
void superres_get_surface_size(EGLSurface surface, int *width, int *height) {
    EGLint w, h;
    eglQuerySurface(eglGetCurrentDisplay(), surface, EGL_WIDTH, &w);
    eglQuerySurface(eglGetCurrentDisplay(), surface, EGL_HEIGHT, &h);
    if (width) *width = w;
    if (height) *height = h;
}

// 执行超分渲染
int superres_render(EGLDisplay dpy, EGLSurface surface) {
    if (!g_state.config.enable_superres) {
        return 1;
    }
    
    // 获取原始分辨率
    EGLint orig_width, orig_height;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &orig_width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &orig_height);
    
    if (orig_width <= 0 || orig_height <= 0) {
        return 0;
    }
    
    // 确保资源存在
    if (!ensure_resources(orig_width, orig_height)) {
        LOGE("Failed to ensure resources");
        return 0;
    }
    
    // 保存当前OpenGL状态
    GLint current_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
    GLint current_viewport[4];
    glGetIntegerv(GL_VIEWPORT, current_viewport);
    
    // 从surface读取像素（这里需要更高效的zero-copy方案）
    // 实际应用中应该使用EGL Image或GraphicBuffer直接访问
    GLuint input_texture;
    glGenTextures(1, &input_texture);
    glBindTexture(GL_TEXTURE_2D, input_texture);
    
    // 简化：假设已经有一个包含渲染结果的纹理
    // 实际应该从EGL Surface的backbuffer获取
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, orig_width, orig_height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
    // 绑定到FBO进行读取
    GLuint read_fbo;
    glGenFramebuffers(1, &read_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, read_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D, input_texture, 0);
    
    // 切换到超分FBO
    glBindFramebuffer(GL_FRAMEBUFFER, g_state.fbo);
    glViewport(0, 0, g_state.target_width, g_state.target_height);
    
    // 使用超分Shader
    glUseProgram(g_state.program);
    
    // 设置Uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_texture);
    glUniform1i(g_state.u_texture_loc, 0);
    
    float texel_x = 1.0f / orig_width;
    float texel_y = 1.0f / orig_height;
    glUniform2f(g_state.u_texel_size_loc, texel_x, texel_y);
    glUniform2f(g_state.u_input_size_loc, (float)orig_width, (float)orig_height);
    glUniform2f(g_state.u_output_size_loc, (float)g_state.target_width, (float)g_state.target_height);
    glUniform1f(g_state.u_sharpness_loc, g_state.config.sharpness);
    glUniform1f(g_state.u_scale_loc, (float)g_state.config.scale_factor);
    
    // 绘制全屏Quad
    glBindVertexArray(g_state.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    // 恢复状态
    glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
    glViewport(current_viewport[0], current_viewport[1], 
               current_viewport[2], current_viewport[3]);
    
    // 清理临时资源
    glDeleteFramebuffers(1, &read_fbo);
    glDeleteTextures(1, &input_texture);
    
    return 1;
}

// 设置配置
void superres_set_config(SuperResConfig *config) {
    if (config) {
        g_state.config = *config;
        g_state.initialized = 0; // 强制重新创建资源
    }
}

// 获取配置
SuperResConfig* superres_get_config(void) {
    return &g_state.config;
}
