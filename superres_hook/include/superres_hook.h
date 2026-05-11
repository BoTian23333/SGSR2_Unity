/*
 * OpenGL ES Hook Header
 * 拦截EGL和GLES关键函数
 */

#ifndef SUPERRES_HOOK_H
#define SUPERRES_HOOK_H

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

// 原始函数指针类型定义
typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (*eglSwapBuffersWithDamage_t)(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
typedef void (*glDrawArrays_t)(GLenum mode, GLint first, GLsizei count);
typedef void (*glDrawElements_t)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (*glViewport_t)(GLint x, GLint y, GLsizei width, GLsizei height);

// 超分配置结构
typedef struct {
    int scale_factor;        // 缩放倍数 (2 = 2x超分)
    int enable_superres;     // 是否启用超分
    int shader_type;         // 0: Bicubic, 1: FSR, 2: NIS
    float sharpness;         // 锐化强度 (0.0-1.0)
} SuperResConfig;

// 初始化超分模块
int superres_init(SuperResConfig *config);

// 清理超分资源
void superres_cleanup(void);

// 执行超分渲染
int superres_render(EGLDisplay dpy, EGLSurface surface);

// 获取当前surface的分辨率
void superres_get_surface_size(EGLSurface surface, int *width, int *height);

// 设置超分配置
void superres_set_config(SuperResConfig *config);

// 获取超分配置
SuperResConfig* superres_get_config(void);

#ifdef __cplusplus
}
#endif

#endif // SUPERRES_HOOK_H
