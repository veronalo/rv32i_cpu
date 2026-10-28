# Mario 图形素材

## 使用

`smb-litenes.chr` 是本地 8 KiB CHR 缓存，已被 Git 忽略。缺少缓存时，在工程根目录导入：

```bash
make mario-assets MARIO_ROM_C=/path/to/litenes/src/mario-rom.c
# 或从自行提供的对应 iNES 文件导入：
make mario-assets MARIO_ROM=/path/to/smb.nes
```

导入器只提取 CHR，不复制 PRG；原图块编号、2bpp 索引和透明色保持不变。原生版与 RV32I 版共用此缓存，后续构建不依赖外部 ROM 路径。

## 来源与授权

本机缓存来自 `/home/vero/ysyx2407/ysyx-workbench/am-kernels/kernels/litenes/src/mario-rom.c` 中的 `rom_mario_nes[]`。

SHA-256：`6ae7ffd9795d670954b76c19706d4f355b53f49c38a1da14027faee2c9266f10`。

该缓存为原作图形，不是 CC0 或开放授权素材。游戏代码来源见[移植接口](../PORTING.md)，公开源码不代表其图形获得相同授权。

保留的 `PlatformerPack_16x16.zip` 不参与构建；来源为 HDST 的 [Platformer pack 16x16](https://opengameart.org/content/platformer-pack-16x16)，其 CC0 说明见压缩包内 About.txt，不适用于当前 SMB CHR。
