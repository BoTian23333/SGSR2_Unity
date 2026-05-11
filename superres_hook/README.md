# Android OpenGL 超分Hook实现指南

## 项目结构

```
superres_hook/
├── include/
│   └── superres_hook.h      # 公共头文件
├── src/
│   ├── superres_core.c      # 超分核心渲染逻辑
│   └── superres_hook.c      # EGL/GLES Hook实现
├── shaders/
│   └── superres_shaders.h   # GLSL Shader代码
├── build.sh                  # 编译脚本
└── README.md                 # 本文档
```

## 技术方案

### 方案一：LD_PRELOAD Hook（推荐）

**原理：** 利用Linux的LD_PRELOAD机制，在目标应用加载前注入我们的共享库，拦截EGL函数调用。

**使用步骤：**

1. **编译库文件**
```bash
export ANDROID_NDK=/path/to/android-ndk
cd superres_hook
chmod +x build.sh
ARCH=arm64 ./build.sh
```

2. **推送到Android设备**
```bash
adb push build/arm64-v8a/libsuperres.so /data/local/tmp/
adb shell chmod 755 /data/local/tmp/libsuperres.so
```

3. **运行目标应用（带Hook）**
```bash
# 方法A: 使用wrap.sh（Android 9+）
adb shell
cd /data/local/tmp
echo 'LD_PRELOAD=/data/local/tmp/libsuperres.so exec "$@"' > wrap.sh
chmod 755 wrap.sh

# 设置wrap属性
adb shell setprop wrap.com.target.app "sh /data/local/tmp/wrap.sh"

# 启动应用
adb shell am start -n com.target.app/.MainActivity

# 清除wrap（测试完成后）
adb shell setprop wrap.com.target.app ""
```

```bash
# 方法B: 直接修改启动命令（需要root）
adb shell
export LD_PRELOAD=/data/local/tmp/libsuperres.so
am start -n com.target.app/.MainActivity
```

### 环境变量配置

通过环境变量控制超分行为：

```bash
# 缩放倍数 (2 = 2倍超分)
export SUPERRES_SCALE=2

# Shader类型: 0=Bicubic, 1=FSR, 2=NIS
export SUPERRES_SHADER=1

# 启用/禁用: 0=禁用, 1=启用
export SUPERRES_ENABLE=1

# 组合使用
adb shell "SUPERRES_SCALE=2 SUPERRES_SHADER=1 SUPERRES_ENABLE=1 LD_PRELOAD=/data/local/tmp/libsuperres.so am start -n com.game.app/.MainActivity"
```

## 核心实现详解

### 1. Hook机制

```c
// 拦截eglSwapBuffers
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // 1. 初始化超分资源
    if (!g_initialized) {
        superres_init(&config);
        g_initialized = 1;
    }
    
    // 2. 调用原始函数
    EGLBoolean result = g_orig_eglSwapBuffers(dpy, surface);
    
    // 3. 执行超分渲染（将结果写入更高分辨率的buffer）
    superres_render(dpy, surface);
    
    return result;
}
```

### 2. 超分渲染流程

```
应用渲染 (低分辨率) 
    ↓
eglSwapBuffers被拦截
    ↓
获取Backbuffer内容 → 创建输入纹理
    ↓
执行超分Shader (FSR/NIS/Bicubic)
    ↓
渲染到高分辨率FBO
    ↓
输出到显示Surface
```

### 3. Shader算法

#### Bicubic插值
- 16 tap采样
- 平滑插值，适合一般场景
- 性能中等

#### FSR (FidelityFX Super Resolution)
- EASU: 边缘自适应空间上采样
- RCAS: 鲁棒对比度自适应锐化
- AMD开源，效果好，性能优秀

#### NIS (NVIDIA Image Scaling)
- 5-tap自适应滤波器
- 边缘检测+锐化
- NVIDIA开源，轻量级

## 高级优化

### Zero-Copy优化

当前实现使用了临时纹理拷贝，高性能场景应使用EGL Image扩展：

```c
// 使用EGL Image实现零拷贝
EGLImageKHR CreateEGLImageFromGraphicBuffer(ANativeWindowBuffer* buffer) {
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    return eglCreateImageKHR(
        display,
        EGL_NO_CONTEXT,
        EGL_NATIVE_BUFFER_ANDROID,
        buffer,
        attribs
    );
}
```

### SurfaceFlinger集成（系统级）

如需对所有应用生效，需修改AOSP源码：

```cpp
// frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
void SurfaceFlinger::compose() {
    // 在Composition阶段插入超分
    for (auto& layer : layers) {
        if (layer->needsSuperResolution()) {
            applySuperResolution(layer);
        }
    }
}
```

## 调试方法

### Logcat日志

```bash
adb logcat | grep -E "SuperRes|SuperResHook"
```

### 性能分析

```bash
# GPU渲染时间
adb shell dumpsys SurfaceFlinger --latency-clear
adb shell dumpsys SurfaceFlinger --latency <surface-name>

# FPS监控
adb shell dumpsys gfxinfo <package-name>
```

## 注意事项

1. **触摸坐标映射**
   - 超分后分辨率变化，需要重新映射触摸事件
   - 可在InputFlinger层做坐标转换

2. **性能开销**
   - FSR 2x超分约增加2-5ms GPU时间
   - 建议在性能模式下使用

3. **兼容性**
   - 需要OpenGL ES 3.0+
   - Android 7.0+ (API 24+)

4. **安全限制**
   - 非root设备可能需要特殊权限
   - 部分游戏有反作弊检测

## 扩展功能

### 动态配置接口

可通过Socket或File实现运行时配置：

```c
// 监听配置变化
void watch_config_changes() {
    int fd = open("/data/local/tmp/superres_config", O_RDONLY);
    // 读取JSON配置并更新
}
```

### 多档位支持

```c
typedef enum {
    SUPERRES_OFF = 0,      // 关闭
    SUPERRES_QUALITY = 1,  // 质量优先 (FSR)
    SUPERRES_PERFORMANCE = 2, // 性能优先 (NIS)
    SUPERRES_BALANCED = 3  // 平衡模式
} SuperResMode;
```

## 参考资料

- [AMD FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR)
- [NVIDIA NIS](https://github.com/NVIDIAGameWorks/NVIDIAImageScaling)
- [Android EGL Spec](https://www.khronos.org/registry/EGL/)
- [LD_PRELOAD技巧](https://shyft.me/posts/2023/05/android_ld_preload/)
