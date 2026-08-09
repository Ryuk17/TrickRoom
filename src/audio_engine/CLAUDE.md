<!--
 * @Author: Ryuk
 * @Date: 2026-07-27 22:56:46
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 21:33:58
 * @Description: First create
-->
# 功能
将webrtc算法封装成统一的接口形式，接口相关定义在`src\audio_engine\interface`中。需要进行如下步骤。相对路径以git项目顶层为起点，当前路径为`src\audio_engine`。
- 分析`src\audio_engine\audio_processing`中的算法，以及`src\audio_engine\unitest\internal`中对应算法源码调用测试文件
- 将`src\audio_engine\audio_processing`中的算法封装成统一接口形式，接口形式参考`src\audio_engine\interface`
- 封装后需要修改`src\audio_engine\CMakeLists.txt`编译使用封装接口的库，库名称格式为`libAE_xxx.a/so/dll`(比如libAE_AEC.a)
- 在`src\audio_engine\unitest`中完成依赖`libAE_xxx.a/so/dll`对应的测试文件，测试输入和输出可以参考`src\audio_engine\unitest\internal`


# 目录说明
`audio_processing`: 算法核心实现  
`signal_processing`: 算法实现计算相关的公共代码  
`data`: 音频测试数据
`interface`: 对外接口  
`unitest`: 算法单元测试代码  
`third_party`: 第三方依赖代码  
`toolchains`: 编译相关toolchains    
`utils`: 算法依赖的公共代码(与计算无关)  
`CMakeLists.txt`: 算法编译CMakeLists.txt

# 改动约束
- 除非经过通义不能修改`src\audio_engine\interface`以外的任何代码
- 改动前先发出计划review，通过后方可执行
- 本项目为跨平台方案LINUX/WIN/嵌入式都需要支持
- `src\audio_engine\interface`中代码预埋log语句
- 库调用结果要求与`src\audio_engine\data`中源代码调用结果一模一样


