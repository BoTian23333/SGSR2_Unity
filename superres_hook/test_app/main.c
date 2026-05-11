/*
 * 测试应用 - 演示如何使用超分Hook
 * 这是一个简单的OpenGL ES渲染示例
 */

#include <android/native_activity.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "SuperResTest"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// EGL上下文
static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;

// OpenGL资源
static GLuint g_program = 0;
static GLuint g_vao = 0;
static GLuint g_vbo = 0;

// 简单的顶点Shader
static const char* VERTEX_SHADER = 
    "#version 300 es\n"
    "layout(location = 0) in vec4 a_position;\n"
    "layout(location = 1) in vec3 a_color;\n"
    "out vec3 v_color;\n"
    "void main() {\n"
    "    gl_Position = a_position;\n"
    "    v_color = a_color;\n"
    "}\n";

// 简单的片段Shader
static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec3 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = vec4(v_color, 1.0);\n"
    "}\n";

// 初始化EGL
static int init_egl(ANativeWindow* window) {
    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_display == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return -1;
    }

    if (!eglInitialize(g_display, NULL, NULL)) {
        LOGE("Failed to initialize EGL");
        return -1;
    }

    EGLConfig config;
    EGLint num_configs;
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    if (!eglChooseConfig(g_display, attribs, &config, 1, &num_configs)) {
        LOGE("Failed to choose EGL config");
        return -1;
    }

    g_surface = eglCreateWindowSurface(g_display, config, window, NULL);
    if (g_surface == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL surface");
        return -1;
    }

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, context_attribs);
    if (g_context == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return -1;
    }

    if (eglMakeCurrent(g_display, g_surface, g_surface, g_context) != EGL_TRUE) {
        LOGE("Failed to make EGL context current");
        return -1;
    }

    LOGI("EGL initialized successfully");
    return 0;
}

// 编译Shader
static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("Shader compile error: %s", log);
        return 0;
    }
    return shader;
}

// 初始化OpenGL
static int init_gl(void) {
    // 创建Program
    GLuint vs = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);

    GLint success;
    glGetProgramiv(g_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(g_program, sizeof(log), NULL, log);
        LOGE("Program link error: %s", log);
        return -1;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // 创建三角形顶点数据
    float vertices[] = {
        // position          // color
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // red
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // green
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // blue
    };

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // 启用超分（如果Hook已加载）
    LOGI("OpenGL initialized, rendering triangle at low resolution");
    LOGI("If SuperRes Hook is loaded, it will upscale the output");

    return 0;
}

// 渲染帧
static void render_frame(void) {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_program);
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    eglSwapBuffers(g_display, g_surface);
}

// 清理资源
static void cleanup(void) {
    if (g_vao) glDeleteVertexArrays(1, &g_vao);
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    if (g_program) glDeleteProgram(g_program);

    if (g_context) eglDestroyContext(g_display, g_context);
    if (g_surface) eglDestroySurface(g_display, g_surface);
    if (g_display) eglTerminate(g_display);
}

// Android Activity回调
static void handle_cmd(ANativeActivity* activity, int32_t cmd) {
    (void)activity;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (activity->window) {
                init_egl(activity->window);
                init_gl();
            }
            break;
        case APP_CMD_TERM_WINDOW:
            cleanup();
            break;
    }
}

static void handle_input(ANativeActivity* activity, AInputEvent* event) {
    (void)activity;
    (void)event;
}

// 入口点
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    activity->callbacks->onConfigurationChanged = NULL;
    activity->callbacks->onLowMemory = NULL;
    activity->callbacks->onWindowFocusChanged = NULL;
    activity->callbacks->onPause = NULL;
    activity->callbacks->onResume = NULL;
    activity->callbacks->onSaveInstanceState = NULL;
    activity->callbacks->onDestroy = NULL;
    activity->callbacks->onStart = NULL;
    activity->callbacks->onStop = NULL;

    activity->callbacks->onNativeActivityCreated = NULL;

    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

    LOGI("SuperRes Test App started");
    LOGI("To enable super resolution, launch with:");
    LOGI("LD_PRELOAD=/path/to/libsuperres.so am start -n com.example.superrestest/.MainActivity");
}
