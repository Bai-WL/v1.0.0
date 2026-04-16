# .clang-format
# 基于 Google 风格，适合 C++ 项目

BasedOnStyle: Google

# 缩进宽度（空格数）
IndentWidth: 4

# 访问修饰符（public:/private:）的缩进
AccessModifierOffset: -1

# 每行字符上限，超过会尝试换行
ColumnLimit: 100

# 连续赋值时对齐等号
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false

# 对齐 trailing 注释
AlignTrailingComments: true

# 大括号风格：Attach（不换行）
BreakBeforeBraces: Attach

# 允许短函数、短循环、短if写在同一行
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: true
AllowShortLoopsOnASingleLine: true

# 构造函数初始化列表换行风格
ConstructorInitializerAllOnOneLineOrOnePerLine: false
ConstructorInitializerIndentWidth: 4

# 连续换行缩进
ContinuationIndentWidth: 4

# 指针/引用的对齐方式：左对齐（int* a）
PointerAlignment: Left

# 注释换行时保留原有缩进
ReflowComments: true

# 排序 #include 块
SortIncludes: true

# 允许 case 语句与 switch 缩进一致
IndentCaseLabels: false

# 命名空间内容不缩进
NamespaceIndentation: None

# 不使用 Tab，永远用空格
UseTab: Never
