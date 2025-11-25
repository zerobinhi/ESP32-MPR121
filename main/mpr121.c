#include "mpr121.h"

static const char *TAG_MPR = "MPR121";

// -------------------------- 静态辅助函数（仅内部使用） --------------------------
/**
 * @brief 向MPR121指定寄存器写入1字节数据
 * @param reg 寄存器地址（0x00~0x80）
 * @param data 要写入的数据
 * @return esp_err_t ESP_OK: 写入成功；其他: 写入失败
 */
static esp_err_t mpr121_write_reg(uint8_t reg, uint8_t data)
{

    i2c_master_transmit_multi_buffer_info_t buffers[2] = {
        {.write_buffer = &reg, .buffer_size = 1},
        {.write_buffer = &data, .buffer_size = 1},
    };

    esp_err_t err = i2c_master_multi_buffer_transmit(mpr121_handle, buffers, 2, portMAX_DELAY);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_MPR, "Write reg 0x%02X (data 0x%02X) failed: %s",
                 reg, data, esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief 从MPR121指定寄存器读取1字节数据
 * @param reg 寄存器地址（0x00~0x80）
 * @param[out] data 读取到的数据
 * @return esp_err_t ESP_OK: 读取成功；其他: 读取失败
 */
static esp_err_t mpr121_read_reg(uint8_t reg, uint8_t *data)
{
    if (data == NULL)
    {
        ESP_LOGE(TAG_MPR, "Read failed: data buffer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = i2c_master_transmit_receive(
        mpr121_handle,
        &reg, // 发送寄存器地址
        1,    // 地址长度
        data, // 接收数据缓冲区
        1,    // 数据长度
        portMAX_DELAY);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_MPR, "Read reg 0x%02X failed: %s",
                 reg, esp_err_to_name(err));
    }
    return err;
}

// -------------------------- 外部接口实现 --------------------------
esp_err_t mpr121_set_thresholds(uint8_t touch, uint8_t release)
{
    // 检查阈值合理性（释放阈值应小于触摸阈值，避免抖动）
    if (release >= touch)
    {
        ESP_LOGE(TAG_MPR, "Invalid thresholds: release(%d) >= touch(%d)", release, touch);
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < 12; i++)
    {
        // 写入触摸阈值（ELE0~ELE11：0x41,0x43,...0x57）
        ESP_RETURN_ON_ERROR(
            mpr121_write_reg(MPR121_TOUCH_THRESH_0 + i * 2, touch),
            TAG_MPR, "Set touch threshold for ELE%d failed", i);
        // 写入释放阈值（ELE0~ELE11：0x42,0x44,...0x58）
        ESP_RETURN_ON_ERROR(
            mpr121_write_reg(MPR121_RELEASE_THRESH_0 + i * 2, release),
            TAG_MPR, "Set release threshold for ELE%d failed", i);
    }
    return ESP_OK;
}

esp_err_t mpr121_init()
{
    uint8_t tmp;

    // Step 1: 进入待机模式（必须先待机，才能修改配置寄存器）
    ESP_RETURN_ON_ERROR(
        mpr121_write_reg(MPR121_ELE_CFG, 0x00),
        TAG_MPR, "Enter stop mode failed");
    vTaskDelay(pdMS_TO_TICKS(5)); // 等待模式切换稳定

    // Step 2: 软复位（恢复所有寄存器默认值，确保初始化一致性）
    ESP_RETURN_ON_ERROR(
        mpr121_write_reg(MPR121_SOFT_RESET, 0x63),
        TAG_MPR, "Soft reset failed");
    vTaskDelay(pdMS_TO_TICKS(5)); // 等待复位完成

    // Step 3: 配置基线滤波（上升沿：数据>基线时的滤波参数）
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_MHDR, 0x01), TAG_MPR, "Write MHDR failed");
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_NHDR, 0x01), TAG_MPR, "Write NHDR failed");
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_NCLR, 0x00), TAG_MPR, "Write NCLR failed");
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_FDLR, 0x00), TAG_MPR, "Write FDLR failed");

    // Step 4: 配置基线滤波（下降沿：数据<基线时的滤波参数）
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_MHDF, 0x01), TAG_MPR, "Write MHDF failed");
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_NHDF, 0x01), TAG_MPR, "Write NHDF failed");
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_NCLF, 0xFF), TAG_MPR, "Write NCLF failed");
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_FDLF, 0x02), TAG_MPR, "Write FDLF failed");

    // Step 5: 配置触摸/释放阈值（所有电极统一阈值，按需可改为动态配置）
    ESP_RETURN_ON_ERROR(
        mpr121_set_thresholds(0x0F, 0x0A),
        TAG_MPR, "Set thresholds failed");

    // Step 6: 配置滤波全局参数（ESI=2，SFI=0，对应采样间隔4ms，滤波迭代4次）
    ESP_RETURN_ON_ERROR(mpr121_write_reg(MPR121_FILT_CDT_CFG, 0x04), TAG_MPR, "Write FIL_CFG failed");

    // Step 7: 清除初始中断（读取状态寄存器，MPR121会自动拉高IRQ）
    ESP_RETURN_ON_ERROR(mpr121_read_reg(MPR121_TOUCHSTATUS_L, &tmp), TAG_MPR, "Clear IRQ failed (L)");
    ESP_RETURN_ON_ERROR(mpr121_read_reg(MPR121_TOUCHSTATUS_H, &tmp), TAG_MPR, "Clear IRQ failed (H)");

    // Step 8: 启用所有12个电极（ECR=0x0C：ELE_EN=1100，对应ELE0~ELE11全部启用）
    ESP_RETURN_ON_ERROR(
        mpr121_write_reg(MPR121_ELE_CFG, 0x0C),
        TAG_MPR, "Enable all electrodes failed");

    ESP_LOGI(TAG_MPR, "MPR121 init successful");
    return ESP_OK;
}

esp_err_t mpr121_read_touch(uint16_t *touch_status)
{
    if (touch_status == NULL)
    {
        ESP_LOGE(TAG_MPR, "Read touch failed: touch_status is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t touch_l = 0, touch_h = 0;
    // 先读低8位，再读高8位（MPR121支持连续读取，确保数据一致性）
    ESP_RETURN_ON_ERROR(
        mpr121_read_reg(MPR121_TOUCHSTATUS_L, &touch_l),
        TAG_MPR, "Read touch status (L) failed");
    ESP_RETURN_ON_ERROR(
        mpr121_read_reg(MPR121_TOUCHSTATUS_H, &touch_h),
        TAG_MPR, "Read touch status (H) failed");

    *touch_status = (touch_h << 8) | touch_l;
    return ESP_OK;
}

esp_err_t mpr121_read_filtered(uint8_t electrode, uint16_t *filtered_val)
{
    if (filtered_val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (electrode > 11)
    {
        ESP_LOGE(TAG_MPR, "Invalid electrode: %d (must be 0~11)", electrode);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t val_l = 0, val_h = 0;
    // 滤波数据为10位：低8位在0x04+2*i，高2位在0x05+2*i的bit1~bit0
    ESP_RETURN_ON_ERROR(
        mpr121_read_reg(MPR121_FILTDATA_0L + electrode * 2, &val_l),
        TAG_MPR, "Read filtered data (L) for ELE%d failed", electrode);
    ESP_RETURN_ON_ERROR(
        mpr121_read_reg(MPR121_FILTDATA_0L + electrode * 2 + 1, &val_h),
        TAG_MPR, "Read filtered data (H) for ELE%d failed", electrode);

    *filtered_val = (val_h << 8) | val_l;
    return ESP_OK;
}

esp_err_t mpr121_read_baseline(uint8_t electrode, uint8_t *baseline_val)
{
    if (baseline_val == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (electrode > 11)
    {
        ESP_LOGE(TAG_MPR, "Invalid electrode: %d (must be 0~11)", electrode);
        return ESP_ERR_INVALID_ARG;
    }

    return mpr121_read_reg(MPR121_BASELINE_0 + electrode, baseline_val);
}
