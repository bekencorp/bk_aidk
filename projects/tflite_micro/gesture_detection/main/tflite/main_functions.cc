/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "main_functions.h"

#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "os/os.h"
#include "os/mem.h"

//#include "arm_math.h"
//#include "arm_nnfunctions.h"


extern "C"{ void * __dso_handle = 0 ;}

// Globals, used for compatibility with Arduino-style sketches.
namespace {
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;

// In order to use optimized tensorflow lite kernels, a signed int8_t quantized
// model is preferred over the legacy unsigned model format. This means that
// throughout this project, input images must be converted from unisgned to
// signed format. The easiest and quickest way to convert from unsigned to
// signed 8-bit integers is to subtract 128 from the unsigned value to get a
// signed value.

// An area of memory to use for input, output, and intermediate arrays.
constexpr int kTensorArenaSize = 360 * 1024;
//alignas(16) static uint8_t tensor_arena[kTensorArenaSize];
}  // namespace

extern unsigned char *data_ptr;
void debug_pin_output(int pinnum, int level)
{
    volatile unsigned int *addr = (volatile unsigned int*)(0x44000400 + (pinnum<<2));
    if(level != 0)
        *addr = 2;
    else
        *addr = 0;
}

// The name of this function is important for Arduino compatibility.
void setup() {

    tflite::InitializeTarget();

    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    model = tflite::GetModel(data_ptr);
    MicroPrintf(
        "Model version %d, schema version %d.\r\n",
        model->version(), TFLITE_SCHEMA_VERSION);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf(
        "Model provided is schema version %d not equal "
        "to supported version %d.\r\n",
        model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    // Pull in only the operation implementations we need.
    // This relies on a complete list of all the ops needed by this graph.

    // NOLINTNEXTLINE(runtime-global-variables)
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

    // Build an interpreter to run the model with.
    // NOLINTNEXTLINE(runtime-global-variables)
    // static tflite::MicroInterpreter static_interpreter(
    //     model, micro_op_resolver, tensor_arena, kTensorArenaSize);

    uint8_t *tensor_arena1 = (uint8_t *)psram_malloc(kTensorArenaSize);
    if (tensor_arena1 == NULL) {
    MicroPrintf("Alloc() failed\r\n");
    return;
    }

    static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena1, kTensorArenaSize);

    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
    MicroPrintf("AllocateTensors() failed\r\n");
    return;
    }

    // Get information about the memory area to use for the model's input.
    input = interpreter->input(0);
}

typedef struct {
    int x1, y1, x2, y2;
    float score;
} Box;

float g_scale;
int32_t g_zero_point;

uint8_t post_process(int8_t *out_data)
{
    int max_boxes_num = 128;
    int boxes_num = 0;
    Box *boxes = NULL;

    boxes = (Box *)malloc(max_boxes_num * sizeof(Box));
    if(boxes == NULL){
        return 0;
    }

    int x1, y1, x2, y2;
    for(int i = 0; i < 2268; i++)
    {

        float score = (out_data[i*8 + 4] - g_zero_point)*g_scale;
        if(score > 62)
        {
            if(boxes_num >= max_boxes_num){
                break;
            }


            int x1, y1, x2, y2;

            int x = (out_data[i*8 + 0] - g_zero_point)*g_scale;
            int y = (out_data[i*8 + 1] - g_zero_point)*g_scale;
            int w = (out_data[i*8 + 2] - g_zero_point)*g_scale;
            int h = (out_data[i*8 + 3] - g_zero_point)*g_scale;
            float paper = (out_data[i*8 + 5] - g_zero_point)*g_scale;
            float rock = (out_data[i*8 + 6] - g_zero_point)*g_scale;
            float scissors = (out_data[i*8 + 7] - g_zero_point)*g_scale;

            x1 = (x - w/2);
            x2 = (x + w/2);
            y1 = (y - h/2);
            y2 = (y + h/2);
            if(x1 < 0) x1 = 0;
            if(y1 < 0) y1 = 0;
            if(x2 > 191) x2 = 191;
            if(y2 > 191) y2 = 191;

            boxes[boxes_num].x1 = x1;
            boxes[boxes_num].y1 = y1;
            boxes[boxes_num].x2 = x2;
            boxes[boxes_num].y2 = y2;
            boxes[boxes_num].score = score;
            boxes_num++;

            //MicroPrintf("score: %.2f, paper: %.2f,rock: %.2f,scissors: %.2f, x1: %d, y1: %d, x2: %d, y2: %d\r\n", score, paper,rock,scissors, x1, y1, x2, y2);

            if (paper > 90)
            {
                MicroPrintf("Paper is detected, x1:%d, y1:%d, x2:%d, y2:%d\r\n", x1, y1, x2, y2);
            }
            else if (rock > 90)
            {
                MicroPrintf("Rock is detected, x1:%d, y1:%d, x2:%d, y2:%d\r\n", x1, y1, x2, y2);
            }
            else if (scissors > 90)
            {
                MicroPrintf("Scissors is detected, x1:%d, y1:%d, x2:%d, y2:%d\r\n", x1, y1, x2, y2);
            }

            break;
        }
    }

    if(boxes){
        free(boxes);
    }

    return boxes_num;
}

// The name of this function is important for Arduino compatibility.
void loop() {
    // Get image from provider.

    //debug_pin_output(52,1);
    TfLiteTensor* output = NULL;
    uint64_t before;
    uint64_t after;

    MicroPrintf("======Start detecting the first gesture======\r\n");
    before = (uint64_t)rtos_get_time();

    if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, input->data.int8, 0)) {
        MicroPrintf("Image capture failed.\r\n");
    }

    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter->Invoke()) {
        MicroPrintf("Invoke failed.\r\n");
    }

    output = interpreter->output(0);
    g_scale = output->params.scale;
    g_zero_point = output->params.zero_point;

    //MicroPrintf("arena_used_bytes = %ld, %ld, %.16f,%ld\r\n", interpreter->arena_used_bytes(),output->bytes,output->params.scale,output->params.zero_point);

    post_process(output->data.int8);
    after = (uint64_t)rtos_get_time();
    MicroPrintf("detection time: %d ms\r\n", (uint32_t)(after - before));

    MicroPrintf("======Start detecting the second gesture======\r\n");
    before = (uint64_t)rtos_get_time();

    if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, input->data.int8, 1)) {
        MicroPrintf("Image capture failed.\r\n");
    }

    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter->Invoke()) {
        MicroPrintf("Invoke failed.\r\n");
    }

    output = interpreter->output(0);
    g_scale = output->params.scale;
    g_zero_point = output->params.zero_point;

    //MicroPrintf("arena_used_bytes = %ld, %ld, %.16f,%ld\r\n", interpreter->arena_used_bytes(),output->bytes,output->params.scale,output->params.zero_point);

    post_process(output->data.int8);
    after = (uint64_t)rtos_get_time();
    MicroPrintf("detection time: %d ms\r\n", (uint32_t)(after - before));

    MicroPrintf("======Start detecting the third gesture======\r\n");
    before = (uint64_t)rtos_get_time();

    if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, input->data.int8, 2)) {
        MicroPrintf("Image capture failed.\r\n");
    }

    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter->Invoke()) {
        MicroPrintf("Invoke failed.\r\n");
    }

    output = interpreter->output(0);
    g_scale = output->params.scale;
    g_zero_point = output->params.zero_point;

    //MicroPrintf("arena_used_bytes = %ld, %ld, %.16f,%ld\r\n", interpreter->arena_used_bytes(),output->bytes,output->params.scale,output->params.zero_point);

    post_process(output->data.int8);
    after = (uint64_t)rtos_get_time();
    MicroPrintf("detection time: %d ms\r\n", (uint32_t)(after - before));
    //debug_pin_output(52,0);
}

