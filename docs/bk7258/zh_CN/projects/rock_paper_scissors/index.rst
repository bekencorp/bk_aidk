石头剪刀布
=================================

:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是基于开源的手势检测模型和TensorFlow Lite Micro轻量级深度学习框架实现的石头剪刀布猜拳游戏demo。通过DVP摄像头采集手势图像，使用TensorFlow Lite Micro在CPU1上进行AI推理识别手势，然后与系统随机生成的手势进行对战，最终将对局结果显示到双LCD屏幕上，并通过语音播报输赢结果。

1.1 系统架构
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* **双核架构**：
  
  * CPU0：运行WIFI、蓝牙
  * CPU1：运行多媒体、TensorFlow Lite Micro AI推理任务（480MHz）

* **AI处理流程**：
  
  * 图像采集 → 预处理 → TensorFlow推理 → 后处理 → 结果输出

1.2 硬件配置
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* SPI LCD X2 (GC9D01) - 双屏显示
* DVP摄像头 (GC2145) - 图像采集
* 麦克风 - 语音输入
* 喇叭 - 语音播报
* SD NAND 128MB - 存储资源
* NFC (MFRC522) - 近场通信
* 陀螺仪 (SC7A20H) - 姿态检测
        * 充电管理芯片 (ETA3422)
        * 锂电池

2. 工程结构
---------------------------------

2.1 目录组织
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

::

    rock_paper_scissors/
    ├── main/
    │   ├── app_cpu0_main.c           # CPU0主程序
    │   ├── app_cpu1_main.c           # CPU1入口
    │   ├── app_main_cpu1.cc          # CPU1 TensorFlow任务
    │   ├── tflite/                   # TensorFlow Lite代码
    │   │   ├── main_functions.cc     # 推理主逻辑
    │   │   ├── gesture_detection_model_data.cc  # 模型数据
    │   │   ├── image_provider.cc     # 图像输入接口
    │   │   ├── detection_responder.cc # 结果处理
    │   │   └── model_settings.h      # 模型配置
    │   ├── media/                    # 媒体处理
    │   │   ├── media_main.c          # 媒体主控制
    │   │   ├── media_audio.c         # 音频处理
    │   │   └── resource/             # 语音资源
    │   ├── display/                  # 显示控制
    │   │   ├── lvgl_app.c            # LVGL应用
    │   │   └── lv_*.c                # 显示界面
    │   └── CMakeLists.txt            # 构建配置
    ├── config/                       # 芯片配置
    └── CMakeLists.txt                # 项目构建

2.2 关键组件
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* **bk_tflite_micro**：TensorFlow Lite Micro组件
* **multimedia**：多媒体处理
* **lvgl**：图形界面库
* **audio_play**：音频播放
* **media_service**：媒体服务

3. TensorFlow Lite Micro代码实现
---------------------------------

3.1 模型配置
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**模型参数** (``model_settings.h``)：

.. code-block:: cpp

    // 输入图像尺寸
    constexpr int kNumCols = 192;      // 宽度
    constexpr int kNumRows = 192;      // 高度
    constexpr int kNumChannels = 3;    // RGB通道
    
    // 输出类别
    constexpr int kCategoryCount = 2;
    
    // 最大图像尺寸
    constexpr int kMaxImageSize = kNumCols * kNumRows * kNumChannels;

3.2 模型初始化
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**CPU1初始化流程** (``app_main_cpu1.cc``)：

.. code-block:: cpp

    extern "C" void app_main_cpu1(void *arg) {
        // 1. 设置CPU频率为480MHz
        bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
        
        // 2. 创建TensorFlow推理任务
        xTaskCreate(tflite_task, "test", 1024*16, NULL, 3, NULL);
        
        // 3. CPU1主循环
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

**TensorFlow任务初始化**：

.. code-block:: cpp

    void tflite_task(void *arg) {
        // 1. 注册调试日志回调
        RegisterDebugLogCallback(debugLogCallback);
        
        // 2. 从PSRAM分配模型数据缓冲区
        data_ptr = (unsigned char*)psram_malloc(
            g_gesture_detection_model_data_len);
        
        if (data_ptr == NULL) {
            os_printf("===data buffer malloc error====\r\n");
            return;
        }
        
        // 3. 拷贝模型数据到PSRAM
        os_memcpy(data_ptr, g_gesture_detection_model_data, 
                  g_gesture_detection_model_data_len);
        
        // 4. 调用setup初始化模型
        setup();
        
        // 5. 推理循环
        while (1) {
            loop();  // 执行一次推理
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

3.3 模型加载
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**Setup函数实现** (``main_functions.cc``)：

.. code-block:: cpp

    void setup() {
        // 1. 初始化目标平台
        tflite::InitializeTarget();
        
        // 2. 加载模型
        model = tflite::GetModel(data_ptr);
        
        // 3. 验证模型版本
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            MicroPrintf("Model schema version mismatch!\r\n");
            return;
        }
        
        // 4. 创建算子解析器（13个算子）
        static tflite::MicroMutableOpResolver<13> micro_op_resolver;
        micro_op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
        micro_op_resolver.AddPad();
        micro_op_resolver.AddMaxPool2D();
        micro_op_resolver.AddAdd();
        micro_op_resolver.AddQuantize();
        micro_op_resolver.AddConcatenation();
        micro_op_resolver.AddReshape();
        micro_op_resolver.AddPadV2();
        micro_op_resolver.AddResizeNearestNeighbor();
        micro_op_resolver.AddLogistic();
        micro_op_resolver.AddTranspose();
        micro_op_resolver.AddSplitV();
        micro_op_resolver.AddMul();
        
        // 5. 从PSRAM分配Tensor Arena（360KB）
        uint8_t *tensor_arena = (uint8_t *)psram_malloc(360 * 1024);
        if (tensor_arena == NULL) {
            MicroPrintf("Tensor arena allocation failed\r\n");
            return;
        }
        
        // 6. 创建解释器
        static tflite::MicroInterpreter static_interpreter(
            model, micro_op_resolver, tensor_arena, 360 * 1024);
        interpreter = &static_interpreter;
        
        // 7. 分配tensor内存
        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk) {
            MicroPrintf("AllocateTensors() failed\r\n");
            return;
        }
        
        // 8. 获取输入tensor
        input = interpreter->input(0);
    }

3.4 推理执行
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**Loop函数实现**：

.. code-block:: cpp

    void loop() {
        TfLiteTensor* output = NULL;
        uint64_t before, after;
        
        // 1. 获取摄像头图像
        MicroPrintf("======Start detecting gesture======\r\n");
        before = (uint64_t)rtos_get_time();
        
        if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, 
                                  input->data.int8, 0)) {
            MicroPrintf("Image capture failed.\r\n");
            return;
        }
        
        // 2. 执行模型推理
        if (kTfLiteOk != interpreter->Invoke()) {
            MicroPrintf("Invoke failed.\r\n");
            return;
        }
        
        // 3. 获取输出tensor和量化参数
        output = interpreter->output(0);
        g_scale = output->params.scale;
        g_zero_point = output->params.zero_point;
        
        // 4. 后处理解析结果
        uint8_t result = GESTURE_MAX;
        post_process(output->data.int8, &result);
        
        // 5. 计算推理耗时
        after = (uint64_t)rtos_get_time();
        MicroPrintf("Detection time: %d ms\r\n", (uint32_t)(after - before));
    }

3.5 结果后处理
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**手势识别后处理**：

.. code-block:: cpp

    uint8_t post_process(int8_t *out_data, uint8_t *result) {
        *result = GESTURE_MAX;
        
        // 遍历2268个检测框
        for(int i = 0; i < 2268; i++) {
            // 1. 反量化置信度分数
            float score = (out_data[i*8 + 4] - g_zero_point) * g_scale;
            
            // 2. 置信度阈值过滤（>62）
            if(score > 62) {
                // 3. 反量化边界框坐标
                int x = (out_data[i*8 + 0] - g_zero_point) * g_scale;
                int y = (out_data[i*8 + 1] - g_zero_point) * g_scale;
                int w = (out_data[i*8 + 2] - g_zero_point) * g_scale;
                int h = (out_data[i*8 + 3] - g_zero_point) * g_scale;
                
                // 4. 反量化手势类别分数
                float paper = (out_data[i*8 + 5] - g_zero_point) * g_scale;
                float rock = (out_data[i*8 + 6] - g_zero_point) * g_scale;
                float scissors = (out_data[i*8 + 7] - g_zero_point) * g_scale;
                
                // 5. 判断手势类型（阈值>90）
                if (paper > 90) {
                    MicroPrintf("Paper detected!\r\n");
                    *result = GESTURE_PAPER;
                } else if (rock > 90) {
                    MicroPrintf("Rock detected!\r\n");
                    *result = GESTURE_ROCK;
                } else if (scissors > 90) {
                    MicroPrintf("Scissors detected!\r\n");
                    *result = GESTURE_SCISSORS;
                }
                
                break;  // 只处理第一个高置信度检测
            }
        }
        
        return 0;
    }

3.6 图像输入接口
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**GetImage函数** (``image_provider.cc``)：

从摄像头或预存图像获取输入数据，并转换为INT8格式：

.. code-block:: cpp

    TfLiteStatus GetImage(int image_width, int image_height,
                         int channels, int8_t* image_data, int index) {
        // 1. 从摄像头或缓存获取图像
        // 2. 转换为192x192x3的INT8格式
        // 3. 填充到image_data缓冲区
        return kTfLiteOk;
    }

3.7 外部调用接口
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

提供C接口供其他模块调用：

.. code-block:: cpp

    // 初始化接口
    extern "C" void tflite_task_init_c(void *arg) {
        bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
        RegisterDebugLogCallback(debugLogCallback);
        
        data_ptr = (unsigned char*)psram_malloc(
            g_gesture_detection_model_data_len);
        os_memcpy(data_ptr, g_gesture_detection_model_data, 
                  g_gesture_detection_model_data_len);
        
        setup();
    }
    
    // 处理接口
    extern "C" void tflite_process(uint8_t *data, uint32_t len, 
                                   uint8_t *result) {
        process_pic(data, len, result);
    }





4. 测试说明
---------------------------------

4.1 手势方向
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

测试时需要按照如图所示的方向伸手手势，且需要将手势正对摄像头上方约50cm处。

.. figure:: ../../../_static/rock_paper_scissors_pic.jpg
    :align: center
    :alt: Rock_Paper_Scissors Gesture Direction
    :figclass: align-center

    Figure 1. Rock_Paper_Scissors Gesture Direction

4.2 测试步骤
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

1. **准备阶段**
   
   * 板子上电后会听到语音播报"准备出拳"
   * 将手势正对摄像头上方
   * 保持手势不变直到语音提醒"将手放下"

2. **检测阶段**
   
   * DVP摄像头采集192x192图像
   * CPU1执行TensorFlow Lite推理（约200-400ms）
   * 系统识别手势类型

3. **结果显示**
   
   * 若检测成功：
     
     - 双LCD屏幕分别显示双方手势
     - 语音播报对战结果（胜/负/平）
   
   * 若检测失败：
     
     - 语音提醒"未检测成功"
     - 可重新尝试

4.3 调试信息
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

通过串口（115200波特率）可查看推理日志：

.. code-block:: text

    ======Start detecting gesture======
    Paper detected!
    Detection time: 285 ms

5. 参考资源
---------------------------------

* **TensorFlow Lite Micro开发指南**：:doc:`../../api-reference/tensorflow`
* **模型训练**：基于YOLO目标检测架构
* **组件源码**：``components/bk_tflite_micro/``
* **示例工程**：``projects/rock_paper_scissors/``
