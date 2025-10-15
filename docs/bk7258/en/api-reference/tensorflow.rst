TensorFlow Lite Micro Developer Guide
===========================================

:link_to_translation:`zh_CN:[中文]`

1. Overview
---------------------------------

TensorFlow Lite Micro (TFLM) is a lightweight machine learning inference framework designed specifically for microcontrollers and other resource-constrained devices. It is a streamlined version of TensorFlow Lite, optimized for embedded systems.

This SDK integrates the TensorFlow Lite Micro framework and provides complete example projects to help developers quickly get started with edge AI application development.

1.1 Key Features
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* **Lightweight Design**: Optimized for embedded devices with minimal memory footprint
* **Hardware Acceleration**: Supports CMSIS-NN acceleration library for improved inference performance
* **Easy Integration**: Complete CMake build system and example projects
* **Rich Operator Support**: Supports commonly used deep learning operators
* **Dual-Core Architecture Support**: Fully utilizes BK7258 multi-core processing capabilities

1.2 System Requirements
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* Chip Platform: BK7258 (ARM Cortex-M33 dual-core)
* Memory: PSRAM for storing models and tensor arena

1.3 System Architecture
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

Since CPU0 has a maximum frequency of 240MHz and CPU1 can run at 480MHz.

Therefore, TensorFlow Lite Micro model inference runs on CPU1.


2. Component Architecture
---------------------------------

2.1 Directory Structure
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

TensorFlow Lite Micro related code consists of two main parts:

**Component Directory** (``components/bk_tflite_micro/``)::

    bk_tflite_micro/
    ├── CMakeLists.txt              # Component build file
    ├── Kconfig                     # Component configuration options
    └── tflite-micro/               # TensorFlow Lite Micro source code
        └── tensorflow/
            └── lite/
                └── micro/          # Core implementation

**Example Project Directory** (``projects/tflite_micro/``)::

    tflite_micro/
    ├── micro_speech/               # Speech recognition example
    │   ├── main/                   # Main program code
    │   │   ├── app_main_cpu1.cc   # CPU1 main function
    │   │   ├── tflite/            # TensorFlow Lite related code
    │   │   │   ├── main_functions.cc     # Inference main logic
    │   │   │   ├── micro_speech_quantized_model_data.cc  # Model data
    │   │   │   └── ...
    │   │   └── CMakeLists.txt     # Main program build file
    │   ├── config/                 # Chip configuration files
    │   ├── CMakeLists.txt         # Project build file
    │   └── Makefile               # Top-level Makefile
    └── gesture_detection/          # Gesture detection example
        ├── main/                   # Main program code
        │   ├── app_main_cpu1.cc   # CPU1 main function
        │   ├── tflite/            # TensorFlow Lite related code
        │   │   ├── main_functions.cc         # Inference main logic
        │   │   ├── gesture_detection_model_data.cc  # Model data
        │   │   ├── image_provider.cc         # Image input interface
        │   │   └── detection_responder.cc    # Result processing interface
        │   └── CMakeLists.txt     # Main program build file
        └── config/                 # Chip configuration files

2.2 Component Configuration
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

TensorFlow Lite Micro component is configured through Kconfig and needs to be enabled in project configuration:

.. code-block:: kconfig

    menu "TFLite Micro"
        config TFLITE_MICRO
            bool "Enable TFLite Micro"
            default n
    endmenu

Once enabled, the component will automatically compile the TensorFlow Lite Micro static library and link it to the application.



3. Example Project Details
---------------------------------

3.1 Micro Speech (Speech Recognition)
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

Micro Speech is an audio feature-based keyword recognition example that can recognize simple voice commands like "yes" and "no".

**3.1.1 Core Components**

* **Audio Preprocessing Model**: Converts raw audio data to feature vectors
* **Speech Recognition Model**: Classifies feature vectors to recognize keywords
* **Test Audio Data**: Contains test samples like yes, no, silence, noise

**3.1.2 Main Flow**

1. **Initialization Phase**:

   .. code-block:: cpp

       // Run AI inference task on CPU1
       void app_main_cpu1(void *arg) {
           // Set CPU frequency to 480MHz for better performance
           bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
           
           // Create TensorFlow Lite inference task
           xTaskCreate(tflite_task, "test", 1024*16, NULL, 3, NULL);
       }

2. **Model Loading**:

   .. code-block:: cpp

       void tflite_task(void *arg) {
           // Register debug log callback
           RegisterDebugLogCallback(debugLogCallback);
           
           // Allocate model data buffer from PSRAM
           data_ptr = (unsigned char*)psram_malloc(
               g_micro_speech_quantized_model_data_len);
           
           // Copy model data to PSRAM
           os_memcpy(data_ptr, g_micro_speech_quantized_model_data, 
               g_micro_speech_quantized_model_data_len);
           
           // Loop inference execution
           while (1) {
               loop();
               vTaskDelay(pdMS_TO_TICKS(5000));
           }
       }

3. **Feature Extraction**:

   .. code-block:: cpp

       // Generate features using audio preprocessing model
       TfLiteStatus GenerateFeatures(const int16_t* audio_data,
                                     const size_t audio_data_size,
                                     Features* features_output) {
           // Load preprocessing model
           const tflite::Model* model = 
               tflite::GetModel(g_audio_preprocessor_int8_model_data);
           
           // Create operator resolver
           AudioPreprocessorOpResolver op_resolver;
           RegisterOps(op_resolver);
           
           // Create interpreter
           tflite::MicroInterpreter interpreter(model, op_resolver, 
                                               g_arena, kArenaSize);
           
           // Allocate tensor memory
           interpreter.AllocateTensors();
           
           // Process audio data to generate features
           // ...
       }

4. **Inference Execution**:

   .. code-block:: cpp

       // Load speech recognition model and perform inference
       TfLiteStatus LoadMicroSpeechModelAndPerformInference(
           const Features& features, const char* expected_label) {
           // Load model
           const tflite::Model* model = 
               tflite::GetModel(g_micro_speech_quantized_model_data);
           
           // Create operator resolver
           MicroSpeechOpResolver op_resolver;
           op_resolver.AddReshape();
           op_resolver.AddFullyConnected();
           op_resolver.AddDepthwiseConv2D();
           op_resolver.AddSoftmax();
           
           // Create interpreter and allocate memory
           tflite::MicroInterpreter interpreter(model, op_resolver, 
                                               g_arena, kArenaSize);
           interpreter.AllocateTensors();
           
           // Fill input data
           TfLiteTensor* input = interpreter.input(0);
           std::copy_n(&features[0][0], kFeatureElementCount,
                      tflite::GetTensorData<int8_t>(input));
           
           // Execute inference
           interpreter.Invoke();
           
           // Get output results
           TfLiteTensor* output = interpreter.output(0);
           // Dequantize and parse results
           // ...
       }

**3.1.3 Memory Configuration**

* **Arena Size**: 28584 bytes (for storing tensor data)
* **Task Stack Size**: 1024*16 bytes
* **Model Storage**: Uses PSRAM to store model data

3.2 Gesture Detection
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

Gesture Detection is a vision-based gesture recognition example that can detect gestures like rock, paper, scissors.

**3.2.1 Core Components**

* **Image Input Interface**: ``image_provider.cc`` - Responsible for capturing camera images
* **Gesture Detection Model**: ``gesture_detection_model_data.cc`` - Contains trained model
* **Result Processing Interface**: ``detection_responder.cc`` - Processes detection results and outputs
* **Model Configuration**: ``model_settings.cc/h`` - Defines model input/output parameters

**3.2.2 Main Flow**

1. **Initialization Phase**:

   .. code-block:: cpp

       void setup() {
           tflite::InitializeTarget();
           
           // Load model
           model = tflite::GetModel(data_ptr);
           
           // Create operator resolver (13 operators)
           static tflite::MicroMutableOpResolver<13> micro_op_resolver;
           micro_op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
           micro_op_resolver.AddPad();
           micro_op_resolver.AddMaxPool2D();
           // ... Add more operators
           
           // Allocate tensor arena from PSRAM (360KB)
           uint8_t *tensor_arena = (uint8_t *)psram_malloc(kTensorArenaSize);
           
           // Create interpreter
           static tflite::MicroInterpreter static_interpreter(
               model, micro_op_resolver, tensor_arena, kTensorArenaSize);
           interpreter = &static_interpreter;
           
           // Allocate tensors
           interpreter->AllocateTensors();
           
           // Get input tensor
           input = interpreter->input(0);
       }

2. **Image Capture and Inference**:

   .. code-block:: cpp

       void loop() {
           // Get image data
           if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, 
                                     input->data.int8, 0)) {
               MicroPrintf("Image capture failed.\r\n");
           }
           
           // Execute inference
           if (kTfLiteOk != interpreter->Invoke()) {
               MicroPrintf("Invoke failed.\r\n");
           }
           
           // Get output and post-process
           TfLiteTensor* output = interpreter->output(0);
           g_scale = output->params.scale;
           g_zero_point = output->params.zero_point;
           
           post_process(output->data.int8);
       }

3. **Result Post-processing**:

   .. code-block:: cpp

       uint8_t post_process(int8_t *out_data) {
           // Iterate through all detection boxes (2268 anchors)
           for(int i = 0; i < 2268; i++) {
               // Dequantize confidence score
               float score = (out_data[i*8 + 4] - g_zero_point) * g_scale;
               
               if(score > 62) {  // Confidence threshold
                   // Parse bounding box coordinates
                   int x = (out_data[i*8 + 0] - g_zero_point) * g_scale;
                   int y = (out_data[i*8 + 1] - g_zero_point) * g_scale;
                   int w = (out_data[i*8 + 2] - g_zero_point) * g_scale;
                   int h = (out_data[i*8 + 3] - g_zero_point) * g_scale;
                   
                   // Parse gesture class
                   float paper = (out_data[i*8 + 5] - g_zero_point) * g_scale;
                   float rock = (out_data[i*8 + 6] - g_zero_point) * g_scale;
                   float scissors = (out_data[i*8 + 7] - g_zero_point) * g_scale;
                   
                   // Determine gesture type and output
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

**3.2.3 Memory Configuration**

* **Tensor Arena Size**: 360KB (allocated from PSRAM)
* **Model Input**: 192x192x3 INT8 image
* **Model Output**: 2268x8 INT8 detection results (coordinates, confidence, classes)

4. Developer Guide
---------------------------------

4.1 Environment Setup
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**4.1.1 Install Development Tools**

1. Install ARM GCC toolchain
2. Install CMake (version ≥3.5)
3. Configure ARMINO_PATH environment variable

**4.1.2 Enable TensorFlow Lite Micro Component**

Enable TFLITE_MICRO in project configuration file:

.. code-block:: bash

    # Use menuconfig to configure
    make menuconfig
    # Navigate to TFLite Micro -> Enable TFLite Micro, select [Y]

Or add in ``sdkconfig`` file:

.. code-block:: text

    CONFIG_TFLITE_MICRO=y

4.2 Create Custom AI Application
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**4.2.1 Prepare Model Files**

1. **Train Model**: Use TensorFlow/Keras to train model
2. **Quantize Model**: Convert to INT8 quantized TFLite model
3. **Convert to C Array**: Use xxd tool to convert to header file

.. code-block:: bash

    # Convert .tflite file to C array
    xxd -i model.tflite > model_data.cc

**4.2.2 Integrate Model into Project**

1. Create model data files:

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
           // Model data...
       };
       const int g_model_data_len = sizeof(g_model_data);

2. Create main inference code:

   .. code-block:: cpp

       #include "tensorflow/lite/micro/micro_interpreter.h"
       #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
       #include "tensorflow/lite/schema/schema_generated.h"
       #include "model_data.h"
       
       // Define tensor arena size
       constexpr int kTensorArenaSize = 100 * 1024;
       uint8_t *tensor_arena = nullptr;
       
       void setup() {
           // Load model
           const tflite::Model* model = tflite::GetModel(g_model_data);
           
           // Add required operators
           static tflite::MicroMutableOpResolver<5> micro_op_resolver;
           micro_op_resolver.AddConv2D();
           micro_op_resolver.AddFullyConnected();
           micro_op_resolver.AddSoftmax();
           // Add other required operators...
           
           // Allocate memory
           tensor_arena = (uint8_t *)psram_malloc(kTensorArenaSize);
           
           // Create interpreter
           static tflite::MicroInterpreter static_interpreter(
               model, micro_op_resolver, tensor_arena, kTensorArenaSize);
           
           static_interpreter.AllocateTensors();
           
           // Get input/output tensors
           TfLiteTensor* input = static_interpreter.input(0);
           TfLiteTensor* output = static_interpreter.output(0);
       }
       
       void loop() {
           // Fill input data
           // ...
           
           // Execute inference
           interpreter->Invoke();
           
           // Process output results
           // ...
       }

**4.2.3 Configure CMakeLists.txt**

Add TensorFlow Lite related source files in project's ``main/CMakeLists.txt``:

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

**4.2.4 Add Required Compile Options**

Add C++ compile options in project's top-level ``CMakeLists.txt``:

.. code-block:: cmake

    # Disable certain warnings
    armino_build_set_property(COMPILE_OPTIONS "-Wno-unused-variable" APPEND)
    armino_build_set_property(COMPILE_OPTIONS "-Wno-sign-compare" APPEND)
    armino_build_set_property(CXX_COMPILE_OPTIONS "-fpermissive" APPEND)

4.3 Dual-Core Application Development
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

BK7258 supports dual-core architecture, allowing AI inference tasks to run on CPU1 to fully utilize hardware resources.

**4.3.1 CPU Configuration**

Enable dual-core mode in configuration:

.. code-block:: text

    CONFIG_SYS_CPU0=y    # CPU0 configuration
    CONFIG_SYS_CPU1=y    # CPU1 configuration

**4.3.2 Task Assignment**

* **CPU0**: Runs system management, communication tasks
* **CPU1**: Runs AI inference tasks

Example code:

.. code-block:: cpp

    // CPU1 entry function
    extern "C" void app_main_cpu1(void *arg) {
        // Increase CPU frequency for better performance
        bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
        
        // Create AI inference task
        xTaskCreate(tflite_task, "tflite", 1024*16, NULL, 3, NULL);
    }

4.4 Performance Optimization Tips
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**4.4.1 Memory Optimization**

1. **Use PSRAM**: Store model data and tensor arena in PSRAM

   .. code-block:: cpp

       // Allocate model buffer from PSRAM
       data_ptr = (unsigned char*)psram_malloc(model_data_len);
       
       // Allocate tensor arena from PSRAM
       tensor_arena = (uint8_t *)psram_malloc(kTensorArenaSize);

2. **Optimize Arena Size**: Use ``interpreter->arena_used_bytes()`` to get actual usage and adjust arena size

   .. code-block:: cpp

       MicroPrintf("Arena used: %d bytes\n", 
                   interpreter->arena_used_bytes());

3. **Static Memory Allocation**: Define ``TF_LITE_STATIC_MEMORY`` to avoid dynamic allocation

**4.4.2 Inference Optimization**

1. **INT8 Quantization**: Use INT8 quantized models to reduce memory footprint and computation
2. **CMSIS-NN Acceleration**: Component enables CMSIS-NN optimized kernels by default
3. **CPU Frequency Adjustment**: Adjust CPU frequency based on performance requirements

   .. code-block:: cpp

       // Set to maximum frequency 480MHz
       bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);

4. **Task Priority**: Set appropriate priority for AI inference tasks

   .. code-block:: cpp

       xTaskCreate(tflite_task, "tflite", stack_size, NULL, 
                   priority, NULL);

**4.4.3 Model Optimization**

1. **Simplify Network Structure**: Reduce number of layers and parameters
2. **Pruning and Compression**: Use model pruning techniques to reduce model size
3. **Operator Selection**: Prioritize CMSIS-NN supported operators (Conv2D, DepthwiseConv2D, FullyConnected, etc.)

5. Model Adaptation and Integration
---------------------------------

5.1 Model Conversion Flow
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.1.1 BK7258 Chip Description**

.. important::
   **BK7258 chip uses ARM Cortex-M33 CPU architecture and does NOT include an NPU (Neural Processing Unit) hardware accelerator.**
   
   Therefore:
   
   * **No NPU model conversion required**
   * All inference runs on CPU
   * Uses CMSIS-NN software library for operator optimization
   * Models only need to be converted to TensorFlow Lite format

**5.1.2 Model Conversion Steps**

Convert trained TensorFlow/Keras model to TFLite INT8 quantized model:

.. code-block:: python

    import tensorflow as tf
    import numpy as np
    
    # 1. Load trained model
    model = tf.keras.models.load_model('your_model.h5')
    
    # 2. Create TFLite converter
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    
    # 3. Enable INT8 quantization optimization
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    # 4. Provide representative dataset for quantization calibration
    def representative_dataset():
        # Use subset of training or validation data
        for i in range(100):
            # Ensure data shape matches model input
            data = np.random.rand(1, input_height, input_width, channels)
            yield [data.astype(np.float32)]
    
    converter.representative_dataset = representative_dataset
    
    # 5. Set input/output types to INT8
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    # 6. Convert and save
    tflite_model = converter.convert()
    with open('model_int8.tflite', 'wb') as f:
        f.write(tflite_model)
    
    print("Model conversion completed!")

**5.1.3 Validate Model**

After conversion, it's recommended to validate model accuracy on PC:

.. code-block:: python

    import tensorflow as tf
    
    # Load TFLite model
    interpreter = tf.lite.Interpreter(model_path="model_int8.tflite")
    interpreter.allocate_tensors()
    
    # Get input/output details
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    
    print("Input info:", input_details)
    print("Output info:", output_details)
    
    # Test inference
    test_data = np.random.rand(*input_details[0]['shape']).astype(np.int8)
    interpreter.set_tensor(input_details[0]['index'], test_data)
    interpreter.invoke()
    output = interpreter.get_tensor(output_details[0]['index'])
    print("Output result:", output)

5.2 Add Model to Project
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.2.1 Convert to C Array**

Use ``xxd`` tool to convert ``.tflite`` file to C/C++ array:

.. code-block:: bash

    # Method 1: Generate .cc file using xxd
    xxd -i model_int8.tflite > model_data.cc
    
    # Method 2: Generate .h header file using xxd
    xxd -i model_int8.tflite model_data.h

Generated file format looks like:

.. code-block:: cpp

    unsigned char model_int8_tflite[] = {
      0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, ...
    };
    unsigned int model_int8_tflite_len = 123456;

**5.2.2 Create Model Data Files**

Manually create standardized model files:

.. code-block:: cpp

    // my_model_data.h
    #ifndef MY_MODEL_DATA_H_
    #define MY_MODEL_DATA_H_
    
    #include <stdint.h>
    
    // Model data declaration
    extern const unsigned char g_my_model_data[];
    extern const int g_my_model_data_len;
    
    #endif  // MY_MODEL_DATA_H_

.. code-block:: cpp

    // my_model_data.cc
    #include "my_model_data.h"
    
    // 8-byte alignment for optimized access performance
    alignas(8) const unsigned char g_my_model_data[] = {
        // Paste xxd generated array data here
        0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
        // ... other data
    };
    
    const int g_my_model_data_len = sizeof(g_my_model_data);

**5.2.3 Add to CMakeLists.txt**

Add model file in project's ``main/CMakeLists.txt``:

.. code-block:: cmake

    if (CONFIG_SYS_CPU1)
        list(APPEND srcs
            app_cpu1_main.c
            app_main_cpu1.cc
            tflite/my_model_data.cc      # Add model data file
            tflite/main_functions.cc      # Inference main logic
        )
        
        list(APPEND incs
            tflite
        )
    endif()

5.3 Write Inference Code
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.3.1 Basic Inference Framework**

Create ``inference.cc`` file to implement complete inference flow:

.. code-block:: cpp

    #include "tensorflow/lite/micro/micro_interpreter.h"
    #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
    #include "tensorflow/lite/schema/schema_generated.h"
    #include "my_model_data.h"
    
    extern "C" {
    #include "os/os.h"
    #include "os/mem.h"
    }
    
    // Global variables
    namespace {
        const tflite::Model* model = nullptr;
        tflite::MicroInterpreter* interpreter = nullptr;
        TfLiteTensor* input = nullptr;
        TfLiteTensor* output = nullptr;
        
        // Adjust arena size based on model requirements
        constexpr int kTensorArenaSize = 200 * 1024;
        uint8_t* tensor_arena = nullptr;
    }
    
    // Initialize model
    bool InitModel() {
        // 1. Load model
        model = tflite::GetModel(g_my_model_data);
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            os_printf("Model version mismatch!\n");
            return false;
        }
        
        // 2. Register required operators
        static tflite::MicroMutableOpResolver<6> micro_op_resolver;
        micro_op_resolver.AddConv2D();
        micro_op_resolver.AddDepthwiseConv2D();
        micro_op_resolver.AddFullyConnected();
        micro_op_resolver.AddSoftmax();
        micro_op_resolver.AddReshape();
        micro_op_resolver.AddMaxPool2D();
        // Add other operators based on actual model
        
        // 3. Allocate tensor arena from PSRAM
        tensor_arena = (uint8_t*)psram_malloc(kTensorArenaSize);
        if (tensor_arena == NULL) {
            os_printf("PSRAM allocation failed!\n");
            return false;
        }
        
        // 4. Create interpreter
        static tflite::MicroInterpreter static_interpreter(
            model, micro_op_resolver, tensor_arena, kTensorArenaSize);
        interpreter = &static_interpreter;
        
        // 5. Allocate tensors
        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk) {
            os_printf("AllocateTensors failed!\n");
            return false;
        }
        
        // 6. Get input/output tensor pointers
        input = interpreter->input(0);
        output = interpreter->output(0);
        
        os_printf("Model initialized successfully! Arena used: %d bytes\n", 
                  interpreter->arena_used_bytes());
        
        return true;
    }

**5.3.2 Execute Inference**

.. code-block:: cpp

    // Execute inference
    bool RunInference(const void* input_data, int input_size) {
        // 1. Check input size
        if (input_size != input->bytes) {
            os_printf("Input size mismatch! Expected: %d, Actual: %d\n", 
                      input->bytes, input_size);
            return false;
        }
        
        // 2. Fill input data
        memcpy(input->data.int8, input_data, input_size);
        
        // 3. Execute inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk) {
            os_printf("Inference execution failed!\n");
            return false;
        }
        
        return true;
    }

5.4 Get Inference Results
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.4.1 Classification Task Result Processing**

For classification tasks (e.g., image classification, speech recognition):

.. code-block:: cpp

    // Get classification result
    int GetClassificationResult(float* confidence) {
        // Get output tensor
        TfLiteTensor* output = interpreter->output(0);
        
        // Get quantization parameters
        float scale = output->params.scale;
        int32_t zero_point = output->params.zero_point;
        
        // Find class with maximum probability
        int max_index = 0;
        float max_score = -1000.0f;
        
        for (int i = 0; i < output->bytes; i++) {
            // Dequantize
            float score = (output->data.int8[i] - zero_point) * scale;
            
            if (score > max_score) {
                max_score = score;
                max_index = i;
            }
        }
        
        if (confidence != nullptr) {
            *confidence = max_score;
        }
        
        os_printf("Recognition result: Class %d, Confidence: %.2f\n", max_index, max_score);
        
        return max_index;
    }
    
    // Usage example
    void ClassificationExample() {
        // Prepare input data
        int8_t input_data[INPUT_SIZE];
        PrepareInputData(input_data);
        
        // Execute inference
        if (RunInference(input_data, INPUT_SIZE)) {
            // Get result
            float confidence = 0.0f;
            int class_id = GetClassificationResult(&confidence);
            
            // Process result based on class ID
            if (confidence > 0.8f) {  // Confidence threshold
                os_printf("High confidence recognition: Class %d\n", class_id);
                // Execute corresponding action
            }
        }
    }

**5.4.2 Object Detection Result Processing**

For object detection tasks (e.g., gesture detection, object detection):

.. code-block:: cpp

    // Detection result structure
    typedef struct {
        float x, y, w, h;      // Bounding box coordinates and size
        int class_id;          // Class ID
        float confidence;      // Confidence
    } DetectionResult;
    
    // Parse detection results
    int GetDetectionResults(DetectionResult* results, int max_results) {
        TfLiteTensor* output = interpreter->output(0);
        
        float scale = output->params.scale;
        int32_t zero_point = output->params.zero_point;
        
        int result_count = 0;
        
        // Assume output format: [num_detections, 8] (x,y,w,h,score,class1,class2,class3)
        int num_boxes = output->dims->data[0];  // e.g., 2268
        int box_size = output->dims->data[1];   // e.g., 8
        
        for (int i = 0; i < num_boxes && result_count < max_results; i++) {
            int8_t* box_data = &output->data.int8[i * box_size];
            
            // Dequantize confidence
            float score = (box_data[4] - zero_point) * scale;
            
            // Filter by confidence threshold
            if (score > 60.0f) {
                // Dequantize coordinates
                results[result_count].x = (box_data[0] - zero_point) * scale;
                results[result_count].y = (box_data[1] - zero_point) * scale;
                results[result_count].w = (box_data[2] - zero_point) * scale;
                results[result_count].h = (box_data[3] - zero_point) * scale;
                results[result_count].confidence = score;
                
                // Find class with maximum probability
                float max_class_score = -1000.0f;
                int max_class_id = 0;
                
                for (int c = 0; c < 3; c++) {  // Assume 3 classes
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
    
    // Usage example
    void DetectionExample() {
        // Prepare input image
        int8_t image_data[IMAGE_SIZE];
        CaptureImage(image_data);
        
        // Execute inference
        if (RunInference(image_data, IMAGE_SIZE)) {
            // Get detection results
            DetectionResult results[10];
            int num_detections = GetDetectionResults(results, 10);
            
            os_printf("Detected %d objects\n", num_detections);
            
            for (int i = 0; i < num_detections; i++) {
                os_printf("Object %d: Class=%d, Confidence=%.2f, "
                         "Position=(%.1f,%.1f), Size=(%.1f,%.1f)\n",
                         i, results[i].class_id, results[i].confidence,
                         results[i].x, results[i].y,
                         results[i].w, results[i].h);
            }
        }
    }

**5.4.3 Regression Task Result Processing**

For regression tasks (e.g., keypoint detection, pose estimation):

.. code-block:: cpp

    // Get regression results
    bool GetRegressionOutput(float* output_values, int output_size) {
        TfLiteTensor* output = interpreter->output(0);
        
        if (output->bytes != output_size) {
            os_printf("Output size mismatch!\n");
            return false;
        }
        
        float scale = output->params.scale;
        int32_t zero_point = output->params.zero_point;
        
        // Dequantize all output values
        for (int i = 0; i < output_size; i++) {
            output_values[i] = (output->data.int8[i] - zero_point) * scale;
        }
        
        return true;
    }

5.5 Complete Application Example
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**5.5.1 Image Classification Application**

.. code-block:: cpp

    // Image classification task entry
    void ImageClassificationTask(void *arg) {
        // 1. Initialize model
        if (!InitModel()) {
            os_printf("Model initialization failed!\n");
            vTaskDelete(NULL);
            return;
        }
        
        // 2. Inference loop
        while (1) {
            // Get camera image
            int8_t image_buffer[192*192*3];
            if (CaptureImage(image_buffer) == 0) {
                
                // Execute inference
                uint32_t start_time = rtos_get_time();
                
                if (RunInference(image_buffer, sizeof(image_buffer))) {
                    // Get classification result
                    float confidence = 0.0f;
                    int class_id = GetClassificationResult(&confidence);
                    
                    uint32_t end_time = rtos_get_time();
                    
                    // Output result
                    os_printf("Classification result: %d, Confidence: %.2f%%, Time: %ums\n",
                             class_id, confidence * 100.0f, 
                             end_time - start_time);
                }
            }
            
            // Delay
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

**5.5.2 CPU1 Main Function Integration**

.. code-block:: cpp

    // app_main_cpu1.cc
    extern "C" {
    #include <os/os.h>
    #include "modules/pm.h"
    #include "FreeRTOS.h"
    #include "task.h"
    }
    
    // Declare inference task
    extern void ImageClassificationTask(void *arg);
    
    extern "C" void app_main_cpu1(void *arg) {
        os_printf("CPU1 started, AI inference task initializing...\n");
        
        // Set CPU frequency to 480MHz for optimal performance
        bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
        
        // Create AI inference task
        // Stack size: 16KB, Priority: 3
        xTaskCreate(ImageClassificationTask, 
                   "ai_inference", 
                   16*1024, 
                   NULL, 
                   3, 
                   NULL);
        
        // CPU1 main loop
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

6. API Reference
---------------------------------

6.1 Core API
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

**6.1.1 Model Loading**

.. code-block:: cpp

    // Load model
    const tflite::Model* model = tflite::GetModel(model_data);
    
    // Verify model version
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        // Handle version mismatch
    }

**6.1.2 Operator Resolver**

.. code-block:: cpp

    // Create mutable operator resolver (specify operator count)
    tflite::MicroMutableOpResolver<N> op_resolver;
    
    // Add operators
    op_resolver.AddConv2D();                          // Convolution
    op_resolver.AddDepthwiseConv2D();                 // Depthwise separable convolution
    op_resolver.AddFullyConnected();                  // Fully connected
    op_resolver.AddSoftmax();                         // Softmax
    op_resolver.AddReshape();                         // Reshape
    op_resolver.AddPad();                             // Padding
    op_resolver.AddMaxPool2D();                       // Max pooling
    op_resolver.AddAdd();                             // Addition
    op_resolver.AddMul();                             // Multiplication
    
    // Use optimized INT8 operator
    op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());

**6.1.3 Interpreter**

.. code-block:: cpp

    // Create interpreter
    tflite::MicroInterpreter interpreter(
        model,              // Model pointer
        op_resolver,        // Operator resolver
        tensor_arena,       // Tensor arena buffer
        tensor_arena_size   // Arena size
    );
    
    // Allocate tensor memory
    TfLiteStatus status = interpreter.AllocateTensors();
    if (status != kTfLiteOk) {
        // Handle allocation failure
    }
    
    // Get arena usage
    size_t used_bytes = interpreter.arena_used_bytes();

**6.1.4 Input/Output Processing**

.. code-block:: cpp

    // Get input tensor
    TfLiteTensor* input = interpreter.input(0);
    
    // Fill input data
    for (int i = 0; i < input->bytes; ++i) {
        input->data.int8[i] = input_data[i];
    }
    
    // Execute inference
    TfLiteStatus invoke_status = interpreter.Invoke();
    if (invoke_status != kTfLiteOk) {
        // Handle inference failure
    }
    
    // Get output tensor
    TfLiteTensor* output = interpreter.output(0);
    
    // Process quantized output (dequantization)
    float scale = output->params.scale;
    int32_t zero_point = output->params.zero_point;
    for (int i = 0; i < output->bytes; ++i) {
        float value = (output->data.int8[i] - zero_point) * scale;
        // Use dequantized value
    }

7. References
---------------------------------

7.1 Official Documentation
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* TensorFlow Lite Micro Official Documentation: https://www.tensorflow.org/lite/microcontrollers
* TensorFlow Lite Model Optimization: https://www.tensorflow.org/lite/performance/model_optimization
* CMSIS-NN Documentation: https://arm-software.github.io/CMSIS_5/NN/html/index.html

7.2 Example Code
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* Micro Speech Example: ``projects/tflite_micro/micro_speech/``
* Gesture Detection Example: ``projects/tflite_micro/gesture_detection/``
* Rock Paper Scissors Complete Application: See :doc:`../projects/rock_paper_scissors/index`

7.3 Related Components
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

* bk_tflite_micro Component: ``components/bk_tflite_micro/``
* FreeRTOS Task Management
* PSRAM Memory Management
* DVP Camera Interface (for vision applications)


