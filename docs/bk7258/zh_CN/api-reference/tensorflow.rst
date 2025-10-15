TensorFlow Lite Micro开发指南
===========================================

:link_to_translation:`en:[English]`

1. 概述
---------------------------------

TensorFlow Lite Micro (TFLM) 是专为微控制器和其他资源受限设备设计的轻量级机器学习推理框架。它是TensorFlow Lite的精简版本，专门针对嵌入式系统进行了优化。

本SDK集成了TensorFlow Lite Micro框架，并提供了完整的示例工程，帮助开发者快速上手边缘AI应用开发。

1.1 主要特性
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* **轻量级设计**：针对嵌入式设备优化，内存占用小
* **硬件加速**：支持CMSIS-NN硬件加速库，提升推理性能
* **易于集成**：提供完整的CMake构建系统和示例工程
* **丰富的算子支持**：支持常用的深度学习算子
* **双核架构支持**：充分利用BK7258多核处理能力

1.2 系统要求
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* 芯片平台：BK7258（Cortex-M33双核）
* 内存：PSRAM用于存储模型和tensor arena

1.3 系统结构
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

由于CPU0最高只有240Mhz主频，CPU1可以运行在480Mhz主频。

因此，Tensor Flow Lite Micro的模型推理运算，运行在CPU1上。


2. 组件架构
---------------------------------

2.1 目录结构
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

TensorFlow Lite Micro相关代码主要包含两个部分：

**组件目录** (``components/bk_tflite_micro/``)::

    bk_tflite_micro/
    ├── CMakeLists.txt              # 组件构建文件
    ├── Kconfig                     # 组件配置选项
    └── tflite-micro/               # TensorFlow Lite Micro源码
        └── tensorflow/
            └── lite/
                └── micro/          # 核心实现代码

**示例工程目录** (``projects/tflite_micro/``)::

    tflite_micro/
    ├── micro_speech/               # 语音识别示例
    │   ├── main/                   # 主程序代码
    │   │   ├── app_main_cpu1.cc   # CPU1主函数
    │   │   ├── tflite/            # TensorFlow Lite相关代码
    │   │   │   ├── main_functions.cc     # 推理主逻辑
    │   │   │   ├── micro_speech_quantized_model_data.cc  # 模型数据
    │   │   │   └── ...
    │   │   └── CMakeLists.txt     # 主程序构建文件
    │   ├── config/                 # 芯片配置文件
    │   ├── CMakeLists.txt         # 项目构建文件
    │   └── Makefile               # 顶层Makefile
    └── gesture_detection/          # 手势检测示例
        ├── main/                   # 主程序代码
        │   ├── app_main_cpu1.cc   # CPU1主函数
        │   ├── tflite/            # TensorFlow Lite相关代码
        │   │   ├── main_functions.cc         # 推理主逻辑
        │   │   ├── gesture_detection_model_data.cc  # 模型数据
        │   │   ├── image_provider.cc         # 图像输入接口
        │   │   └── detection_responder.cc    # 结果处理接口
        │   └── CMakeLists.txt     # 主程序构建文件
        └── config/                 # 芯片配置文件

2.2 组件配置
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

TensorFlow Lite Micro组件通过Kconfig进行配置，需要在项目配置中启用：

.. code-block:: kconfig

    menu "TFLite Micro"
        config TFLITE_MICRO
            bool "Enable TFLite Micro"
            default n
    endmenu

使能后，组件会自动编译TensorFlow Lite Micro静态库，并链接到应用程序。



3. 示例工程详解
---------------------------------

3.1 Micro Speech（语音识别）
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

Micro Speech是一个基于音频特征的关键词识别示例，可以识别"yes"、"no"等简单语音命令。

**3.1.1 核心组件**

* **音频预处理模型**：将原始音频数据转换为特征向量
* **语音识别模型**：对特征向量进行分类，识别关键词
* **测试音频数据**：包含yes、no、silence、noise等测试样本

**3.1.2 主要流程**

1. **初始化阶段**：

   .. code-block:: cpp

       // CPU1上运行AI推理任务
       void app_main_cpu1(void *arg) {
           // 设置CPU频率为480MHz以获得更好性能
           bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
           
           // 创建TensorFlow Lite推理任务
           xTaskCreate(tflite_task, "test", 1024*16, NULL, 3, NULL);
       }

2. **模型加载**：

   .. code-block:: cpp

       void tflite_task(void *arg) {
           // 注册调试日志回调
           RegisterDebugLogCallback(debugLogCallback);
           
           // 从PSRAM分配模型数据缓冲区
           data_ptr = (unsigned char*)psram_malloc(
               g_micro_speech_quantized_model_data_len);
           
           // 拷贝模型数据到PSRAM
           os_memcpy(data_ptr, g_micro_speech_quantized_model_data, 
               g_micro_speech_quantized_model_data_len);
           
           // 循环执行推理
           while (1) {
               loop();
               vTaskDelay(pdMS_TO_TICKS(5000));
           }
       }

3. **特征提取**：

   .. code-block:: cpp

       // 使用音频预处理模型生成特征
       TfLiteStatus GenerateFeatures(const int16_t* audio_data,
                                     const size_t audio_data_size,
                                     Features* features_output) {
           // 加载预处理模型
           const tflite::Model* model = 
               tflite::GetModel(g_audio_preprocessor_int8_model_data);
           
           // 创建算子解析器
           AudioPreprocessorOpResolver op_resolver;
           RegisterOps(op_resolver);
           
           // 创建解释器
           tflite::MicroInterpreter interpreter(model, op_resolver, 
                                               g_arena, kArenaSize);
           
           // 分配tensor内存
           interpreter.AllocateTensors();
           
           // 处理音频数据生成特征
           // ...
       }

4. **推理执行**：

   .. code-block:: cpp

       // 加载语音识别模型并执行推理
       TfLiteStatus LoadMicroSpeechModelAndPerformInference(
           const Features& features, const char* expected_label) {
           // 加载模型
           const tflite::Model* model = 
               tflite::GetModel(g_micro_speech_quantized_model_data);
           
           // 创建算子解析器
           MicroSpeechOpResolver op_resolver;
           op_resolver.AddReshape();
           op_resolver.AddFullyConnected();
           op_resolver.AddDepthwiseConv2D();
           op_resolver.AddSoftmax();
           
           // 创建解释器并分配内存
           tflite::MicroInterpreter interpreter(model, op_resolver, 
                                               g_arena, kArenaSize);
           interpreter.AllocateTensors();
           
           // 填充输入数据
           TfLiteTensor* input = interpreter.input(0);
           std::copy_n(&features[0][0], kFeatureElementCount,
                      tflite::GetTensorData<int8_t>(input));
           
           // 执行推理
           interpreter.Invoke();
           
           // 获取输出结果
           TfLiteTensor* output = interpreter.output(0);
           // 反量化并解析结果
           // ...
       }

**3.1.3 内存配置**

* **Arena Size**：28584字节（用于存储tensor数据）
* **任务栈大小**：1024*16字节
* **模型存储**：使用PSRAM存储模型数据

3.2 Gesture Detection（手势检测）
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

Gesture Detection是一个基于视觉的手势识别示例，可以检测石头、剪刀、布等手势。

**3.2.1 核心组件**

* **图像输入接口**：``image_provider.cc`` - 负责获取摄像头图像
* **手势检测模型**：``gesture_detection_model_data.cc`` - 包含训练好的模型
* **结果处理接口**：``detection_responder.cc`` - 处理检测结果并输出
* **模型配置**：``model_settings.cc/h`` - 定义模型输入输出参数

**3.2.2 主要流程**

1. **初始化阶段**：

   .. code-block:: cpp

       void setup() {
           tflite::InitializeTarget();
           
           // 加载模型
           model = tflite::GetModel(data_ptr);
           
           // 创建算子解析器（13个算子）
           static tflite::MicroMutableOpResolver<13> micro_op_resolver;
           micro_op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
           micro_op_resolver.AddPad();
           micro_op_resolver.AddMaxPool2D();
           // ... 添加更多算子
           
           // 从PSRAM分配tensor arena（360KB）
           uint8_t *tensor_arena = (uint8_t *)psram_malloc(kTensorArenaSize);
           
           // 创建解释器
           static tflite::MicroInterpreter static_interpreter(
               model, micro_op_resolver, tensor_arena, kTensorArenaSize);
           interpreter = &static_interpreter;
           
           // 分配tensor
           interpreter->AllocateTensors();
           
           // 获取输入tensor
           input = interpreter->input(0);
       }

2. **图像获取与推理**：

   .. code-block:: cpp

       void loop() {
           // 获取图像数据
           if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, 
                                     input->data.int8, 0)) {
               MicroPrintf("Image capture failed.\r\n");
           }
           
           // 执行推理
           if (kTfLiteOk != interpreter->Invoke()) {
               MicroPrintf("Invoke failed.\r\n");
           }
           
           // 获取输出并后处理
           TfLiteTensor* output = interpreter->output(0);
           g_scale = output->params.scale;
           g_zero_point = output->params.zero_point;
           
           post_process(output->data.int8);
       }

3. **结果后处理**：

   .. code-block:: cpp

       uint8_t post_process(int8_t *out_data) {
           // 遍历所有检测框（2268个anchor）
           for(int i = 0; i < 2268; i++) {
               // 反量化得到置信度分数
               float score = (out_data[i*8 + 4] - g_zero_point) * g_scale;
               
               if(score > 62) {  // 置信度阈值
                   // 解析边界框坐标
                   int x = (out_data[i*8 + 0] - g_zero_point) * g_scale;
                   int y = (out_data[i*8 + 1] - g_zero_point) * g_scale;
                   int w = (out_data[i*8 + 2] - g_zero_point) * g_scale;
                   int h = (out_data[i*8 + 3] - g_zero_point) * g_scale;
                   
                   // 解析手势类别
                   float paper = (out_data[i*8 + 5] - g_zero_point) * g_scale;
                   float rock = (out_data[i*8 + 6] - g_zero_point) * g_scale;
                   float scissors = (out_data[i*8 + 7] - g_zero_point) * g_scale;
                   
                   // 判断手势类型并输出
                   if (paper > 90) {
                       MicroPrintf("Paper is detected\r\n");
                   } else if (rock > 90) {
                       MicroPrintf("Rock is detected\r\n");
                   } else if (scissors > 90) {
                       MicroPrintf("Scissors is detected\r\n");
                   }
               }
           }
       }

**3.2.3 内存配置**

* **Tensor Arena Size**：360KB（从PSRAM分配）
* **模型输入**：192x192x3 INT8图像
* **模型输出**：2268x8 INT8检测结果（坐标、置信度、类别）

4. 开发指南
---------------------------------

4.1 环境准备
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**4.1.1 安装开发工具**

1. 安装ARM GCC工具链
2. 安装CMake（版本≥3.5）
3. 配置ARMINO_PATH环境变量

**4.1.2 启用TensorFlow Lite Micro组件**

在项目配置文件中启用TFLITE_MICRO：

.. code-block:: bash

    # 使用menuconfig配置
    make menuconfig
    # 导航到 TFLite Micro -> Enable TFLite Micro，选择[Y]

或在``sdkconfig``文件中添加：

.. code-block:: text

    CONFIG_TFLITE_MICRO=y

4.2 创建自定义AI应用
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**4.2.1 准备模型文件**

1. **训练模型**：使用TensorFlow/Keras训练模型
2. **量化模型**：转换为INT8量化的TFLite模型
3. **转换为C数组**：使用xxd工具转换为头文件

.. code-block:: bash

    # 将.tflite文件转换为C数组
    xxd -i model.tflite > model_data.cc

**4.2.2 集成模型到工程**

1. 创建模型数据文件：

   .. code-block:: cpp

       // model_data.h
       #ifndef MODEL_DATA_H_
       #define MODEL_DATA_H_
       
       extern const unsigned char g_model_data[];
       extern const int g_model_data_len;
       
       #endif  // MODEL_DATA_H_

   .. code-block:: cpp

       // model_data.cc
       #include "model_data.h"
       
       alignas(8) const unsigned char g_model_data[] = {
           // 模型数据...
       };
       const int g_model_data_len = sizeof(g_model_data);

2. 创建主推理代码：

   .. code-block:: cpp

       #include "tensorflow/lite/micro/micro_interpreter.h"
       #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
       #include "tensorflow/lite/schema/schema_generated.h"
       #include "model_data.h"
       
       // 定义tensor arena大小
       constexpr int kTensorArenaSize = 100 * 1024;
       uint8_t *tensor_arena = nullptr;
       
       void setup() {
           // 加载模型
           const tflite::Model* model = tflite::GetModel(g_model_data);
           
           // 添加所需算子
           static tflite::MicroMutableOpResolver<5> micro_op_resolver;
           micro_op_resolver.AddConv2D();
           micro_op_resolver.AddFullyConnected();
           micro_op_resolver.AddSoftmax();
           // 添加其他需要的算子...
           
           // 分配内存
           tensor_arena = (uint8_t *)psram_malloc(kTensorArenaSize);
           
           // 创建解释器
           static tflite::MicroInterpreter static_interpreter(
               model, micro_op_resolver, tensor_arena, kTensorArenaSize);
           
           static_interpreter.AllocateTensors();
           
           // 获取输入输出tensor
           TfLiteTensor* input = static_interpreter.input(0);
           TfLiteTensor* output = static_interpreter.output(0);
       }
       
       void loop() {
           // 填充输入数据
           // ...
           
           // 执行推理
           interpreter->Invoke();
           
           // 处理输出结果
           // ...
       }

**4.2.3 配置CMakeLists.txt**

在项目的``main/CMakeLists.txt``中添加TensorFlow Lite相关源文件：

.. code-block:: cmake

    if (CONFIG_SYS_CPU1)
        file(GLOB_RECURSE TF_SOURCES tflite/*.c tflite/*.cc)
        
        list(APPEND srcs
            app_cpu1_main.c
            app_main_cpu1.cc
            ${TF_SOURCES}
        )
        
        list(APPEND incs
            tflite
        )
    endif()
    
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "${incs}"
    )

**4.2.4 添加必要的编译选项**

在项目的顶层``CMakeLists.txt``中添加C++编译选项：

.. code-block:: cmake

    # 禁用某些警告
    armino_build_set_property(COMPILE_OPTIONS "-Wno-unused-variable" APPEND)
    armino_build_set_property(COMPILE_OPTIONS "-Wno-sign-compare" APPEND)
    armino_build_set_property(CXX_COMPILE_OPTIONS "-fpermissive" APPEND)

4.3 双核应用开发
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

BK7258支持双核架构，可以将AI推理任务运行在CPU1上，充分利用硬件资源。

**4.3.1 CPU配置**

在配置中启用双核模式：

.. code-block:: text

    CONFIG_SYS_CPU0=y    # CPU0配置
    CONFIG_SYS_CPU1=y    # CPU1配置

**4.3.2 任务分配**

* **CPU0**：运行系统管理、通信等任务
* **CPU1**：运行AI推理任务

示例代码：

.. code-block:: cpp

    // CPU1入口函数
    extern "C" void app_main_cpu1(void *arg) {
        // 提升CPU频率以获得更好性能
        bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
        
        // 创建AI推理任务
        xTaskCreate(tflite_task, "tflite", 1024*16, NULL, 3, NULL);
    }

4.4 性能优化建议
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**4.4.1 内存优化**

1. **使用PSRAM**：将模型数据和tensor arena放在PSRAM中

   .. code-block:: cpp

       // 从PSRAM分配模型缓冲区
       data_ptr = (unsigned char*)psram_malloc(model_data_len);
       
       // 从PSRAM分配tensor arena
       tensor_arena = (uint8_t *)psram_malloc(kTensorArenaSize);

2. **优化Arena大小**：使用``interpreter->arena_used_bytes()``获取实际使用量，调整arena大小

   .. code-block:: cpp

       MicroPrintf("Arena used: %d bytes\n", 
                   interpreter->arena_used_bytes());

3. **静态内存分配**：定义``TF_LITE_STATIC_MEMORY``避免动态分配

**4.4.2 推理优化**

1. **INT8量化**：使用INT8量化模型减少内存占用和计算量
2. **CMSIS-NN加速**：组件默认启用CMSIS-NN优化内核
3. **CPU频率调节**：根据性能需求调整CPU频率

   .. code-block:: cpp

       // 设置为最高频率480MHz
       bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);

4. **任务优先级**：为AI推理任务设置适当优先级

   .. code-block:: cpp

       xTaskCreate(tflite_task, "tflite", stack_size, NULL, 
                   priority, NULL);

**4.4.3 模型优化**

1. **简化网络结构**：减少层数和参数量
2. **剪枝与压缩**：使用模型剪枝技术减少模型大小
3. **算子选择**：优先使用CMSIS-NN支持的算子（Conv2D、DepthwiseConv2D、FullyConnected等）

5. 模型适配与集成
---------------------------------

5.1 模型转换流程
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.1.1 BK7258芯片说明**

.. important::
   **BK7258芯片采用ARM Cortex-M33 CPU架构，不包含NPU（神经网络处理器）硬件加速单元。**
   
   因此：
   
   * **无需进行NPU模型转换**
   * 所有推理运算在CPU上执行
   * 使用CMSIS-NN软件库进行算子优化
   * 模型只需转换为TensorFlow Lite格式即可

**5.1.2 模型转换步骤**

将训练好的TensorFlow/Keras模型转换为TFLite INT8量化模型：

.. code-block:: python

    import tensorflow as tf
    import numpy as np
    
    # 1. 加载训练好的模型
    model = tf.keras.models.load_model('your_model.h5')
    
    # 2. 创建TFLite转换器
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    
    # 3. 启用INT8量化优化
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    # 4. 提供代表性数据集用于量化校准
    def representative_dataset():
        # 使用训练集或验证集的部分数据
        for i in range(100):
            # 确保数据形状匹配模型输入
            data = np.random.rand(1, input_height, input_width, channels)
            yield [data.astype(np.float32)]
    
    converter.representative_dataset = representative_dataset
    
    # 5. 设置输入输出类型为INT8
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    # 6. 转换并保存
    tflite_model = converter.convert()
    with open('model_int8.tflite', 'wb') as f:
        f.write(tflite_model)
    
    print("模型转换完成！")

**5.1.3 验证模型**

转换完成后，建议在PC上验证模型准确性：

.. code-block:: python

    import tensorflow as tf
    
    # 加载TFLite模型
    interpreter = tf.lite.Interpreter(model_path="model_int8.tflite")
    interpreter.allocate_tensors()
    
    # 获取输入输出详情
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    
    print("输入信息:", input_details)
    print("输出信息:", output_details)
    
    # 测试推理
    test_data = np.random.rand(*input_details[0]['shape']).astype(np.int8)
    interpreter.set_tensor(input_details[0]['index'], test_data)
    interpreter.invoke()
    output = interpreter.get_tensor(output_details[0]['index'])
    print("输出结果:", output)

5.2 添加模型到工程
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.2.1 转换为C数组**

使用 ``xxd`` 工具将 ``.tflite`` 文件转换为C/C++数组：

.. code-block:: bash

    # 方法1：使用xxd生成.cc文件
    xxd -i model_int8.tflite > model_data.cc
    
    # 方法2：使用xxd生成.h头文件
    xxd -i model_int8.tflite model_data.h

生成的文件格式类似：

.. code-block:: cpp

    unsigned char model_int8_tflite[] = {
      0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, ...
    };
    unsigned int model_int8_tflite_len = 123456;

**5.2.2 创建模型数据文件**

手动创建规范的模型文件：

.. code-block:: cpp

    // my_model_data.h
    #ifndef MY_MODEL_DATA_H_
    #define MY_MODEL_DATA_H_
    
    #include <stdint.h>
    
    // 模型数据声明
    extern const unsigned char g_my_model_data[];
    extern const int g_my_model_data_len;
    
    #endif  // MY_MODEL_DATA_H_

.. code-block:: cpp

    // my_model_data.cc
    #include "my_model_data.h"
    
    // 8字节对齐以优化访问性能
    alignas(8) const unsigned char g_my_model_data[] = {
        // 将xxd生成的数组数据粘贴到这里
        0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
        // ... 其他数据
    };
    
    const int g_my_model_data_len = sizeof(g_my_model_data);

**5.2.3 添加到CMakeLists.txt**

在项目的 ``main/CMakeLists.txt`` 中添加模型文件：

.. code-block:: cmake

    if (CONFIG_SYS_CPU1)
        list(APPEND srcs
            app_cpu1_main.c
            app_main_cpu1.cc
            tflite/my_model_data.cc      # 添加模型数据文件
            tflite/main_functions.cc      # 推理主逻辑
        )
        
        list(APPEND incs
            tflite
        )
    endif()

5.3 编写推理代码
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.3.1 基础推理框架**

创建 ``inference.cc`` 文件，实现完整的推理流程：

.. code-block:: cpp

    #include "tensorflow/lite/micro/micro_interpreter.h"
    #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
    #include "tensorflow/lite/schema/schema_generated.h"
    #include "my_model_data.h"
    
    extern "C" {
    #include "os/os.h"
    #include "os/mem.h"
    }
    
    // 全局变量
    namespace {
        const tflite::Model* model = nullptr;
        tflite::MicroInterpreter* interpreter = nullptr;
        TfLiteTensor* input = nullptr;
        TfLiteTensor* output = nullptr;
        
        // 根据模型需求调整arena大小
        constexpr int kTensorArenaSize = 200 * 1024;
        uint8_t* tensor_arena = nullptr;
    }
    
    // 初始化模型
    bool InitModel() {
        // 1. 加载模型
        model = tflite::GetModel(g_my_model_data);
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            os_printf("模型版本不匹配！\n");
            return false;
        }
        
        // 2. 注册需要的算子
        static tflite::MicroMutableOpResolver<6> micro_op_resolver;
        micro_op_resolver.AddConv2D();
        micro_op_resolver.AddDepthwiseConv2D();
        micro_op_resolver.AddFullyConnected();
        micro_op_resolver.AddSoftmax();
        micro_op_resolver.AddReshape();
        micro_op_resolver.AddMaxPool2D();
        // 根据实际模型添加其他算子
        
        // 3. 从PSRAM分配tensor arena
        tensor_arena = (uint8_t*)psram_malloc(kTensorArenaSize);
        if (tensor_arena == NULL) {
            os_printf("PSRAM分配失败！\n");
            return false;
        }
        
        // 4. 创建解释器
        static tflite::MicroInterpreter static_interpreter(
            model, micro_op_resolver, tensor_arena, kTensorArenaSize);
        interpreter = &static_interpreter;
        
        // 5. 分配tensors
        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk) {
            os_printf("AllocateTensors失败！\n");
            return false;
        }
        
        // 6. 获取输入输出tensor指针
        input = interpreter->input(0);
        output = interpreter->output(0);
        
        os_printf("模型初始化成功！Arena使用: %d bytes\n", 
                  interpreter->arena_used_bytes());
        
        return true;
    }

**5.3.2 执行推理**

.. code-block:: cpp

    // 执行推理
    bool RunInference(const void* input_data, int input_size) {
        // 1. 检查输入大小
        if (input_size != input->bytes) {
            os_printf("输入大小不匹配！期望: %d, 实际: %d\n", 
                      input->bytes, input_size);
            return false;
        }
        
        // 2. 填充输入数据
        memcpy(input->data.int8, input_data, input_size);
        
        // 3. 执行推理
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk) {
            os_printf("推理执行失败！\n");
            return false;
        }
        
        return true;
    }

5.4 获取推理结果
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.4.1 分类任务结果处理**

对于分类任务（如图像分类、语音识别）：

.. code-block:: cpp

    // 获取分类结果
    int GetClassificationResult(float* confidence) {
        // 获取输出tensor
        TfLiteTensor* output = interpreter->output(0);
        
        // 获取量化参数
        float scale = output->params.scale;
        int32_t zero_point = output->params.zero_point;
        
        // 找到最大概率的类别
        int max_index = 0;
        float max_score = -1000.0f;
        
        for (int i = 0; i < output->bytes; i++) {
            // 反量化
            float score = (output->data.int8[i] - zero_point) * scale;
            
            if (score > max_score) {
                max_score = score;
                max_index = i;
            }
        }
        
        if (confidence != nullptr) {
            *confidence = max_score;
        }
        
        os_printf("识别结果: 类别 %d, 置信度: %.2f\n", max_index, max_score);
        
        return max_index;
    }
    
    // 使用示例
    void ClassificationExample() {
        // 准备输入数据
        int8_t input_data[INPUT_SIZE];
        PrepareInputData(input_data);
        
        // 执行推理
        if (RunInference(input_data, INPUT_SIZE)) {
            // 获取结果
            float confidence = 0.0f;
            int class_id = GetClassificationResult(&confidence);
            
            // 根据类别ID处理结果
            if (confidence > 0.8f) {  // 置信度阈值
                os_printf("高置信度识别: 类别%d\n", class_id);
                // 执行相应操作
            }
        }
    }

**5.4.2 目标检测结果处理**

对于目标检测任务（如手势检测、物体检测）：

.. code-block:: cpp

    // 检测结果结构
    typedef struct {
        float x, y, w, h;      // 边界框坐标和尺寸
        int class_id;          // 类别ID
        float confidence;      // 置信度
    } DetectionResult;
    
    // 解析检测结果
    int GetDetectionResults(DetectionResult* results, int max_results) {
        TfLiteTensor* output = interpreter->output(0);
        
        float scale = output->params.scale;
        int32_t zero_point = output->params.zero_point;
        
        int result_count = 0;
        
        // 假设输出格式: [num_detections, 8] (x,y,w,h,score,class1,class2,class3)
        int num_boxes = output->dims->data[0];  // 如2268
        int box_size = output->dims->data[1];   // 如8
        
        for (int i = 0; i < num_boxes && result_count < max_results; i++) {
            int8_t* box_data = &output->data.int8[i * box_size];
            
            // 反量化置信度
            float score = (box_data[4] - zero_point) * scale;
            
            // 置信度阈值过滤
            if (score > 60.0f) {
                // 反量化坐标
                results[result_count].x = (box_data[0] - zero_point) * scale;
                results[result_count].y = (box_data[1] - zero_point) * scale;
                results[result_count].w = (box_data[2] - zero_point) * scale;
                results[result_count].h = (box_data[3] - zero_point) * scale;
                results[result_count].confidence = score;
                
                // 找出最大概率的类别
                float max_class_score = -1000.0f;
                int max_class_id = 0;
                
                for (int c = 0; c < 3; c++) {  // 假设3个类别
                    float class_score = (box_data[5 + c] - zero_point) * scale;
                    if (class_score > max_class_score) {
                        max_class_score = class_score;
                        max_class_id = c;
                    }
                }
                
                results[result_count].class_id = max_class_id;
                result_count++;
            }
        }
        
        return result_count;
    }
    
    // 使用示例
    void DetectionExample() {
        // 准备输入图像
        int8_t image_data[IMAGE_SIZE];
        CaptureImage(image_data);
        
        // 执行推理
        if (RunInference(image_data, IMAGE_SIZE)) {
            // 获取检测结果
            DetectionResult results[10];
            int num_detections = GetDetectionResults(results, 10);
            
            os_printf("检测到 %d 个目标\n", num_detections);
            
            for (int i = 0; i < num_detections; i++) {
                os_printf("目标 %d: 类别=%d, 置信度=%.2f, "
                         "位置=(%.1f,%.1f), 大小=(%.1f,%.1f)\n",
                         i, results[i].class_id, results[i].confidence,
                         results[i].x, results[i].y,
                         results[i].w, results[i].h);
            }
        }
    }

**5.4.3 回归任务结果处理**

对于回归任务（如关键点检测、姿态估计）：

.. code-block:: cpp

    // 获取回归结果
    bool GetRegressionOutput(float* output_values, int output_size) {
        TfLiteTensor* output = interpreter->output(0);
        
        if (output->bytes != output_size) {
            os_printf("输出大小不匹配！\n");
            return false;
        }
        
        float scale = output->params.scale;
        int32_t zero_point = output->params.zero_point;
        
        // 反量化所有输出值
        for (int i = 0; i < output_size; i++) {
            output_values[i] = (output->data.int8[i] - zero_point) * scale;
        }
        
        return true;
    }

5.5 完整应用示例
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.5.1 图像分类应用**

.. code-block:: cpp

    // 图像分类任务入口
    void ImageClassificationTask(void *arg) {
        // 1. 初始化模型
        if (!InitModel()) {
            os_printf("模型初始化失败！\n");
            vTaskDelete(NULL);
            return;
        }
        
        // 2. 推理循环
        while (1) {
            // 获取摄像头图像
            int8_t image_buffer[192*192*3];
            if (CaptureImage(image_buffer) == 0) {
                
                // 执行推理
                uint32_t start_time = rtos_get_time();
                
                if (RunInference(image_buffer, sizeof(image_buffer))) {
                    // 获取分类结果
                    float confidence = 0.0f;
                    int class_id = GetClassificationResult(&confidence);
                    
                    uint32_t end_time = rtos_get_time();
                    
                    // 输出结果
                    os_printf("分类结果: %d, 置信度: %.2f%%, 耗时: %ums\n",
                             class_id, confidence * 100.0f, 
                             end_time - start_time);
                }
            }
            
            // 延时
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

**5.5.2 CPU1主函数集成**

.. code-block:: cpp

    // app_main_cpu1.cc
    extern "C" {
    #include <os/os.h>
    #include "modules/pm.h"
    #include "FreeRTOS.h"
    #include "task.h"
    }
    
    // 声明推理任务
    extern void ImageClassificationTask(void *arg);
    
    extern "C" void app_main_cpu1(void *arg) {
        os_printf("CPU1 启动，AI推理任务初始化...\n");
        
        // 设置CPU频率为480MHz以获得最佳性能
        bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
        
        // 创建AI推理任务
        // 栈大小: 16KB, 优先级: 3
        xTaskCreate(ImageClassificationTask, 
                   "ai_inference", 
                   16*1024, 
                   NULL, 
                   3, 
                   NULL);
        
        // CPU1主循环
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

6. API参考
---------------------------------

6.1 核心API
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**6.1.1 模型加载**

.. code-block:: cpp

    // 加载模型
    const tflite::Model* model = tflite::GetModel(model_data);
    
    // 验证模型版本
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        // 版本不匹配处理
    }

**6.1.2 算子解析器**

.. code-block:: cpp

    // 创建可变算子解析器（指定算子数量）
    tflite::MicroMutableOpResolver<N> op_resolver;
    
    // 添加算子
    op_resolver.AddConv2D();                          // 卷积
    op_resolver.AddDepthwiseConv2D();                 // 深度可分离卷积
    op_resolver.AddFullyConnected();                  // 全连接
    op_resolver.AddSoftmax();                         // Softmax
    op_resolver.AddReshape();                         // 重塑
    op_resolver.AddPad();                             // 填充
    op_resolver.AddMaxPool2D();                       // 最大池化
    op_resolver.AddAdd();                             // 加法
    op_resolver.AddMul();                             // 乘法
    
    // 使用优化的INT8算子
    op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());

**6.1.3 解释器**

.. code-block:: cpp

    // 创建解释器
    tflite::MicroInterpreter interpreter(
        model,              // 模型指针
        op_resolver,        // 算子解析器
        tensor_arena,       // tensor arena缓冲区
        tensor_arena_size   // arena大小
    );
    
    // 分配tensor内存
    TfLiteStatus status = interpreter.AllocateTensors();
    if (status != kTfLiteOk) {
        // 分配失败处理
    }
    
    // 获取arena使用量
    size_t used_bytes = interpreter.arena_used_bytes();

**6.1.4 输入输出处理**

.. code-block:: cpp

    // 获取输入tensor
    TfLiteTensor* input = interpreter.input(0);
    
    // 填充输入数据
    for (int i = 0; i < input->bytes; ++i) {
        input->data.int8[i] = input_data[i];
    }
    
    // 执行推理
    TfLiteStatus invoke_status = interpreter.Invoke();
    if (invoke_status != kTfLiteOk) {
        // 推理失败处理
    }
    
    // 获取输出tensor
    TfLiteTensor* output = interpreter.output(0);
    
    // 处理量化输出（反量化）
    float scale = output->params.scale;
    int32_t zero_point = output->params.zero_point;
    for (int i = 0; i < output->bytes; ++i) {
        float value = (output->data.int8[i] - zero_point) * scale;
        // 使用反量化后的值
    }






7. 参考资源
---------------------------------

7.1 官方文档
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* TensorFlow Lite Micro官方文档: https://www.tensorflow.org/lite/microcontrollers
* TensorFlow Lite模型优化: https://www.tensorflow.org/lite/performance/model_optimization
* CMSIS-NN文档: https://arm-software.github.io/CMSIS_5/NN/html/index.html

7.2 示例代码
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* Micro Speech示例: ``projects/tflite_micro/micro_speech/``
* Gesture Detection示例: ``projects/tflite_micro/gesture_detection/``
* 石头剪刀布完整应用: 参考 :doc:`../projects/rock_paper_scissors/index`

7.3 相关组件
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* bk_tflite_micro组件: ``components/bk_tflite_micro/``
* FreeRTOS任务管理
* PSRAM内存管理
* DVP摄像头接口（用于视觉应用）


