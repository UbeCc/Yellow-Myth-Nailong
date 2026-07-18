# 黑神话奶龙 · Yellow Myth: Nailong

一个致敬《黑神话：悟空》的 3D 动作原型，主角为原创“奶龙风格”小黄龙。使用 Open 3D Engine (O3DE) 26.05 与 Atom 渲染器。

## 当前状态

- Linux 上 `GameLauncher` 可直接启动并加载 `DefaultLevel`。
- 玩家与 BOSS 已替换为真实的 Quaternius 卡通龙静态模型（CC0）。
  - 玩家为黄色奶龙风格：黄身体、浅黄腹部、深色爪翼、黑眼睛。
  - BOSS 为同模型的深色放大版巨龙。
- 已实现：WASD 移动、空格闪避、Enter 重攻击、鼠标左键轻攻击。
- BOSS 会在仇恨范围内追击并攻击，玩家与 BOSS 可互相造成伤害，有死亡/胜利判定。

## 环境要求

- O3DE 26.05（已安装到 `/opt/O3DE/26.05`）
- Linux (Ubuntu 24.04) + Vulkan 显卡
- Python 3（O3DE 自带）
- 远程游玩额外需求：
  - shouyun1 上已有 NVIDIA X display `:2`
  - `x11vnc` 在端口 `5902` 共享该显示器
  - `openbox` 作为窗口管理器（`run_game.sh` 会自动拉起）

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

### 在 shouyun1 上直接游玩

```bash
/root/Yellow-Myth-Nailong/run_game.sh
```

等价手动命令：

```bash
export DISPLAY=:2
ulimit -n 65536
cd /root/Yellow-Myth-Nailong/YellowMythNailong/build/linux/bin/profile
./YellowMythNailong.GameLauncher \
    --project-path=/root/Yellow-Myth-Nailong/YellowMythNailong \
    --rhi=Vulkan
```

启动后如果弹出 Asset Processor 的 *Startup Errors* 对话框（通常是 ALSA 音频警告），直接关闭即可进入游戏画面。

### 无显示环境验证

```bash
./build/linux/bin/profile/YellowMythNailong.GameLauncher --rhi=Null -bg_ConnectToAssetProcessor=0
```

## 从 macOS 远程游玩

1. 在本地 Mac 终端建立 SSH 隧道（保持运行）：

   ```bash
   ssh -L 5902:localhost:5902 -N shouyun1
   ```

2. 使用 macOS 自带的屏幕共享连接：

   ```bash
   open vnc://localhost:5902
   ```

   `x11vnc` 未设置密码，连接时按提示选择 **继续**（未加密）即可。

3. 在 VNC 桌面里打开终端，运行：

   ```bash
   /root/Yellow-Myth-Nailong/run_game.sh
   ```

4. 游戏窗口会出现在 VNC 桌面中，即可用鼠标键盘操作。

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
Docs/OriginalAssets/     # 原始 FBX 等不参与资产处理的归档
run_game.sh              # shouyun1 一键启动脚本
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

本项目采用 MIT 协议。第三方资产授权见 `Assets-Source/README.md`。
