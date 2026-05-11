# SGSR2_Unity
 the Snapdragon™ Game Super Resolution 2 for unity

## Introduction
 This project is a Unity plugin for Snapdragon™ Game Super Resolution 2. \
 The source code is based on the Snapdragon™ Game Super Resolution 2 SDK.[Snapdragon™ Game Super Resolution 2 SDK](https://github.com/SnapdragonStudios/snapdragon-gsr.git) \
 Scene Assets are from Adreno GPU Code Samples (https://github.com/quic/game-assets-for-adreno-gpu-code-samples).
## Project Structure

```
SGSR2_Unity/
├── Assets/                 # Unity 资源目录
│   ├── Resources/          # 运行时资源
│   │   ├── Shaders/        # SGSR2 着色器文件
│   │   │   └── SGSR2.shader
│   │   └── UnityGLTFSettings.asset  # GLTF 导入设置
│   ├── Scenes/             # 场景文件
│   │   ├── SampleScene.unity         # 示例场景
│   │   └── ThaiCarving/    # 泰国雕刻示例模型资源
│   │       ├── ThaiCarving.gltf
│   │       ├── ThaiCarving.bin
│   │       └── Textures/
│   ├── Scripts/            # C# 脚本
│   │   ├── SGSR2.cs        # SGSR2 核心实现
│   │   ├── SGSR2_UICamera.cs  # UI 相机控制
│   │   ├── CameraController.cs  # 相机控制器
│   │   ├── MoveTest.cs     # 移动测试脚本
│   │   └── TestSR.cs       # 超分辨率测试脚本
│   └── *.meta              # Unity 元数据文件
├── Packages/               # Unity 包配置
│   ├── manifest.json       # 包依赖清单
│   └── packages-lock.json  # 包版本锁定文件
├── ProjectSettings/        # Unity 项目设置
├── LICENSE                 # 开源许可证
└── README.md               # 本说明文档
```

## Directory Descriptions

### Assets/Resources
- **Shaders/**: 包含 SGSR2 超分辨率着色器 (`SGSR2.shader`)，用于实现图像超分辨率渲染效果
- **UnityGLTFSettings.asset**: GLTF 模型导入配置，用于正确加载 glTF 格式的 3D 模型

### Assets/Scenes
- **SampleScene.unity**: 主示例场景，展示 SGSR2 功能的完整演示
- **ThaiCarving/**: 来自 Adreno GPU Code Samples 的示例模型资源
  - `ThaiCarving.gltf` / `ThaiCarving.bin`: 泰国雕刻 3D 模型
  - `Textures/`: 模型纹理贴图

### Assets/Scripts
- **SGSR2.cs**: SGSR2 核心功能实现，处理超分辨率渲染的主要逻辑
- **SGSR2_UICamera.cs**: UI 相机控制脚本，管理界面相关的相机行为
- **CameraController.cs**: 通用相机控制器，提供视角控制功能
- **MoveTest.cs**: 移动测试脚本，用于测试对象移动
- **TestSR.cs**: 超分辨率功能测试脚本

### Packages
包含 Unity 包依赖配置：
- `manifest.json`: 定义项目所需的 Unity 包和依赖项
- `packages-lock.json`: 锁定已安装包的确切版本

### ProjectSettings
Unity 项目配置文件，包含项目特定的设置和偏好。
