# TODO — 算法与应用解耦（soundproof FetchContent 迁移）

> 背景：删除内嵌副本 `src/audio_engine`（543 文件），TrickRoom 通过 FetchContent 拉取
> [soundproof](https://github.com/Ryuk17/soundproof.git)（`GIT_TAG master`）。
> 已确认决策：① Git fetch master，本地开发用
> `-DFETCHCONTENT_SOURCE_DIR_SOUNDPROOF=F:/share/soundproof` 覆盖；② 依赖
> （abseil/neon-fft/pffft/eigen）由 soundproof 自行拉取，TrickRoom 删除相关声明。

## 任务列表

- [x] **1. 重写 CMakeLists.txt**（整文件替换）
  - 删除 ABSEIL/NEON_FFT/PFFFT/EIGEN 变量、依赖 FetchContent 块、`NEON_FFT_SOURCE_DIR`/`PFFFT_SOURCE_DIR`/`EIGEN3_ROOT` 传递、CMP0169 处理、`add_subdirectory(src/audio_engine)`
  - `BUILD_AUDIO_ENGINE` 块改为 `FetchContent_Declare(soundproof GIT_REPOSITORY ... GIT_TAG master GIT_SHALLOW TRUE)` + `FetchContent_MakeAvailable(soundproof)`
  - trickroom include 目录：`src/audio_engine` → `${soundproof_SOURCE_DIR}/soundproof`
  - 新增 4 个 AE 目标存在性守卫（AE_AEC/AE_NS/AE_AGC2/AE_SRC，configure 期提前报错）
  - 保留：option 名 `BUILD_AUDIO_ENGINE`、强制 ON 逻辑、`absl::strings` 链接（冗余但零成本）、OPENAL/OPUS 变量（移入 trickroom 块）
  - 更新文件头注释（soundproof 来源 + 本地覆盖用法）

- [x] **2. 删除内嵌副本** `src/audio_engine`
  - `git rm -r src/audio_engine`，再 `rm -rf src/audio_engine` 兜底
  - 注意：`src/audio_engine/CLAUDE.md`（算法约束文档）随目录删除，内容可从 git 历史恢复；迁移到 soundproof 需用户另行确认

- [x] **3. 更新 README.md / README_zh.md**
  - 目录结构树、独立构建命令（`cmake -S src/audio_engine ...` → soundproof 仓库内）、产物路径、toolchain 路径
  - 补充：TrickRoom 通过 FetchContent（git，master）拉取 soundproof 及本地覆盖用法

- [x] **4. 更新 .claude/skills/wrap-audio-algo/SKILL.md 路径**
  - `src/audio_engine` → `F:/share/soundproof`；`audio_processing` → `soundproof/core`；`signal_processing` → `soundproof/dsp`；`unitest` → `tests`；`cmake/libAE_XXX.cmake` → 单一扁平 CMakeLists.txt 对应段落
  - skill 完整重写列为后续跟进项

- [x] **5. 验证编译**
  - 实测通过（2026-11-01，CMake 4.2.1 + MinGW g++ 15.2.0，网络经 7897 代理）
  - git 拉取路径：soundproof 克隆到 master @ 43b9624 ✓；全量编译 0 错误
    → `bin/trickroom.exe`（48 MB）、`build/_deps/soundproof-src/lib/libAE_*.a`（12 个）、12 个 `test_ae_*.exe` 全部生成
  - 本地覆盖路径：configure 通过，trickroom 的 include 正确解析到 `F:/share/soundproof/soundproof{/interface,/utils,/dsp}`；
    本地仓库与拉取副本源码一致（仅 LF/CRLF 差异），即全量编译同样覆盖该路径
  - ⚠️ 遗留：本机 CMake 4.2.1 下 openal-soft 1.21.0 的 `cmake_minimum_required`（<3.5）被拒，
    需加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` 才能配置（**与本次迁移无关的既有问题**，待决定是否写进 CMakeLists）
  - 未执行：`ctest`（测试会向 soundproof 源码树 `assets/` 写输出 wav）

## 范围外事项

- CLAUDE.md 迁移到 soundproof（修改项目目录之外的路径，需用户允许）
- git 提交（用户未要求；改动留在工作区，需要时走 /git-commit）

## 风险与对策

| 风险 | 对策 |
|---|---|
| `GIT_TAG master` 不自动拉新提交 | 本地覆盖是主开发路径；非覆盖用户 `rm -rf build/_deps/soundproof-*` 后重配 |
| `GIT_SHALLOW TRUE` 无法切旧 commit | 删除 `_deps/soundproof-*`；soundproof 打 tag 后可改 pin tag |
| 用户关闭 `BUILD_AE_AEC` 等选项 | foreach 守卫在 configure 期清晰报错 |
| 旧 build 目录缓存旧变量 | 验证步骤强制 `rm -rf build` |
| soundproof 注册 12 个 ctest + NeFFT/test_fft 副产物 | 无害；测试用 `ctest -R` 筛选 |
