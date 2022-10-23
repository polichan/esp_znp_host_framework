## ESP_ZNP_HOST_FRAMEWORK

ESP_ZNP_HOST_FRAMEWORK 是基于 Ti 官方的 [znp_host_framwork](https://git.ti.com/cgit/znp-host-framework/znp-host-framework) 在 ESP-IDF 工具链平台上的移植。

#### Usage


**rpcTransportUart.c** 

implements your own uart transport

```c
/*********************************************************************
 * @fn      rpcTransportWrite
 *
 * @brief   Write to the the serial port to the CC253x.
 *
 * @param   fd - file descriptor of the UART device
 *
 * @return  status
 */
void rpcTransportWrite(uint8_t *buf, uint8_t len)
{
	dbg_print(TAG, PRINT_LEVEL_VERBOSE, "rpcTransportWrite : len = %d", len);
	// @implements Your own uart
	// uart_write((char *)buf, len);
	return;
}
```

**main.c**

```c

#include <stdlib.h>
#include "esp_system.h"
#include "znp.h"
#include "rpc.h"
#include "app.h"

/**
 * @brief ZNP 消息队列处理消息RPC任务
 *
 * @param argument 参数
 */
void znpRpcTask(void *argument);


/**
 * @brief ZNP 任务
 *
 * @param argument 参数
 */
void znpTask(void *argument);

/**
 * @brief ZNP 消息处理进入队列任务
 *
 * @param argument 参数
 */
void znpInMessageTask(void *argument);


void app_main(void)
{
    // 串口初始化，取决于你的板子是怎么设计的
    // 具体初始化方式，参考乐鑫官方文档
    uart_init();

    // 创建 RPC
    if (rpcOpen() == -1)
    {
        return;
    }

    // 创建 ZNP 任务
    xTaskCreate(&znpTask, "znpTask", 8192, NULL, 4, NULL);

    // 创建 RPC 任务
    ESP_LOGI(TAG, "creating RPC thread");
    xTaskCreate(&znpRpcTask, "znpRpcTask", 8192, NULL, 4, NULL);

    // 创建 ZNP 消息队列任务
    xTaskCreate(&znpInMessageTask, "znpInMessageTask", 8192, NULL, 4, NULL);
}

void znpRpcTask(void *argument)
{
    ESP_LOGI(TAG, "rpcTask Started!");
    while (1)
    {
        rpcProcess();
        vTaskDelay(10);
    }
    ESP_LOGE(TAG, "rpcTask exited!");
}


void znpTask(void *argument)
{
    // 初始化 RPC 消息队列
    rpcInitMq();

    // 初始化ZNP应用注册回调
    znpInit();

    vTaskDelete(NULL);
}


void znpInMessageTask(void *argument)
{
    ESP_LOGI(TAG, "appInMessageTask Started!");
    while (1)
    {
        znpMsgProcess(NULL);
        vTaskDelay(10);
    }
}
```