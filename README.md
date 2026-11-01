# Logic Lyrics & Sheet

一个跑在 Logic Pro 里的 AU 效果器：按播放位置滚动 LRC 歌词，按手动打的时间点自动翻 PDF / 图片 / GIF 乐谱，支持偏移校准、跨电脑携带乐谱文件，以及只显示歌词或乐谱的“演奏模式”。

---

## 重要说明：这是一个 Vibe Coding 产物

这个项目是**通过 AI 辅助的 vibe coding 方式生成**的：需求由使用者口述，代码、调试和验证由 AI 与使用者多轮迭代完成，**没有经过逐行的人工代码审查**。

这意味着：

- 功能是真实可用的（构建、`auval` 验证、离屏渲染、跨机恢复都实际跑过）；
- 但代码质量、边界情况和长期可维护性**不保证达到手写生产代码的标准**；
- 遇到 Logic 版本升级、异常工程、特殊文件时可能出问题；
- 分发前请先确认许可证（见文末“许可”）。

项目**有意保持代码量小、结构直白**，方便任何人（包括继续用 AI 辅助）在这个基础上接着改。如果你要继续开发，建议先读“工作原理”和“继续开发指南”两节。

---

## 开发背景

做音乐时经常遇到两个需求，Logic 原生都不太好解决：

1. **歌词跟唱**：LRC 是带时间戳的纯文本，Logic 没有“按工程时间滚动歌词”的显示工具。外部歌词软件（LyricsX 等）只跟 Apple Music/Spotify，不跟 DAW 播放头。
2. **翻谱**：PDF 乐谱阅读器（forScore、Newzik 等）要靠脚踏板或自带音频，不能跟随 Logic 的播放位置；Logic 的 Marker 也不能触发任何事件。

调研后确认市面上没有开箱即用的方案，于是做了这个插件。核心思路很简单：

- 插件在 `processBlock` 里从宿主读取播放位置；
- 歌词和翻页点统一换算到一条“内容时间轴”；
- 歌词按当前行平滑滚动，乐谱按翻页点二分查找目标页；
- 所有素材随工程状态保存，换电脑也能打开。

---

## 功能

- LRC 歌词：解析 `[mm:ss.xx]`、多时间戳、`[offset:]`、逐字标签；当前行高亮、平滑滚动、字号可调。
- 乐谱：PDF（多页）、PNG/JPG/TIFF/BMP、GIF（**每一帧算一页**）；支持多选批量导入、追加导入。
- 校准：整体起点捕获 + 毫秒级偏移滑杆。
- 翻页点：播放中一键记录“当前时间 → 第 N 页”，列表可预览、删除、清空。
- 演奏模式：隐藏所有设置，只显示歌词或乐谱，带歌词/乐谱切换、翻页和退出浮层。
- 持久化：歌词文本、乐谱文件内容、偏移、翻页点、演奏模式全部随工程保存；乐谱会嵌入工程，换电脑无需原文件。
- 直通效果器：不改变声音，只读走带信息。

---

## 安装

### 方式 A：安装包（推荐）

`dist/LogicLyricsAndSheet-1.0.1.pkg`，双击安装，需要管理员密码，装到：

```
/Library/Audio/Plug-Ins/Components/LyricsAndSheet.component
```

系统提示“无法验证开发者”时：右键点 pkg → 打开；或 系统设置 → 隐私与安全性 → 仍要打开；或：

```bash
sudo installer -pkg LogicLyricsAndSheet-1.0.1.pkg -target /
```

### 方式 B：免管理员密码

解压 `dist/LogicLyricsAndSheet-1.0.1.zip`，右键点击 `Install-User.command` → 打开。装到：

```
~/Library/Audio/Plug-Ins/Components/LyricsAndSheet.component
```

两种方式二选一，不要同时装。装完**完全退出并重新打开 Logic**。

### 从源码构建

依赖：macOS 12+、Xcode Command Line Tools、CMake、Ninja、JUCE 8.0.15。

```bash
brew install cmake ninja
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git .build/JUCE

cmake -S . -B .build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/cmake --target LogicLyricsAndSheet_AU LogicLyricsAndSheet_Standalone -j 10
```

构建会自动把 AU 复制到 `~/Library/Audio/Plug-Ins/Components`。

打通用二进制 + 安装包：

```bash
./Scripts/package.sh      # 产出 dist/*.pkg 和 dist/*.zip
```

---

## 在 Logic 中使用

1. **插入插件**：在一条有音频的轨道，或 `Stereo Out` 上，音频效果里选 `Local > LyricsAndSheet`。
   > 插件是直通效果器。放在没有音频流的空轨上时，Logic 不会持续回调，拿不到播放位置。
2. **加载歌词**：点“打开 LRC”，选择 `.lrc` / `.txt` 文件。
3. **加载乐谱**：点“打开乐谱”，可**多选** PDF / 图片 / GIF；点“追加”把文件加到现有页列表后面。
   - 多文件按文件名自然排序（`1, 2, 10` 不会排成 `1, 10, 2`）；
   - PDF 按页展开；
   - GIF 按帧展开（单帧 GIF 就是 1 页）。
4. **设置起点**：播放到“歌词 0:00 应该在的位置”，点“设为起点”。
   - 停止状态下点击会进入**预备模式**：按下播放后，在第一个位置回调处自动记录。
   - 原因：Logic 停止时不给插件任何回调，停在原地无法读取播放头。
5. **微调偏移**：拖“偏移”滑杆，正数让歌词/翻页更晚发生。和 LRC 的 `[offset:]` 叠加。
6. **记翻页点**：在“页码”里选目标页，到换页那一刻点“记翻页点”。
   - 翻页点按内容时间保存，拖动播放头、循环播放都能正确落页；
   - 双击列表里的翻页点可在停止时预览；
   - 播放中手动翻页会保持，直到经过下一个翻页点才自动接管。
7. **演奏模式**：点“演奏模式”，隐藏所有设置。右上角浮层：

   ```
   [歌词] [乐谱]  ‹  ›  [退出]
   ```

   模式随工程保存，下次打开插件窗口直接进入演奏界面。

所有设置随工程保存在插件状态里。

---

## 数据与跨电脑迁移

| 内容 | 存储方式 | 是否随工程走 |
|---|---|---|
| LRC 文本 | 插件状态里的 base64 | ✅ |
| PDF / 图片 / GIF | gzip + base64 嵌入插件状态（同一文件只存一份） | ✅（单文件 ≤ 64 MB） |
| 起点 / 偏移 / 字号 / 页码 / 翻页点 / 演奏模式 | 插件状态属性 | ✅ |

在新电脑打开工程时：

- 原文件路径存在 → 直接用原文件；
- 原文件不存在 → 把嵌入的副本写到 `~/Library/Application Support/LogicLyricsAndSheet/assets/`，从缓存加载。

超过 64 MB 的文件不会嵌入，只保存路径，换电脑会丢（缓存目录可安全删除，会按需重建）。

---

## 工作原理

### 音频单元

- 类型：AUv2 效果器（`aufx`），Manufacturer `LyrS`，Subtype `LySh`，名称 `LyricsAndSheet`。
- 直通处理，不修改音频；`processBlock` 只做一件事：读走带。
- 位置优先取 `timeInSamples / sampleRate`，退化时用 `timeInSeconds`；停止用“300 ms 没有回调”推断。

### 时间轴

歌词和翻页点统一用**内容时间**：

```
projectSeconds = host 给的项目秒数
contentTime    = projectSeconds - origin - fineOffset
```

- `origin`：起点（粗校准，由“设为起点”捕获）。
- `fineOffset`：毫秒级微调（细校准，滑杆）。
- 翻页点保存为 **内容时间**，所以改起点不会破坏已记录的点。

### 翻页点解析

不是维护“下一条指针”，而是**二分查找最后一个 `time <= contentTime` 的 cue**：

- 拖动播放头、循环回卷、从任意位置起播都能落到正确页；
- 只有当 cue 序号变化或发生跳转时才套用页，手动翻页不会被每帧覆写。

### 状态

`getStateInformation` 生成 `LYRICS_SHEET` ValueTree：文本、页面列表（路径 + PDF 页 + GIF 帧）、`ASSETS`（压缩后的文件内容）、cue 列表。`ASSETS` 按页面路径去重。

### 目录结构

```
CMakeLists.txt                 JUCE + CMake 工程
Source/
  Core/LrcDocument.*           LRC 解析 / 行定位（不依赖 JUCE）
  Core/CueModel.*              翻页点模型 / 二分查找（不依赖 JUCE）
  PluginProcessor.*            AU 主体、走带采集、状态序列化、素材嵌入
  PluginEditor.*               全部 UI：歌词、乐谱、翻页点、演奏模式
  PdfRenderer.h / .mm          Core Graphics 渲染 PDF 页
  GifFrames.h / .mm            ImageIO 读取 GIF 帧
Scripts/
  package.sh                   通用二进制构建 + pkg/zip 打包
  Install-User.command        用户级安装脚本
  README-install.txt             分发给别人的安装说明
```

`Core/` 不依赖 JUCE，可以单独编译测试。

---

## 已知限制

- **停止时读不到播放头**：AUv2 在停止后不再回调，所以“设为起点”在停止状态下是预备模式。这是宿主机制，不是 bug。
- **插件窗口不是自由窗口**：能拖到第二块屏、能拉大，但不能进 macOS 全屏，也不能脱离插件实例常驻。
- **GIF 不播放动画**：每一帧当作一页，用于翻页。
- **PDF 页切换是即时渲染**（约几十毫秒），未做预渲染缓存。
- **乐谱是路径 + 嵌入副本**：超过 64 MB 的文件不嵌入。
- **没有自动识谱/跟随**：翻页完全依赖手动打点。
- **未签名公证**：ad-hoc 签名，首次安装会被 Gatekeeper 拦一次。
- **没有提交自动化测试**：验证靠 `auval`、临时的离屏渲染/状态往返脚本完成。

---

## 继续开发指南

项目刻意做小，改起来应该很直接。几个常见切入点：

### 1. 换一个时钟来源（MTC / 独立 App）

目前只有“插件直读宿主”一种时钟。可以把时间来源抽象成接口：

```
ClockSource
  ├─ HostClock        （现有：processBlock 读 timeInSamples）
  ├─ MtcClock         （接收 IAC 上的 MTC，做 standalone 时用）
  └─ BridgeClock      （瘦插件通过 localhost UDP/OSC 推给独立 App）
```

只影响 `PluginProcessor` 里写 `projectSeconds` 的地方，`contentTime` 之后的所有逻辑不用动。

### 2. 演奏模式做成独立窗口

当前是“插件窗口内全屏显示”。若要做可全屏、可常驻第二屏的显示端，需要拆成“瘦插件 + companion App”，时钟走 UDP/OSC。

### 3. 给 PDF 加预渲染缓存

`SheetView::renderPage` 每次换页都调用 `PdfDocument::renderPage`。可以按（路径, 页号）缓存 `juce::Image`，翻页会更跟手。

### 4. 翻页点编辑

`CueListComponent` 目前只有预览/删除/清空。可以加拖动改时间、直接改页号、导入导出 JSON。

### 5. 动画 GIF

现在是“按帧分页”。若要做播放动画，用 `GifFrames` 已读到的帧序列 + 延时，在 `SheetView` 里加定时器即可。

### 6. 自动化测试

`LrcDocument` 和 `CueModel` 是纯 C++，加一个 `ctest` 目标成本很低；UI 可以用现有的离屏渲染方式做快照测试。

### 构建与验证

```bash
# 开发构建（自动安装到 ~/Library/...）
cmake -S . -B .build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/cmake --target LogicLyricsAndSheet_AU -j 10

# 验证 AU
auval -v aufx LySh LyrS

# 打安装包
./Scripts/package.sh
```

改完插件后如果 Logic 还显示旧版，重启 Logic，或：

```bash
killall -9 AudioComponentRegistrar
```

---

## 路线图（按价值排序）

- [ ] MTC / standalone 时钟源，支持全屏独立显示窗
- [ ] PDF 页预渲染缓存
- [ ] 翻页点列表可编辑、导入导出
- [ ] 动画 GIF 播放
- [ ] 歌词逐字高亮（增强 LRC）
- [ ] 正式签名 + 公证（需要 Apple Developer Program）
- [ ] 单元测试与 UI 快照测试

---

## 许可

项目本身**暂未指定开源许可证**，分发前请自行添加（例如 MIT）。

第三方依赖：

- **JUCE 8.0.15**：AGPLv3 或商业许可二选一。以 AGPLv3 分发源码时需遵守其条款；闭源分发需购买 JUCE 商业许可。
- **Apple 框架**：Core Graphics、ImageIO、CoreText（系统自带）。
- 安装包里的 ad-hoc 签名不涉及任何第三方证书。
