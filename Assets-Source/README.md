# 资产来源与授权

本项目使用以下明确允许再分发的免费/开源资产。

## 角色模型

- **Dragon.obj**（玩家奶龙 / 黑神话 BOSS 共用）
  - 来源：Quaternius「Free Animated Monster Pack」
  - 原始下载：https://quaternius.com/packs/animatedmonster.html
  - 镜像仓库：https://github.com/beep2bleep/FreeAssetsByKenneyNLandQuaternius
  - 授权：Creative Commons Zero (CC0) 1.0
  - 说明：
    - 原始文件为带动画的 `Dragon.fbx`，O3DE 场景构建器不支持其骨骼/动画格式。
    - 使用 `assimp` 转换为无骨骼、无动画的静态 `OBJ/Dragon.obj`，并重新调配色板（黄身体、浅黄腹部、深爪/翼、黑眼睛）以呈现奶龙风格。
    - 原始 `Dragon.fbx` 保留在 `Docs/OriginalAssets/Quaternius/MonsterPack/Dragon.fbx`，仅作归档，不参与 O3DE 资产处理。

## 音效

- **Impact Sounds（攻击/受击音效）**
  - 来源：Kenney「Impact Sounds」
  - 镜像仓库：https://github.com/Daarko/sparkstream-sounds
  - 授权：Creative Commons Zero (CC0) 1.0
  - 说明：玩家/BOSS 攻击命中与受击音效。

- **Dragon Roar（BOSS 咆哮）**
  - 来源：Freesound - "Dragon Roar" by tambascot
  - 链接：https://freesound.org/s/203105/
  - 授权：Creative Commons Zero (CC0) 1.0
  - 说明：BOSS 攻击/登场时的咆哮。

## 材质

- `YellowMythNailong/Assets/Materials/YellowNailong_*.material` 为项目自制的 O3DE StandardPBR 材质，可用于后续进一步替换龙模型各材质槽。
- 当前玩家龙直接使用 OBJ 自带的多材质（Main/Belly/Claws/Wings/Eyes）默认生成材质；BOSS 龙使用 `darkboss.azmaterial` 整体覆盖为深色。

所有资产均遵守其原始授权协议；CC0 资产允许自由修改与商业使用。
