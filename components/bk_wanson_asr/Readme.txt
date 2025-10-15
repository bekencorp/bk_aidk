# Wanson ASR模型及中文版授权功能使用说明

## 概述
本文档详细介绍如何在工程中配置Wanson ASR（自动语音识别）模型及启用中文版授权功能，以beken_genie工程为例进行说明。

## 一、Wanson ASR模型简介
Wanson ASR模型是一款高性能的语音识别模型，支持中文语音识别功能。通过启用中文版授权功能，可以解锁模型的完整中文语音识别能力。

## 二、配置修改步骤

### 1. CPU0配置修改
在CPU0的config文件中添加以下宏定义以启用中文版授权功能：
```
CONFIG_WANSON_CN_LICENSE=y
```

### 2. CPU2配置修改
在CPU2的config文件中进行以下配置：
- 开启中文Wanson ASR相关宏配置：
  ```
  CONFIG_WANSON_CN_LICENSE=y
  CONFIG_OTP=y
  CONFIG_OTP_V1=y
  ```

### 3. 存储配置调整
- **Flash调整**：
  CPU2上需要根据实际使用的Wanson ASR .a库大小（即`bk_thirdparty\asr\wanson\bk7258\libasrfst_with_auth.a`）调整Flash分配，确保有足够空间存放模型文件
- **PSRAM调整**：
  CPU2上的PSRAM空间至少需要分配0x10000用于模型运行时的数据缓存

> **注意**：Flash及PSRAM调整的具体细节可参考AIDK官方文档。
