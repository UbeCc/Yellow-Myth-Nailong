# Yellow Myth: Nailong

一个致敬《黑神话：悟空》的 3D 动作原型，主角为原创“奶龙风格”小黄龙。使用 Open 3D Engine (O3DE) 26.05 与 Atom 渲染器。

## 当前状态

- Linux 上 `GameLauncher` 可直接启动 `DefaultLevel`。
- 玩家为黄色奶龙风格卡通小龙，BOSS 为同模型的深色放大版巨龙。
- 已实现：WASD 移动、空格闪避、Enter 重攻击、鼠标左键轻攻击。
- BOSS 会在仇恨范围内追击并攻击，玩家与 BOSS 可互相造成伤害，有死亡/胜利判定。

## 环境要求

- O3DE 26.05（已安装到 `/opt/O3DE/26.05`）
- Linux (Ubuntu 24.04) + Vulkan 显卡
- Python 3（O3DE 自带）

## 构建与运行（Linux）

1. 注册引擎（只需一次）：
   ```bash
   /opt/O3DE/26.05/python/python.sh /opt/O3DE/26.05/scripts/o3de.py register --this-engine
   ```
2. 生成/更新构建系统（若 `build/linux` 不存在）：
   ```bash
   cd Yellow-Myth-Nailong/YellowMythNailong
   cmake --preset profile-linux  # 或项目使用的对应 preset
   ```
   本项目已配置好 `build/linux`，通常可直接构建。
3. 构建 GameLauncher：
   ```bash
   cd Yellow-Myth-Nailong/YellowMythNailong/build/linux
   ninja YellowMythNailong.GameLauncher
   ```
4. 运行：
   ```bash
   cd Yellow-Myth-Nailong/YellowMythNailong
   ./build/linux/bin/profile/YellowMythNailong.GameLauncher
   ```
   若通过远程桌面/VNC 运行，需设置对应显示号，例如：
   ```bash
   DISPLAY=:2 ./build/linux/bin/profile/YellowMythNailong.GameLauncher
   ```

## 操作

- `W/A/S/D`：移动
- `Space`：闪避（带短暂无敌）
- `Enter`：重攻击
- 鼠标左键：轻攻击
- 视角：当前为固定跟随相机

## 项目结构

```
YellowMythNailong/       # O3DE 项目
  Assets/                # 材质、输入绑定等资产
  Levels/                # 关卡文件（当前为 DefaultLevel）
  Gem/                   # 项目专属 C++ Gem（玩家、BOSS、战斗逻辑）
  Registry/              # 引擎配置
Assets-Source/           # 原始资产来源与授权说明
Docs/                    # 设计文档
```

## macOS / Windows 可移植性

- O3DE 官方支持 macOS 与 Windows；当前已在 Linux 完成验证。
- 将仓库克隆到目标平台后，使用对应平台的 CMake preset/工具链重新生成工程即可：
  - macOS：需 Xcode + Metal RHI，O3DE 26.05 支持 Apple Silicon/Intel（详见 O3DE 文档）。
  - Windows：需 Visual Studio 2022 + DirectX 或 Vulkan。
- 已知注意点：
  - 已启用 `Atom_Component_DebugCamera` Gem（见 `project.json`），运行时相机完全由代码创建。
  - 输入基于 `AzFramework::InputChannelEventListener`，跨平台由 O3DE 统一处理，无需平台专属输入代码。
  - 当前未使用任何 Linux 专属 API。

## 授权

本项目采用 MIT 协议。第三方资产授权见各自说明。
