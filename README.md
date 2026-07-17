# Yellow Myth: Nailong

一个致敬《黑神话：悟空》的 3A 级基调 3D 动作原型，主角为原创的“奶龙风格”小黄龙。

## 引擎

- [Open 3D Engine (O3DE)](https://o3de.org/) 26.05
- Atom 渲染器
- Lua / Script Canvas / C++ Gem

## 环境

- 主要开发/运行环境：Linux (Ubuntu 24.04, NVIDIA RTX 4090)
- 支持通过 VNC 远程连接游玩/调试
- 项目设计为可移植，后续可扩展 macOS/Windows 构建

## 操作

- `WASD` 移动
- 鼠标 调整视角
- 左键 轻攻击
- 右键 重攻击
- 空格 闪避/翻滚
- `Esc` 退出

## 项目结构

```
YellowMythNailong/       # O3DE 项目
  Assets/                # 模型、材质、贴图、动画
  Levels/                # 关卡文件
  Prefabs/               # 预制体
  Gem/                   # 项目专属 C++ Gem
  Scripts/               # Lua 游戏逻辑
  Registry/              # 引擎配置
Assets-Source/           # 原始资产来源与授权说明
Docs/                    # 设计文档
```

## 资产声明

项目中使用的第三方资产均为允许再分发的免费/CC0 资源，具体来源与授权见 `Assets-Source/README.md`。

## 运行方式

1. 在 Linux 上安装 O3DE 26.05。
2. 注册引擎：`/opt/O3DE/26.05/python/python.sh /opt/O3DE/26.05/scripts/o3de.py register --this-engine`
3. 打开项目：`/opt/O3DE/26.05/bin/Linux/profile/Default/Editor --project-path=/path/to/YellowMythNailong`
4. 加载 `Levels/MainLevel.prefab` 并点击 Play Game (Ctrl+G)。

## 授权

本项目采用 MIT 协议。第三方资产授权见各自说明。
