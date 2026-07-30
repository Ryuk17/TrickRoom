<!--
 * @Author: Ryuk
 * @Date: 2026-07-27 22:56:46
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-07-30 23:08:45
 * @Description: First create
-->
# 功能
- `src\pure`是为了实现源代码轻量化的工程，旨在`src\algo`中算法核心实现代码抽取，只保留核心部分代码
- 为了保证代码抽取的有效性，改动前请先指定计划，待我review后再执行
- 不要改动除`src\pure`目录下的其他文件，缺少的代码你可以在`src\algo`中获取
- 使用英文进行思考，使用中文进行回答


# 使用说明

编译：
```
cd src\pure\build
rm -r build && mkdir build
cmake .. -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchains/x86_64-windows.cmake
```

运行：   
不同算法会生成不同的可执行程序，具体参考`src\pure\CMakeLists.txt`中配置


# 目录说明
`audio_processing`: 算法核心实现  
`common`: 算法实现计算相关的公共代码  
`data`: 音频测试数据  
`test`: 算法单元测试代码  
`third_party`: 第三方依赖代码  
`toolchains`: 编译相关toolchains    
`utils`: 算法依赖的公共代码(与计算无关)  
`CMakeLists.txt`: 算法编译CMakeLists.txt

# 改动约束
- 所有fft必须使用`src\pure\common\neon_fft`


