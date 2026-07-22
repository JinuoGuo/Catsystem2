# CatSystem2 Tools 使用说明

CatSystem2 Tools 是一组 Windows 命令行工具，用于处理 CST 剧本、HG3 图像以及 FES、KCS、KSC zlib 容器。

## 1. 环境要求

- Windows 10 或 Windows 11，64 位系统。
- 64 位 MinGW-w64 GCC。
- CMake 3.24 或更高版本。
- 不支持 MSVC、Visual Studio 工程或其他 C++ 编译器。
- 不需要安装 OpenCV。

确认当前编译器：

```powershell
g++ --version
cmake --version
```

## 2. 编译

在项目根目录打开 PowerShell：

```powershell
cmake --preset mingw-release
cmake --build --preset mingw-release -j
```

生成的程序位于 `build/release`：

```text
build/release/
├── catsystem2-cst.exe
├── catsystem2-hg3.exe
├── catsystem2-fes.exe
├── catsystem2-kcs.exe
└── catsystem2-ksc.exe
```

程序已经静态链接 MinGW 的 libgcc 和 libstdc++，通常不需要另外复制 `libgcc_s_seh-1.dll` 或 `libstdc++-6.dll`。HG3 图像处理使用 Windows 自带的 WIC。

后续示例均在项目根目录执行，因此程序路径写为 `./build/release/<程序名>.exe`。

## 3. 通用规则

所有工具均为批处理工具，只处理指定目录第一层中的普通文件，不递归搜索子目录。

通用返回值：

| 返回值 | 含义 |
| ---: | --- |
| `0` | 操作成功 |
| `1` | 命令或参数不正确，程序显示使用方法 |
| `2` | 文件、格式、压缩数据或目录处理失败 |

如果输出目录不存在，程序会自动创建。输出目录中已有同名文件时会被覆盖，因此处理重要文件前应保留备份。

## 4. CST 剧本工具

程序：`catsystem2-cst.exe`

### 4.1 命令格式

```text
catsystem2-cst unpack [CST输入目录] [工作目录]
catsystem2-cst pack   [工作目录]    [CST输出目录]
```

省略目录时使用以下默认值：

| 命令 | 默认输入 | 默认输出 |
| --- | --- | --- |
| `unpack` | `scene` | `cst-work` |
| `pack` | `cst-work` | `scene` |

### 4.2 解包 CST

准备输入目录：

```text
scene/
├── scene01.cst
├── scene02.cst
└── scene03.cst
```

执行：

```powershell
./build/release/catsystem2-cst.exe unpack scene cst-work
```

输出结构：

```text
cst-work/
├── text/
│   ├── scene01.txt
│   ├── scene02.txt
│   └── scene03.txt
└── metadata/
    ├── scene01.cstmeta
    ├── scene02.cstmeta
    └── scene03.cstmeta
```

`text` 中保存可以翻译的标题、选择项、人物名和对白。`metadata` 保存文件头、指令类型、偏移表和不需要翻译的指令。

### 4.3 编辑 CST 文本

编辑 `text/*.txt` 时必须遵守以下规则：

- 文本文件使用 GBK 编码，不要转换为 UTF-8、UTF-16 或其他编码。
- 每一行对应一个可翻译指令。
- 不要增加或删除行，包括空行。
- 每一行都必须以换行结束，文件最后一行也不能缺少换行。
- 可以修改每行的字节长度，重新打包时程序会重新计算指令偏移。
- 不要修改脚本中的控制字符和转义内容。
- 不要修改、删除或重命名对应的 `.cstmeta` 文件。
- `.txt` 与 `.cstmeta` 必须保持同名并一一对应。

### 4.4 重新打包 CST

执行：

```powershell
./build/release/catsystem2-cst.exe pack cst-work rebuilt-scene
```

输出结构：

```text
rebuilt-scene/
├── scene01.cst
├── scene02.cst
└── scene03.cst
```

如果文本行数与元数据记录的可翻译指令数不一致，程序会停止并报告错误。

## 5. HG3 图像工具

程序：`catsystem2-hg3.exe`

### 5.1 命令格式

```text
catsystem2-hg3 extract [HG3输入目录] [工作目录] [--metadata-only]
catsystem2-hg3 pack    [工作目录]    [HG3输出目录]
```

省略目录时使用以下默认值：

| 命令 | 默认输入 | 默认输出 |
| --- | --- | --- |
| `extract` | 当前目录 | `hg3-work` |
| `pack` | `hg3-work` | 当前目录 |

### 5.2 提取 HG3

准备输入目录：

```text
hg3-input/
├── BGA01.hg3
└── character.hg3
```

执行：

```powershell
./build/release/catsystem2-hg3.exe extract hg3-input hg3-work
```

每个 HG3 文件会生成一个同名子目录：

```text
hg3-work/
├── BGA01/
│   ├── BGA01#0000.png
│   └── BGA01#0000.hg3meta
└── character/
    ├── character#0000.png
    ├── character#0000.hg3meta
    ├── character#0001.png
    └── character#0001.hg3meta
```

编号从 `#0000` 开始。一个 HG3 中包含多个图像块时，会依次生成 `#0001`、`#0002` 等文件。

### 5.3 只提取元数据

不需要图片时可以使用：

```powershell
./build/release/catsystem2-hg3.exe extract hg3-input hg3-work --metadata-only
```

此模式只生成 `.hg3meta`，不会执行图像解码，也不会生成 PNG。仅有元数据的目录不能直接重新打包，重新打包仍需要对应 PNG。

### 5.4 支持的 HG3 图像段

| HG3 图像段 | 提取行为 |
| --- | --- |
| `img0000` | 解压并转换为 PNG |
| `img_jpg` 和 `img_al` | 解码 JPEG，并使用 `img_al` 作为 Alpha 通道 |
| 只有 `img_jpg` | 解码 JPEG，所有像素 Alpha 设置为 255 |

外部图片目前只支持 PNG：

- 提取结果固定为 `.png`。
- 重新打包只读取 `.png`。
- 不直接输出 JPG、BMP、WebP 等格式。
- PNG 使用 BGRA 32 位像素格式，可以保留透明通道。

### 5.5 编辑 HG3 图片

编辑 PNG 时应遵守以下规则：

- PNG 和 `.hg3meta` 必须保持同名。
- 不要删除或修改 `.hg3meta`。
- 可以修改 PNG 的宽度和高度，重新打包时会更新 `stdinfo` 中的尺寸。
- 推荐保留 32 位 RGBA/BGRA PNG，避免丢失透明通道。
- 不要把 JPG 文件直接改扩展名为 PNG。
- 每个子目录对应一个 HG3 输出文件，子目录名会成为 HG3 文件名。

### 5.6 重新打包 HG3

执行：

```powershell
./build/release/catsystem2-hg3.exe pack hg3-work rebuilt-hg3
```

输入：

```text
hg3-work/BGA01/BGA01#0000.png
hg3-work/BGA01/BGA01#0000.hg3meta
```

输出：

```text
rebuilt-hg3/BGA01.hg3
```

重新打包时，所有图片统一写为无损 `img0000` 图像段。原始 HG3 即使使用 JPEG，重新打包后也不会再次执行 JPEG 有损压缩。

## 6. FES 工具

程序：`catsystem2-fes.exe`，文件签名为 `FES\0`。

### 6.1 命令格式

```text
catsystem2-fes pack   [普通文件输入目录] [FES输出目录]
catsystem2-fes unpack [FES输入目录]      [普通文件输出目录]
```

默认目录：

| 命令 | 默认输入 | 默认输出 |
| --- | --- | --- |
| `pack` | `fes-input` | 当前目录 |
| `unpack` | 当前目录 | `fes-output` |

### 6.2 打包 FES

```powershell
./build/release/catsystem2-fes.exe pack fes-input fes-files
```

文件名变化：

```text
fes-input/script.bin
fes-files/script.bin.fes
```

输入目录中的每个普通文件都会单独生成一个 FES 文件，原扩展名会保留在 `.fes` 之前。

### 6.3 解包 FES

```powershell
./build/release/catsystem2-fes.exe unpack fes-files fes-output
```

文件名恢复：

```text
fes-files/script.bin.fes
fes-output/script.bin
```

解包只处理 `.fes` 文件，并验证 `FES\0` 签名、压缩大小、解压大小和 zlib 数据。

## 7. KCS 工具

程序：`catsystem2-kcs.exe`，文件签名为 `KCS\0`。

原项目中的格式记录和目录名称为 KCS，因此保留此工具。

### 7.1 打包 KCS

```powershell
./build/release/catsystem2-kcs.exe pack kcs-input kcs-files
```

```text
kcs-input/data.bin
kcs-files/data.bin.kcs
```

### 7.2 解包 KCS

```powershell
./build/release/catsystem2-kcs.exe unpack kcs-files kcs-output
```

```text
kcs-files/data.bin.kcs
kcs-output/data.bin
```

省略参数时，`pack` 默认读取 `kcs-input` 并输出到当前目录；`unpack` 默认读取当前目录并输出到 `kcs-output`。

## 8. KSC 工具

程序：`catsystem2-ksc.exe`，文件签名为 `KSC\0`。

KSC 与 KCS 是两个签名和扩展名不同的容器，不能混用对应工具。

### 8.1 打包 KSC

```powershell
./build/release/catsystem2-ksc.exe pack ksc-input ksc-files
```

```text
ksc-input/data.bin
ksc-files/data.bin.ksc
```

### 8.2 解包 KSC

```powershell
./build/release/catsystem2-ksc.exe unpack ksc-files ksc-output
```

```text
ksc-files/data.bin.ksc
ksc-output/data.bin
```

省略参数时，`pack` 默认读取 `ksc-input` 并输出到当前目录；`unpack` 默认读取当前目录并输出到 `ksc-output`。

## 9. 常见错误

### `directory does not exist`

输入目录不存在。检查命令中的目录拼写，或者先创建对应目录。

### `no .cst files found`

CST 输入目录中没有 `.cst` 文件。工具不会递归搜索子目录。

### `missing CST metadata file`

重新打包 CST 时，某个 `.txt` 找不到对应的 `.cstmeta`。恢复同名元数据文件。

### `CST text file has fewer lines` 或 `more lines`

翻译文本的行数发生变化。恢复被删除的行，或者删除额外增加的行。

### `CST text file must end every translated command with a newline`

文本最后一行没有换行。使用能够保留文件末尾换行的编辑器重新保存。

### `missing HG3 PNG image`

重新打包 HG3 时，`.hg3meta` 找不到同名 PNG。检查文件名、扩展名和编号。

### `HG3 block has no supported image segment`

该块既没有受支持的 `img0000`，也没有可解码的 `img_jpg`。

### `archive signature does not match its tool`

使用了错误的容器工具。例如不能用 KSC 工具解包 KCS 文件，也不能只修改文件扩展名来转换格式。

### `zlib decompression failed`

文件已损坏、压缩数据不完整，或者并非受支持的 CatSystem2 格式。应重新获取原始文件，不要继续重新打包该文件。
