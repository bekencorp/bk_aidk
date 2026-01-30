# Unisound ASR 配置指南

本文档说明如何在工程中配置 Unisound ASR 功能，包括所需的配置修改步骤。

## 概述

`beken_genie_unisound` 工程已经集成了 Unisound ASR（自动语音识别）功能，并已完成相关配置。本文档详细说明了 `beken_genie_unisound` 工程中 Unisound ASR 的配置情况。

**如果您需要在其他工程上使用 Unisound ASR 功能，可以参考 `beken_genie_unisound` 工程的这些配置步骤进行相应的修改。**

## 配置步骤

以下配置步骤基于 `beken_genie_unisound` 工程。如果要在其他工程上使用 Unisound ASR，请参考这些步骤进行相应的配置修改。

### 1. 配置项目根目录 CMakeLists.txt

**文件路径**: `projects/beken_genie_unisound/CMakeLists.txt`

**配置说明**: 该文件主要用于配置组件路径。在 `beken_genie_unisound` 工程中，Unisound 相关组件的路径已经配置完成。

**如果要在其他工程上使用，需要在 `CMakeLists.txt` 中添加以下组件路径配置**：
- `$ENV{ARMINO_PATH}/../components/bk_thirdparty/unisound`
- `$ENV{ARMINO_PATH}/../../components/bk_unisound_asr`

### 2. 配置 main/CMakeLists.txt

**文件路径**: `projects/beken_genie_unisound/main/CMakeLists.txt`

**配置说明**: 在 `beken_genie_unisound` 工程中，`PRIV_REQUIRES` 已包含 `unisound` 和 `bk_unisound_asr` 组件依赖。

**如果要在其他工程上使用，需要在 `PRIV_REQUIRES` 中添加 `unisound` 和 `bk_unisound_asr` 组件依赖**：

```cmake
# 第 20-24 行，在 PRIV_REQUIRES 中添加 unisound 和 bk_unisound_asr
armino_component_register(SRCS "${srcs}" 
    INCLUDE_DIRS "${incs}" 
    PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia avdk_utils mbedtls json wanson 
    bk_wanson_asr bk_factory_config bk_nfc bk_app_event bk_countdown bk_led_blink bk_motor bk_boarding_service 
    bk_smart_config bk_bt bk_key_app asr audio_engine video_engine network_transfer unisound bk_unisound_asr
)
```

### 3. 配置 config/bk7258/config

**文件路径**: `projects/beken_genie_unisound/config/bk7258/config`

**配置说明**: 在 `beken_genie_unisound` 工程中，已添加 Unisound License 配置项和 NTP 时间同步配置。

**如果要在其他工程上使用，需要在 CPU0 配置文件中添加以下配置**：
```
CONFIG_UNISOUND_LICENSE=y
CONFIG_NTP_SYNC_RTC=y
```

**说明**:
- `CONFIG_UNISOUND_LICENSE=y`: 启用 Unisound License 管理功能
- `CONFIG_NTP_SYNC_RTC=y`: 启用 NTP 时间同步到 RTC，**必须配置**。由于 Unisound 的联网授权需要获取时间，在 CPU0 上必须启用此配置以确保系统时间正确

**注意**: 该配置文件还可能需要调整 CPU 的 RAM 大小配置，具体取决于您的硬件配置需求。

### 4. 配置 config/bk7258_cp2/config

**文件路径**: `projects/beken_genie_unisound/config/bk7258_cp2/config`

**配置说明**: 在 `beken_genie_unisound` 工程中，已添加 Unisound ASR 相关配置，并已禁用 Wanson ASR。

**如果要在其他工程上使用，需要在 CPU2 配置文件中添加以下内容**：

```
#@ Enable Unisound Automatic Speech Recognition
CONFIG_ASR_ENGINE_UNISOUND=y
CONFIG_UNISOUND=y
CONFIG_BEKEN_UNISOUND_ASR=y
```

同时，需要禁用 Wanson ASR 相关配置（如果之前已启用）：
```
CONFIG_ASR_ENGINE_WANSON=n
CONFIG_WANSON_ASR=n
CONFIG_BEKEN_WANSON_ASR=n
```

**重要配置**（必须添加）：

1. OTP 配置：
```
CONFIG_OTP=y
CONFIG_OTP_V1=y
```

2. Flash 和 IPC 配置：
```
CONFIG_MAILBOX_IPC=y
CONFIG_FLASH=y
CONFIG_FLASH_MB=n
CONFIG_FLASH_TEST=y
CONFIG_OVERRIDE_FLASH_PARTITION=y
```

**其他配置调整**:
- 调整 CPU 的 RAM 大小配置
- **可选配置** - 如果需要查看 CPU2 的日志，可以配置 UART：
```
CONFIG_UART2=y
CONFIG_UART_PRINT_PORT=1
```
  **注意**: UART 的具体使用需要根据您的硬件进行实际配置，以上配置仅供参考。请根据您的硬件连接情况调整 UART 端口和打印端口配置。

### 5. 配置 config/bk7258/partitions.csv

**文件路径**: `projects/beken_genie_unisound/config/bk7258/partitions.csv`

**配置说明**: 在 `beken_genie_unisound` 工程中，已添加 unisound_config 分区，并已调整其他分区大小。

**如果要在其他工程上使用，需要在 `usr_config` 分区之后添加以下分区**：

```
unisound_config,0x7f9000,4K,,TRUE
```

**注意**: 
- 添加该分区后，需要调整其他分区的大小和偏移地址，确保分区不重叠。例如：
  - `primary_cpu2_app` 分区大小可能需要从 476K 调整为 1156K
  - `ota` 分区大小可能需要从 3196K 调整为 2516K
  - `usr_config` 分区大小可能需要从 64K 调整为 60K
- **重要**: 修改 CSV 文件后，`main/vendor_flash_partition.h` 和 `main/vendor_flash.c` 文件会根据 CSV 配置自动生成，无需手动修改。

### 6. 配置 config/bk7258/bk7258_partitions.csv

**文件路径**: `projects/beken_genie_unisound/config/bk7258/bk7258_partitions.csv`

**配置说明**: 在 `beken_genie_unisound` 工程中，已添加 unisound_config 分区。

**如果要在其他工程上使用，需要在 `usr_config` 分区之后添加以下分区**：

```
unisound_config,0x7f9000,4K,data,TRUE,
```

同样需要调整其他分区的大小和偏移地址，与 `partitions.csv` 保持一致。

**注意**: 修改此 CSV 文件后，相关的头文件和源文件会自动生成，无需手动修改。

### 7. 配置 main/app_main.c

**文件路径**: `projects/beken_genie_unisound/main/app_main.c`

**配置说明**: 在 `beken_genie_unisound` 工程中，已包含 Unisound License 初始化代码。

```c
#if (CONFIG_UNISOUND_LICENSE)
    extern bk_err_t user_key_task_init(void);
    user_key_task_init();
#endif
```

**注意**: 在 `beken_genie_unisound` 工程中已包含上述代码。如果要在其他工程上使用，请添加上述代码。

## 配置说明

### 分区布局

启用 Unisound ASR 后，Flash 分区布局需要包含以下配置：

- **unisound_config**: 位于 `0x7f9000`，大小为 4KB，用于存储 Unisound 配置信息
- 其他分区（如 `primary_cpu2_app`、`ota`、`usr_config`）的大小和偏移地址需要相应调整

### 内存配置

具体配置请根据实际硬件资源和应用需求进行调整。

### 组件依赖

启用 Unisound ASR 需要以下组件支持：

- `unisound`: Unisound SDK 组件（位于 `bk_avdk/components/bk_thirdparty/unisound/`）
- `bk_unisound_asr`: Beken Unisound ASR 封装组件

这些组件需要在项目根目录的 `CMakeLists.txt` 中配置路径。在 `beken_genie_unisound` 工程中，这些组件已经配置完成。

### 唤醒关键字

**重要说明**: 
- 唤醒关键字、识别分数阈值调整或算法库优化，需要联系 Unisound 进行修改
- 这些修改涉及算法库层面的调整，无法通过配置文件直接修改

## 验证步骤

完成以上配置后，请进行以下验证：

1. **编译验证**: 确保项目能够正常编译，无编译错误
2. **分区验证**: 检查 Flash 分区表是否正确，确保分区不重叠
3. **功能验证**: 测试 Unisound ASR 功能是否正常工作

## 注意事项

1. **分区大小**: 添加 `unisound_config` 分区后，需要确保 Flash 空间足够，可能需要调整其他分区的大小。在 `beken_genie_unisound` 工程中，这些配置已经完成
2. **配置一致性**: 确保 `partitions.csv` 和 `bk7258_partitions.csv` 中的分区配置保持一致
3. **License 配置**: `CONFIG_UNISOUND_LICENSE=y` 用于启用 Unisound License 管理功能，请确保已正确配置相关 License 信息
4. **NTP 时间同步**: **必须**在 CPU0 配置文件中启用 `CONFIG_NTP_SYNC_RTC=y`，因为 Unisound 的联网授权需要获取准确的系统时间。如果未启用此配置，可能导致授权失败
5. **OTP 配置**: **必须**在 CPU2 配置文件中启用 `CONFIG_OTP=y` 和 `CONFIG_OTP_V1=y`，如果未启用此配置，可能导致初始化失败
6. **Flash 和 IPC 配置**: **必须**在 CPU2 配置文件中启用 Flash 和 Mailbox IPC 相关配置（`CONFIG_MAILBOX_IPC=y`、`CONFIG_FLASH=y`、`CONFIG_FLASH_MB=n`、`CONFIG_FLASH_TEST=y`、`CONFIG_OVERRIDE_FLASH_PARTITION=y`）

## 故障排查

如果遇到问题，请检查：

1. 组件路径是否正确配置在 `CMakeLists.txt` 中
2. 分区表配置是否正确，分区是否重叠
3. 内存配置是否满足应用需求
4. License 配置是否正确
5. **NTP 时间同步是否已启用**: 检查 `config/bk7258/config` 中是否已配置 `CONFIG_NTP_SYNC_RTC=y`，如果 Unisound 授权失败，很可能是时间同步未启用导致的
6. **OTP 配置是否已启用**: 检查 `config/bk7258_cp2/config` 中是否已配置 `CONFIG_OTP=y` 和 `CONFIG_OTP_V1=y`，如果 Unisound 初始化失败，很可能是 OTP 配置未启用导致的
7. **Flash 和 IPC 配置是否已启用**: 检查 `config/bk7258_cp2/config` 中是否已配置 `CONFIG_MAILBOX_IPC=y`、`CONFIG_FLASH=y`、`CONFIG_FLASH_MB=n`、`CONFIG_FLASH_TEST=y`、`CONFIG_OVERRIDE_FLASH_PARTITION=y`，如果授权码读取失败，很可能是这些配置未启用导致的

## 相关文件

- `components/bk_unisound_asr/`: Unisound ASR 组件源码
- `bk_avdk/components/bk_thirdparty/unisound/`: Unisound SDK 源码和库文件


