---
name: wrap-audio-algo
description: '将 soundproof 仓库 soundproof/core 中的 WebRTC 算法封装为统一 C 接口（libAE_xxx）。触发词：封装/接口/包装 xxx 算法、wrap algorithm、libAE_xxx。覆盖完整流程：接口设计 → 实现 → CMake 库 → 单元测试 → 与内部测试一致性验证 → git 提交。'
license: MIT
allowed-tools: Bash, Read, Write, Edit, Glob, Grep, AskUserQuestion
---

# 封装音频算法为统一 C 接口

把 soundproof 仓库 `soundproof/core` 中的某个算法（VAD、AEC、ANR、AGC...）封装成统一 C 接口库 `libAE_xxx`。**参考的已完成范例：VAD**（`soundproof/interface/audio_engine_vad.h/.cpp`、`CMakeLists.txt` 的 AE_VAD 段、`tests/test_ae_vad.cc`）。

> 算法库位于独立仓库 `F:/share/soundproof`（https://github.com/Ryuk17/soundproof），TrickRoom 通过 FetchContent 消费，不再内置副本。本 skill 的改动全部落在 soundproof 仓库内。

## 硬约束（soundproof/CLAUDE.md 规定）

- **只能修改 `soundproof/interface/` 内代码**（错误码/日志宏在 `audio_engine_def.h`，属于 interface）
- ⚠️ soundproof 仓库当前尚无 CLAUDE.md，约束以本 skill 为准；如需迁移原 `src/audio_engine/CLAUDE.md`，须经用户批准（跨项目目录）
- 跨平台：LINUX / WIN / 嵌入式
- 接口代码**每个 API 预埋日志**（`LOG_DEBUG` 入口、`LOG_WARN`/`LOG_ERROR` 异常）
- 库名 `libAE_xxx.a/so/dll`（如 `libAE_VAD`）
- 改动前先出计划 review，通过后执行
- fft要使用neon fft, ref中为其他fft参考，供替换成neon fft使用

## 一、分析阶段

1. 读 soundproof 的 `CLAUDE.md`（若存在）确认约束；否则以本 skill 的硬约束为准
2. 读该算法的内部源码 `F:/share/soundproof/soundproof/core/<algo>/` 头文件 + 调用测试 `F:/share/soundproof/soundproof/core/tests/test_<algo>.cc`，搞清楚：
   - 算法类的构造/析构、核心处理函数签名（帧长约束、采样率）
   - 输入输出数据流向（内部测试怎么喂数据、取结果）
3. 读现有 `F:/share/soundproof/soundproof/interface/audio_engine_<algo>.h/.cpp` 草稿（若有）—— 通常有编译错误，逐项修复
4. 参考已完成的 `audio_engine_vad.h`（正确范本）与 `audio_engine_aec.h`（未完成范本）的接口风格

## 二、接口设计模式（VAD 沉淀的结论）

```c
typedef void* XxxHandle;                          /* opaque handle */

typedef struct { /* 初始化配置：采样率、帧长、通道数等 */ } XxxInitConfig;
typedef struct { /* 运行时配置 */ } XxxRtConfig;

AUDIO_ENGINE_API XxxHandle AudioEngine_Xxx_Create(void);
AUDIO_ENGINE_API int AudioEngine_Xxx_Destroy(XxxHandle handle);
AUDIO_ENGINE_API int AudioEngine_Xxx_Init(XxxHandle handle, const XxxInitConfig* c);
AUDIO_ENGINE_API int AudioEngine_Xxx_SetParam(XxxHandle handle, const XxxRtConfig* c);    /* 增量：只覆盖传入字段 */
AUDIO_ENGINE_API int AudioEngine_Xxx_ResetParam(XxxHandle handle, const XxxRtConfig* c);  /* 全量：先回默认再应用 */
AUDIO_ENGINE_API int AudioEngine_Xxx_Process(XxxHandle handle, const short* in, ..., out);
AUDIO_ENGINE_API int AudioEngine_Xxx_Deinit(XxxHandle handle);
AUDIO_ENGINE_API int AudioEngine_Xxx_Reset(XxxHandle handle);   /* 重建内部实例，保留配置 */
```

关键决策（勿回退）：

| 决策 | 理由 |
|------|------|
| **C 函数直操作数据容器**，不做中间类方法层 | `AudioEngineXxx` 降为纯数据容器（成员：`initialized_`、`init_config_`、`rt_config_`、算法实例指针），C API 函数内直接 `static_cast` 后操作。C API 就是最终接口，套 class 方法是无价值间接层 |
| `Init` 里**强制设 rt_config 默认值** | 防用户跳过 SetParam 直接 Process 读到未初始化值 |
| `SetParam` 增量 / `ResetParam` 全量重置 | 语义正确，为未来多字段扩展预留 |
| Process 严格校验：NULL → 未初始化 → 参数范围 → 帧长 | 逐层返回不同错误码 |
| **阈值比较用严格 `>`**（对齐内部测试） | 见下方"一致性验证陷阱" |
| 输入参数命名体现语义（`audio_in` 而非 `out`） | 与 AEC 风格一致，`const` 修饰的必是输入 |
| `audio_engine_def.h` 加 `AUDIO_ENGINE_STATIC` 分支 | Windows 下静态库链接时 `dllimport` 会报 `__imp_` 未定义；测试目标需 `target_compile_definitions(... AUDIO_ENGINE_STATIC)` |

头文件要点：`#include "audio_engine_def.h"`（拿 `AUDIO_ENGINE_API` 和错误码）；所有函数加 `AUDIO_ENGINE_API`；`extern "C"` 包裹。

错误码：`audio_engine_def.h` 的 `AudioEngineStatus` 已含 `INVALID_HANDLE / NULL_POINTER / NOT_INITIALIZED / PROCESS_FAILED / INVALID_PARAM`，新算法直接复用，不够再扩。

## 三、实现要点

1. `.cpp` 内定义数据容器类（含 RAII 析构释放算法实例），每个 C API 函数：
   - 入口 `LOG_DEBUG`，异常路径 `LOG_WARN`/`LOG_ERROR`
   - NULL/状态/参数校验链，返回对应错误码
2. 内部算法实例用 `new (std::nothrow)`，失败返回 `ERR_INIT_FAILED`
3. `Reset` = delete + 重新 new（保留 init/rt 配置），`Deinit` = delete + `initialized_ = false`
4. 帧长约束：Init 校验 `frame_len == sample_rate / 100`（10ms），Process 再次校验

## 四、CMake 构建

soundproof 的构建集中在一个扁平文件 `F:/share/soundproof/CMakeLists.txt`（没有 `cmake/*.cmake` 分文件）。

1. 在 `CMakeLists.txt` 中新增 `option(BUILD_AE_XXX "..." ON)`，并在对应位置加 `add_library(AE_XXX STATIC ...)`：**源文件列表直接复制该算法内部测试所用的源文件**（算法依赖的 `utils/`、`dsp/` 源文件与内部测试完全相同），仅追加 `soundproof/interface/audio_engine_xxx.cpp`
2. include 目录加公共组 `LIB_COMMON_INCLUDES`（已含 `interface/`）；`target_compile_definitions(... PRIVATE AUDIO_ENGINE_EXPORTS)`（库自身导出）；`target_link_libraries(AE_XXX PUBLIC absl::strings)`
3. `set_target_properties(AE_XXX PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${AE_PROJECT_ROOT}/lib")` → `lib/libAE_XXX.a`
4. 若新库被 TrickRoom 使用，另需在 TrickRoom 的 `CMakeLists.txt` 中加进 `target_link_libraries(trickroom ...)` 与 AE 目标存在性守卫

## 五、单元测试（tests/test_ae_xxx.cc）

三组测试（参考 `F:/share/soundproof/tests/test_ae_vad.cc`）：

1. **正常流**：Create → Init → SetParam → 逐帧 Process（WAV 读写用 `utils/dr_wav.h` 的 `DrWavReader/DrWavWriter`）→ Deinit → Destroy
2. **Reset**：处理几帧 → Reset → 继续 Process 正常
3. **错误用例**：NULL handle / 未 Init 调 Process / 非法参数范围 → 断言返回对应错误码不崩溃

在 `CMakeLists.txt` 中加一行 `add_ae_test(test_ae_xxx "test_ae_xxx.cc" AE_XXX)`：该宏执行 `add_executable` + 链接 `AE_XXX` 目标 + **`target_compile_definitions PRIVATE AUDIO_ENGINE_STATIC`** + 注册 CTest。

## 六、一致性验证（与内部测试对比）

1. 编译并运行内部测试 `test_<algo>`（生成 `assets/xxx_out.wav` 基线）
2. 编译运行接口测试 `test_ae_<algo>`（生成接口输出）
3. 对比方法优先级：
   - **先确认长度**：两输出样本数、WAV 结构（RIFF size、data chunk）一致
   - **帧级相似度**：逐帧比较 VAD 决策/能量，算匹配率
   - **字节级**：`cmp -i 44`（跳过 44 字节 WAV 头）对比 PCM
4. 差异排查（算法层已验证同进程 100% 一致时的常见陷阱）：
   - **阈值比较符**：WebRTC StandaloneVad 对 active 帧输出恰好 0.5 → `>=` 会把边界帧误判为语音。**必须与内部测试一致用 `>`**。定位方法：统计 `p == 0.5` 的帧数，若与差异帧数相等即根因
   - **缓冲 memset**：内部测试不清理缓冲区，最后不完整帧残留旧数据；接口测试若要 memset 必须对完整帧也保持一致，或统一不 memset
   - **帧长约束**：内部测试末尾帧（如 56 samples）仍喂 160 处理；接口若拒绝则总帧数差 1
   - **同进程双路径对比**（终极手段）：同一帧数据分别喂内部类与接口，决策 100% 一致则证明算法无差异，差异必在测试调用方式

## 七、git 提交

改动落在 soundproof 仓库（`F:/share/soundproof`），**不在 TrickRoom 仓库内**：先 cd 到 soundproof 按 `/git-commit` 流程提交，再按需更新 TrickRoom 中对 soundproof 的引用（`GIT_TAG master` 无需改动；若改为 pin tag/commit 则需在 TrickRoom 侧提交）。

## 完成标准

- [ ] `lib/libAE_XXX.a` 构建成功
- [ ] `test_ae_xxx` 三组测试全过
- [ ] 接口输出与内部测试输出一致性验证通过（字节级或帧级，差异有明确根因说明）
- [ ] 接口代码日志齐备、无编译警告
