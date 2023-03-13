# aise
Automatic Instruction Set Extension

## 简介
通用指令系统已可以满足大部分应用的功能需求；但对于特定应用，为了获取更高性能、更优的能效比，指令系统扩展成为重要的技术手段之一。传统的指令系统扩展流程需要较多的人工参与，有较高的时间成本，其结果也一定程度依赖于经验。因此，我们有必要通过设计自动化的方法来加速这一流程，并使用设计空间探索的方法提升结果质量。

本文提出了一个基于软硬件协同设计方法的指令系统扩展流程。该流程针对RISC架构下的多输入单输出指令，实现了从应用程序的高层次描述生成扩展指令，根据目标函数进一步筛选扩展指令，和在LLVM中间代码中应用扩展指令的过程。实验表明，该流程可以有效且快速地发现、选择和应用扩展指令。

本文的贡献有：
1. 在指令集生成上，实现了根据给定应用的C代码，在其中遍历MISO指令的流程；
2. 设计了一种扩展指令的正规表示形式，该形式易于阅读，且不包含重复的指令；
3. 在指令集选择上，实现了使用遗传算法进行指令集选择的过程；
4. 在指令应用上，实现了在程序的LLVM中间代码中应用扩展指令的流程；
5. 在6个测试程序上进行了实验，验证了上述流程的有效性。

## 使用方法
### 安装LLVM
* 参考 [Getting Started with the LLVM System](https://llvm.org/docs/GettingStarted.html)，请将LLVM加入`PATH`中

### 编译本项目
* 在本目录运行`make`即可，它会自动和LLVM库链接
* 生成的可执行程序为`main`

### 寻找程序热点函数
* 使用Perf工具记录程序的热点函数，采样率为999Hz
  ```bash
  $ perf record -F 999 <exe_path>
  ```
* 生成分析报告，可根据运行时间占比得到热点函数
  ```bash
  $ perf report
  ```
* 使用LLVM编译程序生成LLVM IR，生成文件的后缀名为`.bc`
  ```bash
  $ clang -emit-llvm -O3 -c -o hello.bc hello.c
  ```
* 从LLVM IR中提取出热点函数，单独作为一个`.bc`文件存放
  ```bash
  $ llvm-extract -func=a -o a.bc hello.bc
  ```

### 遍历MISO指令
* 假设最大输入为2，使用下面指令遍历MISO
  ```bash
  $ ./main enum -max-input 2 -o result.miso.txt a.bc 
  ```

### 使用遗传算法选择指令
* 先安装[遗传算法库](https://pypi.org/project/geneticalgorithm/)
  ```bash
  $ pip3 install geneticalgorithm
  ```
* 直接运行Python脚本
  ```bash
  $ python3 genetic.py
  ```
* 注：目前脚本没有输入，如果要改输入的path需要直接改脚本

## 原理

### 遍历MISO指令
* 我使用了[Atasu K et al., IJPP 2003](https://infoscience.epfl.ch/record/53109/files/AtasuDec03_AutomaticApplicationSpecificInstructionSetExtensionsUnderMicroarchitecturalConstraints_IJPP.pdf)的算法，该算法可以完整地遍历DAG中所有多输入单输出的指令（可限制输入数），缺点是算法复杂度较高
* 基本概念
  * **子图：**图的节点的一个子集和在该子集中的节点之间直接连接的所有边构成的图
  * **可调度性：**若图中一个节点的输入来自于图外，而且向上追溯又依赖于图中另一个节点的输出，则这样的指令不符合硬件一个周期执行的原则，称为不可调度
* 核心思想：

### MISO指令表示

### 指令集合选择
