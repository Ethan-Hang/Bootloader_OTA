# 嵌入式C 规则参考

## 目录

- 适用范围与归一决策
- 格式与文件组织
- 命名与可见性
- 安全性与健壮性
- 审查检查单
- CI 与静态检查示例

## 适用范围与归一决策

- 本文是 `嵌入式C语言编码规范文档` 的工作参考版本，供  在实际改码和审查时快速加载。
- 默认执行全部 MUST 规则；SHOULD 规则在性能、平台限制、兼容性或明确用户意图存在冲突时可以带理由放宽。
- 当原始规范文本出现口径不一致时，按以下优先级裁决：
- 安全性与正确性优先
- 正文详细说明与示例优先
- 仓库内稳定公共接口与既有约定优先
- 摘要表中的简写结论最后参考
- 在项目自有代码中新建标识符时，使用以下归一规则：
- 文件名与函数名：`snake_case`
- 局部变量与参数：`lowerCamelCase`
- 文件级和全局可变状态：`g_` + `lowerCamelCase`
- 宏、枚举值、标签：`UPPER_SNAKE_CASE`
- `typedef` 抽象类型：`PascalCase`
- 对已有公共 API、厂商符号、寄存器名和生成代码符号，除非用户明确要求，否则不要仅因风格统一而重命名。
- 原始文档核心面向 `.c` 文件；只有在修改接口时，才把相关规则同步应用到 `.h`。

## 格式与文件组织

- 使用空格缩进，每级 4 个空格，禁止 TAB。
- 行宽控制在 120 列以内；URL、命令、`#include`、`#error` 可按需例外。
- 使用 K&R 大括号风格：
- 函数左大括号另起一行
- 控制语句左大括号跟在语句行尾
- 右大括号一般单独占行，除非后接 `else`、`else if` 或 `do { } while (0)` 尾部
- `if`、`else`、`for`、`while`、`do`、`switch` 必须带大括号。
- `case`、`default` 相对 `switch` 缩进一级。
- 一行只写一条语句。
- `.c` 文件推荐组织顺序：
- 文件头注释
- `#include`
- 宏与常量
- 类型定义
- 文件级 `static` 变量
- 内部声明
- 内部 `static` 函数
- 对外函数
- 新建 `.c` 或 `.h` 文件时必须使用以下文件头注释模板（替换对应字段）：
  ```c
  /******************************************************************************
   * @file <filename>
   *
   * @par dependencies
   *
   * @author <Author Name>
   *
   * @brief <Brief description>
   *
   * @version V1.0 <YYYY-M-D>
   *
   * @note 1 tab == 4 spaces!
   *****************************************************************************/
  ```
- 新增函数或大幅改写函数时，补充 Doxygen 注释。
- 注释默认用英文，解释意图、边界、约束和硬件行为，并在代码变化时同步更新。

## 命名与可见性

- 文件名使用 `snake_case`。
- 函数名使用 `snake_case`。
- 局部变量和参数使用 `lowerCamelCase`。
- 文件级或全局可变变量使用 `g_` 前缀。
- 宏和枚举值使用 `UPPER_SNAKE_CASE`。
- 新引入的抽象类型优先使用 `PascalCase`。
- 尽量减少全局状态；不需要外部链接的符号统一使用 `static`。
- 结构体若不需要自引用，优先使用匿名 `typedef struct { ... } TypeName;`；需要自引用时再使用带 tag 的形式。

## 安全性与健壮性

- 一个函数尽量只承担一个职责。
- 在可行前提下，函数控制在 50 行非空非注释代码以内，嵌套不超过 4 层。
- 对所有可能失败的 API 必须检查返回值。
- 未确认成功前，不要先使用调用结果。
- 同一模块内保持一致的返回值语义。
- 对以下外部输入必须做校验：
- API 参数
- 文件或网络报文
- IPC 数据
- 环境信息
- 跨信任边界的共享全局状态
- 禁止使用 `assert` 校验外部输入。
- 对所有自有资源保证退出路径可释放，包括：
- 动态内存
- 锁和互斥量
- 队列、信号量、句柄
- 文件描述符和设备句柄
- 复杂资源流优先使用 `cleanup:` 统一回收。
- 能用函数解决就不用函数式宏。
- 宏不可避免时，表达式整体和参数都要完整加括号，多语句宏必须使用 `do { ... } while (0)`。
- 共享文件级状态若会跨线程或中断使用，必须说明同步策略。
- 针对 32/64 位可移植性：
- 指针运算使用 `uintptr_t` 或 `uint8_t *`
- 禁止将指针通过 `uint32_t` 传递
- 将 `size_t` 缩窄到 `int32_t` 或 `uint32_t` 前必须先做边界检查
- 循环索引和长度类型必须与边界宽度一致
- 日志优先走结构化项目日志接口，不输出敏感内容。
- 需要保留的调试代码必须加编译开关保护。

## 审查检查单

- 新建 `.c` 文件是否包含完整文件头。
- 是否存在 TAB、行尾空格、无法解释的超长行。
- 控制语句是否全部带大括号。
- `switch` 的 `case/default` 是否缩进一致，fallthrough 是否显式。
- 文件名与函数名是否满足 `snake_case`。
- 新增全局或文件级可变状态是否使用 `g_`，并在可行时声明为 `static`。
- 返回值是否在依赖使用前完成检查。
- 外部输入是否在作为长度、索引、偏移、分配大小、除数、格式串前完成校验。
- 是否存在对外部输入使用 `assert` 的情况。
- 所有自有资源是否能在所有退出路径释放。
- 指针运算是否使用 `uintptr_t` 或字节指针，而非 `uint32_t`。
- `size_t` 缩窄转换是否做了边界检查。
- 日志是否走项目日志链路，而不是随意遗留 `printf`。
- 调试代码是否被隔离在非发布路径之外。

## CI 与静态检查示例

以下配置作为起点，落地时再按项目编译链和目录结构调整。

```yaml
# .clang-format
# 基础代码风格
BasedOnStyle: LLVM
# 缩进与空格
IndentWidth: 4
UseTab: Never
# 换行规则
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortBlocksOnASingleLine: Never
# 对齐规则
AlignConsecutiveDeclarations:
  Enabled: true
  AcrossEmptyLines: false
  AcrossComments: false
AlignConsecutiveAssignments:
  Enabled: true
  AcrossEmptyLines: true
  AcrossComments: true
  PadOperators: true
AlignConsecutiveMacros:
  Enabled: true
  AcrossEmptyLines: true
  AcrossComments: true
  PadOperators: true
AlignTrailingComments:
  Kind: Always
  OverEmptyLines: 2
# 指针与空格
PointerAlignment: Right
# 其他
MaxEmptyLinesToKeep: 2
SortIncludes: Never

```

```yaml
# .clang-tidy
Checks: >
  -*,
  clang-analyzer-*,
  bugprone-*,
  readability-*,
  cert-err33-c
WarningsAsErrors: ""
HeaderFilterRegex: ".*"
AnalyzeTemporaryDtors: false
CheckOptions:
  - key: cert-err33-c.AllowCastToVoid
    value: "false"
```

```bash
cppcheck \
  --std=c99 \
  --enable=warning,style,performance,portability \
  --inline-suppr \
  --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  -I. \
  src/
```

```bash
# 示例脚本检查
grep -nP "\t" src/**/*.c && exit 1
grep -nP "[ \t]+$" src/**/*.c && exit 1
awk 'FNR<=80{print} FNR==81{exit}' file.c | grep -q "Copyright" || exit 1
awk 'length($0)>120{print FNR ":" $0}' file.c
```
