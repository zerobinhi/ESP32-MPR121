#include "mpr121.h"

// -------------------------- 硬件参数配置（集中管理，方便移植） --------------------------
#define I2C_MASTER_NUM I2C_NUM_0            // I2C端口号
#define I2C_MASTER_SCL_IO 17                // SCL引脚
#define I2C_MASTER_SDA_IO 18                // SDA引脚
#define I2C_MASTER_FREQ_HZ 100000           // I2C频率
#define MPR121_INT_PIN 4                    // 中断引脚
#define MPR121_I2C_ADDR MPR121_DEFAULT_ADDR // MPR121地址

static const char *TAG = "main";

// -------------------------- 全局资源（仅必要时声明为全局） --------------------------
SemaphoreHandle_t mpr121_semaphore = NULL;     // 触摸中断信号量
i2c_master_bus_handle_t i2c_bus_handle = NULL; // I2C总线句柄
i2c_master_dev_handle_t mpr121_handle = NULL;  // MPR121设备句柄

// -------------------------- 资源清理函数（专业代码必备） --------------------------
static void i2c_master_deinit(void)
{
    if (mpr121_handle != NULL)
    {
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(mpr121_handle));
        mpr121_handle = NULL;
        ESP_LOGI(TAG, "Removed MPR121 I2C device");
    }
    if (i2c_bus_handle != NULL)
    {
        ESP_ERROR_CHECK(i2c_del_master_bus(i2c_bus_handle));
        i2c_bus_handle = NULL;
        ESP_LOGI(TAG, "Deleted I2C master bus");
    }
}

// -------------------------- IRQ中断处理函数（轻量化+IRAM安全） --------------------------
static void IRAM_ATTR mpr121_irq_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    // 仅在IRQ引脚拉低时触发（MPR121中断为低电平有效）
    if (gpio_num == MPR121_INT_PIN && gpio_get_level(MPR121_INT_PIN) == 0)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // 信号量触发（ISR中必须用FromISR版本）
        if (mpr121_semaphore != NULL)
        {
            xSemaphoreGiveFromISR(mpr121_semaphore, &xHigherPriorityTaskWoken);
        }
        // 若有高优先级任务被唤醒，立即切换（多核安全）
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// -------------------------- I2C初始化函数（规范参数+错误检查） --------------------------
static esp_err_t i2c_master_init(void)
{
    i2c_master_bus_config_t i2c_bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,               // 过滤7个时钟周期的毛刺
        .flags.enable_internal_pullup = true, // 启用内部上拉（外部建议加4.7kΩ上拉）
    };

    // 创建I2C总线
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_handle),
        TAG, "Create I2C master bus failed");

    // 配置MPR121从设备
    i2c_device_config_t mpr121_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPR121_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    // 添加MPR121设备到I2C总线
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(i2c_bus_handle, &mpr121_dev_cfg, &mpr121_handle),
        TAG, "Add MPR121 to I2C bus failed");

    ESP_LOGI(TAG, "I2C master init successful (SCL: %d, SDA: %d)",
             I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    return ESP_OK;
}

// -------------------------- GPIO中断配置函数（分离逻辑+规范命名） --------------------------
static esp_err_t mpr121_irq_init(void)
{
    gpio_config_t irq_gpio_cfg = {
        .pin_bit_mask = (1ULL << MPR121_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // MPR121 IRQ为开漏输出，需上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // 下降沿触发（IRQ从高变低时触发）
    };
    gpio_config(&irq_gpio_cfg);

    // 安装中断服务（参数0：不使用共享中断）
    gpio_install_isr_service(0);
    // 添加中断处理函数
    ESP_RETURN_ON_ERROR(
        gpio_isr_handler_add(MPR121_INT_PIN, mpr121_irq_handler, (void *)MPR121_INT_PIN),
        TAG, "Add MPR121 IRQ handler failed");

    // 创建二进制信号量（用于同步中断和主任务）
    mpr121_semaphore = xSemaphoreCreateBinary();
    if (mpr121_semaphore == NULL)
    {
        ESP_LOGE(TAG, "Create MPR121 semaphore failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "MPR121 IRQ init successful (INT pin: %d)", MPR121_INT_PIN);
    return ESP_OK;
}

// -------------------------- 主函数（流程清晰+错误处理） --------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "Starting MPR121 touch demo...");
    esp_err_t err = ESP_OK;

    // 1. 初始化I2C总线
    err = i2c_master_init();
    if (err != ESP_OK)
    {
        goto app_exit; // 初始化失败，跳转到清理流程
    }

    // 2. 初始化MPR121
    err = mpr121_init(); // 启用接近电极和自动配置
    if (err != ESP_OK)
    {
        goto app_exit;
    }

    // 3. 初始化MPR121中断
    err = mpr121_irq_init();
    if (err != ESP_OK)
    {
        goto app_exit;
    }

    // 4. 主循环：等待中断，处理触摸状态
    uint16_t current_touch_status = 0;
    while (1)
    {
        // 等待中断信号量（永久阻塞，直到有触摸事件）
        if (xSemaphoreTake(mpr121_semaphore, portMAX_DELAY) == pdTRUE)
        {
            // 读取触摸状态
            err = mpr121_read_touch(&current_touch_status);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Read touch status failed, skip this event");
                continue;
            }

            // 打印触摸状态（仅在有触摸时打印，减少冗余）
            ESP_LOGI(TAG, "Touch status: 0x%04X", current_touch_status);
            for (int i = 0; i < 12; i++)
            {
                if (current_touch_status & (1 << i))
                {
                    ESP_LOGI(TAG, "→ Electrode %d is touched", i);
                }
            }
        }
    }

app_exit:
    // 清理资源（初始化失败时执行）
    i2c_master_deinit();
    if (mpr121_semaphore != NULL)
    {
        vSemaphoreDelete(mpr121_semaphore);
        mpr121_semaphore = NULL;
    }
    ESP_LOGW(TAG, "MPR121 demo exited with error: %s", esp_err_to_name(err));
}
