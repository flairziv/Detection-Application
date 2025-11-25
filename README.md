# 目标检测系统（Armor & Buff Detection）

这是一个基于 Qt + OpenCV + OpenVINO 的目标检测桌面应用（图像/视频），包含：
- 装甲板（Armor）检测（支持红/蓝/全部目标切换）
- Buff 检测（使用 OpenVINO IR 模型）
- 图片 / 视频加载与播放
- 多种显示模式（原始、检测结果、二值化、ROI）
- 可调阈值（置信度 / NMS / ROI 大小）
- 主题（浅色 / Moonlight）
- 模型资源支持（可从资源提取或外部路径加载）
- 打包部署脚本，适配 OpenVINO 2024.6.0（`libopenvino.so.2460`）

## 快速开始

1. 克隆仓库并进入项目目录：
```bash
git clone <your-repo-url>
cd Detection   # 或项目根目录
```

2. 准备模型（放在 `model/`）：
- 装甲板模型：`model/armor.xml` 和 `model/armor.bin`
- Buff 模型：`model/buff.xml` 和 `model/buff.bin`

3. 编译并运行（以 CMake 为例）：
```bash
mkdir build && cd build
cmake .. 
make -j$(nproc)
./Detection   # 或 Detection，视你的目标文件名而定
```

> 如果你使用 Qt Creator，可直接打开 `CMakeLists.txt` / `.pro` 并构建。

---

## 功能

- 打开图片/视频并在 UI 中显示
- 视频播放控制（播放 / 暂停 / 停止 / 进度条）
- 显示模式：原始、识别结果（检测框 + 关键点）、二值化、ROI 预览
- 左侧控制面板支持：
  - 显示模式选择
  - 媒体信息显示（类型 / 分辨率 / 详细信息）
  - 参数设置（置信度、NMS、ROI 大小）
  - 检测结果列表（可导出）
- 主题切换（浅色 / Moonlight）
- 装甲板检测器支持三种目标颜色选择（红 / 蓝 / 全部）
- OpenVINO 模型推理（支持 GPU/CPU 编译优先策略）

---

## 依赖与环境

- Qt 5.12+（推荐 5.14.x）或 Qt6（需修改 CMake）
- CMake >= 3.5
- OpenCV（编译时链接）
- OpenVINO Runtime（此项目测试/适配 OpenVINO 2024.6.0，库名 `libopenvino.so.2460`）
- Linux（打包脚本面向 Linux），也可在 Windows/macOS 上移植（需修改打包步骤）

注意：OpenVINO 的共享库必须和程序运行环境一致（libopenvino 版本、插件文件 `plugins.xml`）。

---

## 开发 / 构建

示例 CMake 构建：

```bash
mkdir build && cd build
cmake .. \
  -DOpenCV_DIR=/path/to/opencv \
  -DOpenVINO_DIR=/opt/intel/openvino_2024/runtime
make -j$(nproc)
```

如果使用 Qt Creator，打开项目并使用 Qt Kit 构建。

---

## 模型与资源放置

程序加载模型的优先查找逻辑（示例）：
1. 在可执行文件目录下的 `model/`（例如与 `Detection` 同目录下）
   - `./model/armor.xml` + `./model/armor.bin`
   - `./model/buff.xml` + `./model/buff.bin`
2. 上级目录 `../model/`

---

## 运行与调试

- 直接运行程序：
```bash
./Detection
```

- 打开视频并播放：UI -> 文件 -> 打开视频 -> 点击播放按钮

---

## 项目结构

```
Detection/
├── CMakeLists.txt
├── res.qrc
├── models/
│   ├── armor.xml
│   ├── armor.bin
│   ├── buff.xml
│   └── buff.bin
├── themes/
│   ├── light.qss
│   └── moonlight.qss
├── src/
│   ├── main.cpp

│   ├── mainwindow.cpp
│   ├── mediaprocessor.cpp
│   ├── armordetector.cpp
│   ├── buffdetector.cpp
│   └── ...
├── include/
│   ├── mainwindow.h
│   ├── mediaprocessor.h
│   ├── armordetector.h
│   ├── buffdetector.h
├── ui/
│   ├── mainwindow.ui
└── README.md
```

---

## 调试建议

- 使用 `ldd ./Detection` 或 `LD_LIBRARY_PATH=./lib ldd ./Detection` 检查缺失依赖
- 打包后优先使用 `./run.sh --debug` 输出环境与库定位日志
- 模型加载失败时，在 `detector` / `detector` 的 `loadModel()` 里打印完整路径与异常信息以便定位


## 贡献

欢迎提交 Issue 或 Pull Request。请在 PR 中说明修改目的与测试方法。

---
