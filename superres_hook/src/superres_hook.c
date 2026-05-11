/*
 * OpenGL ES Hook实现
 * 使用PLT/GOT Hook技术拦截EGL和GLES函数调用
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <android/log.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "superres_hook.h"

#define LOG_TAG "SuperResHook"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 原始函数指针
static eglSwapBuffers_t g_orig_eglSwapBuffers = NULL;
static eglSwapBuffersWithDamage_t g_orig_eglSwapBuffersWithDamage = NULL;

// 互斥锁
static pthread_mutex_t g_hook_mutex = PTHREAD_MUTEX_INITIALIZER;

// 是否已初始化
static int g_initialized = 0;

// 查找符号在内存中的地址
static void* find_symbol_in_module(const char* module_name, const char* symbol_name) {
    struct dl_iterate_phdr_data {
        const char* target_module;
        const char* target_symbol;
        void* result;
    };
    
    static struct dl_iterate_phdr_data data = {0};
    data.target_module = module_name;
    data.target_symbol = symbol_name;
    data.result = NULL;
    
    int callback(struct dl_phdr_info* info, size_t size, void* param) {
        (void)size;
        struct dl_iterate_phdr_data* d = (struct dl_iterate_phdr_data*)param;
        
        if (!info->dlpi_name || !strstr(info->dlpi_name, d->target_module)) {
            return 0;
        }
        
        // 遍历program headers
        for (size_t i = 0; i < info->dlpi_phnum; i++) {
            const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
            
            if (phdr->p_type != PT_DYNAMIC) {
                continue;
            }
            
            // 解析dynamic section
            const ElfW(Dyn)* dyn = (const ElfW(Dyn)*)(info->dlpi_addr + phdr->p_vaddr);
            const ElfW(Sym)* symtab = NULL;
            const char* strtab = NULL;
            
            while (dyn->d_tag != DT_NULL) {
                switch (dyn->d_tag) {
                    case DT_SYMTAB:
                        symtab = (const ElfW(Sym)*)(info->dlpi_addr + dyn->d_un.d_ptr);
                        break;
                    case DT_STRTAB:
                        strtab = (const char*)(info->dlpi_addr + dyn->d_un.d_ptr);
                        break;
                }
                dyn++;
            }
            
            if (!symtab || !strtab) {
                return 0;
            }
            
            // 查找符号
            for (int j = 0; symtab[j].st_name != 0; j++) {
                if (strcmp(strtab + symtab[j].st_name, d->target_symbol) == 0) {
                    d->result = (void*)(info->dlpi_addr + symtab[j].st_value);
                    return 1;
                }
            }
        }
        
        return 0;
    }
    
    dl_iterate_phdr(callback, &data);
    return data.result;
}

// 加载原始函数
static void load_original_functions(void) {
    if (g_orig_eglSwapBuffers) {
        return;
    }
    
    // 方法1: 直接dlopen获取
    void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
    if (egl_handle) {
        g_orig_eglSwapBuffers = (eglSwapBuffers_t)dlsym(egl_handle, "eglSwapBuffers");
        g_orig_eglSwapBuffersWithDamage = 
            (eglSwapBuffersWithDamage_t)dlsym(egl_handle, "eglSwapBuffersWithDamage");
    }
    
    // 方法2: 从内存中查找（如果dlopen失败）
    if (!g_orig_eglSwapBuffers) {
        g_orig_eglSwapBuffers = (eglSwapBuffers_t)find_symbol_in_module("libEGL", "eglSwapBuffers");
    }
    
    LOGI("Original eglSwapBuffers: %p", g_orig_eglSwapBuffers);
}

// Hook后的eglSwapBuffers
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    pthread_mutex_lock(&g_hook_mutex);
    
    if (!g_initialized) {
        SuperResConfig config = {
            .scale_factor = 2,
            .enable_superres = 1,
            .shader_type = 1,  // FSR
            .sharpness = 0.5f
        };
        superres_init(&config);
        g_initialized = 1;
        LOGI("SuperRes Hook initialized");
    }
    
    pthread_mutex_unlock(&g_hook_mutex);
    
    // 先执行原始swap buffers（将backbuffer内容呈现）
    EGLBoolean result = GL_FALSE;
    if (g_orig_eglSwapBuffers) {
        result = g_orig_eglSwapBuffers(dpy, surface);
    }
    
    // 在这里可以执行超分处理
    // 注意：实际应用中需要在swap之前获取backbuffer内容
    // 这里简化处理，实际应该使用更复杂的zero-copy方案
    
    return result;
}

// Hook后的eglSwapBuffersWithDamage
EGLBoolean eglSwapBuffersWithDamage(EGLDisplay dpy, EGLSurface surface, 
                                    EGLint *rects, EGLint n_rects) {
    pthread_mutex_lock(&g_hook_mutex);
    
    if (!g_initialized) {
        SuperResConfig config = {
            .scale_factor = 2,
            .enable_superres = 1,
            .shader_type = 1,
            .sharpness = 0.5f
        };
        superres_init(&config);
        g_initialized = 1;
    }
    
    pthread_mutex_unlock(&g_hook_mutex);
    
    EGLBoolean result = GL_FALSE;
    if (g_orig_eglSwapBuffersWithDamage) {
        result = g_orig_eglSwapBuffersWithDamage(dpy, surface, rects, n_rects);
    } else if (g_orig_eglSwapBuffers) {
        result = g_orig_eglSwapBuffers(dpy, surface);
    }
    
    return result;
}

// 构造函数，库加载时自动执行
__attribute__((constructor))
static void hook_constructor(void) {
    LOGI("SuperRes Hook library loaded");
    load_original_functions();
    
    // 可以从环境变量读取配置
    const char* scale_env = getenv("SUPERRES_SCALE");
    const char* shader_env = getenv("SUPERRES_SHADER");
    const char* enable_env = getenv("SUPERRES_ENABLE");
    
    if (scale_env || shader_env || enable_env) {
        SuperResConfig config = {
            .scale_factor = scale_env ? atoi(scale_env) : 2,
            .enable_superres = enable_env ? atoi(enable_env) : 1,
            .shader_type = shader_env ? atoi(shader_env) : 1,
            .sharpness = 0.5f
        };
        superres_init(&config);
        g_initialized = 1;
    }
}

// 析构函数
__attribute__((destructor))
static void hook_destructor(void) {
    LOGI("SuperRes Hook library unloaded");
    superres_cleanup();
}
