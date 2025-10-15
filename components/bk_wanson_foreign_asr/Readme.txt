# Wanson Foreign ASR模型及外语版授权功能使用说明

本文档介绍如何在工程中配置Wanson Foreign ASR模型及启用外语版授权功能（以beken_genie工程为例）。

## 一、配置修改步骤

### 1. CPU0配置修改
在CPU0的config文件中添加以下宏定义以启用外语版授权功能：
```
CONFIG_WANSON_FL_LICENSE=y
```

### 2. CPU2配置修改
在CPU2的config文件中进行以下配置：
- 关闭中文Wanson ASR相关宏配置：
  ```
  CONFIG_WANSON_ASR=n
  CONFIG_BEKEN_WANSON_ASR=n
  CONFIG_WANSON_ASR_GROUP_VERSION_WORDS_V1=n
  ```
- 开启外语Wanson ASR相关宏配置：
  ```
  CONFIG_WANSON_FOREIGN_ASR=y
  CONFIG_OTP=y
  CONFIG_OTP_V1=y
  ```

### 3. 存储配置调整
由于外语模型的加入，需根据实际情况调整工程配置文件中的Flash及PSRAM分配：
- **Flash调整**：
  CPU2上需要根据实际使用的Wanson ASR外语模型大小（即`bk_thirdparty\asr\wanson_foreign\model\english_air.bin`和`japanese_air.bin`）增加Flash分配
- **PSRAM调整**：
  CPU2上的PSRAM空间需要分配0x100000（1MB）

> **注意**：Flash及PSRAM调整的具体细节可参考AIDK官方文档。