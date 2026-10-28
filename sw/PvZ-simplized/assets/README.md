# PvZ 离线素材

## 生成

在工程根目录执行：

```bash
make pvz-assets
```

依赖 Python 3 和 Pillow。输入为本目录素材，输出为 `../generated/` 中的头文件、清单和预览；`make clean` 可删除生成文件，不删除输入素材。构建无需访问完整资源库或下载图片。

## 目录

- `images/`、`reanim/`：两个关卡、菜单与戴夫动画使用的 95 个原始文件，含遮罩和骨骼描述。
- `fonts/DejaVuSans.ttf`：离线文字烘焙字体，授权见 `fonts/LICENSE-DejaVu.txt`。
- `source-manifest.json`：来源提交、原始文件清单及资源统计。

## 来源

[原始资源库 PvZ_Assets](https://github.com/FregD156/PvZ_Assets)，固定提交 `8ba94e95cfe7d0de5682f8258d3b932dbdac885c`。这是原作提取美术，未确认开放美术授权，不应按 CC0 或字体授权再分发。
