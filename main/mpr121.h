#ifndef MPR121_H
#define MPR121_H

#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>
#include <stdint.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>

// -------------------------- 可配置参数（集中管理，方便修改） --------------------------
#define MPR121_DEFAULT_ADDR 0x5A // 默认I2C地址（ADD引脚接地）

// 状态寄存器（触摸/超范围状态）
#define MPR121_TOUCHSTATUS_L 0x00 // 触摸状态低8位（ELE0~ELE7：1=触摸，0=释放）
#define MPR121_TOUCHSTATUS_H 0x01 // 触摸状态高8位（D7=OVCF过流标志，D4=ELEPROX接近电极，D3~D0=ELE11~ELE8）
#define MPR121_OORSTATUS_L 0x02   // 超范围状态低8位（ELE0~ELE7：1=超出配置范围）
#define MPR121_OORSTATUS_H 0x03   // 超范围状态高8位（D7=ACFF配置失败，D6=ARFF重配置失败，D4=ELEPROX，D3~D0=ELE11~ELE8）

// 滤波数据寄存器（10位，低8位+高2位）
#define MPR121_FILTDATA_0L 0x04    // ELE0滤波数据低8位
#define MPR121_FILTDATA_0H 0x05    // ELE0滤波数据高2位（bit1~bit0）
#define MPR121_FILTDATA_1L 0x06    // ELE1滤波数据低8位
#define MPR121_FILTDATA_1H 0x07    // ELE1滤波数据高2位
#define MPR121_FILTDATA_2L 0x08    // ELE2滤波数据低8位
#define MPR121_FILTDATA_2H 0x09    // ELE2滤波数据高2位
#define MPR121_FILTDATA_3L 0x0A    // ELE3滤波数据低8位
#define MPR121_FILTDATA_3H 0x0B    // ELE3滤波数据高2位
#define MPR121_FILTDATA_4L 0x0C    // ELE4滤波数据低8位
#define MPR121_FILTDATA_4H 0x0D    // ELE4滤波数据高2位
#define MPR121_FILTDATA_5L 0x0E    // ELE5滤波数据低8位
#define MPR121_FILTDATA_5H 0x0F    // ELE5滤波数据高2位
#define MPR121_FILTDATA_6L 0x10    // ELE6滤波数据低8位
#define MPR121_FILTDATA_6H 0x11    // ELE6滤波数据高2位
#define MPR121_FILTDATA_7L 0x12    // ELE7滤波数据低8位
#define MPR121_FILTDATA_7H 0x13    // ELE7滤波数据高2位
#define MPR121_FILTDATA_8L 0x14    // ELE8滤波数据低8位
#define MPR121_FILTDATA_8H 0x15    // ELE8滤波数据高2位
#define MPR121_FILTDATA_9L 0x16    // ELE9滤波数据低8位
#define MPR121_FILTDATA_9H 0x17    // ELE9滤波数据高2位
#define MPR121_FILTDATA_10L 0x18   // ELE10滤波数据低8位
#define MPR121_FILTDATA_10H 0x19   // ELE10滤波数据高2位
#define MPR121_FILTDATA_11L 0x1A   // ELE11滤波数据低8位
#define MPR121_FILTDATA_11H 0x1B   // ELE11滤波数据高2位
#define MPR121_FILTDATA_PROXL 0x1C // ELEPROX接近电极滤波数据低8位
#define MPR121_FILTDATA_PROXH 0x1D // ELEPROX接近电极滤波数据高2位

// 基线寄存器（8位，需左移2位与10位滤波数据对比）
#define MPR121_BASELINE_0 0x1E    // ELE0基线值
#define MPR121_BASELINE_1 0x1F    // ELE1基线值
#define MPR121_BASELINE_2 0x20    // ELE2基线值
#define MPR121_BASELINE_3 0x21    // ELE3基线值
#define MPR121_BASELINE_4 0x22    // ELE4基线值
#define MPR121_BASELINE_5 0x23    // ELE5基线值
#define MPR121_BASELINE_6 0x24    // ELE6基线值
#define MPR121_BASELINE_7 0x25    // ELE7基线值
#define MPR121_BASELINE_8 0x26    // ELE8基线值
#define MPR121_BASELINE_9 0x27    // ELE9基线值
#define MPR121_BASELINE_10 0x28   // ELE10基线值
#define MPR121_BASELINE_11 0x29   // ELE11基线值
#define MPR121_BASELINE_PROX 0x2A // ELEPROX接近电极基线值

// 基线滤波配置寄存器（普通电极：上升/下降/触摸状态）
#define MPR121_MHDR 0x2B // 上升沿最大半增量（数据>基线时滤波参数）
#define MPR121_NHDR 0x2C // 上升沿噪声半增量
#define MPR121_NCLR 0x2D // 上升沿噪声计数限制
#define MPR121_FDLR 0x2E // 上升沿滤波延迟计数
#define MPR121_MHDF 0x2F // 下降沿最大半增量（数据<基线时滤波参数）
#define MPR121_NHDF 0x30 // 下降沿噪声半增量
#define MPR121_NCLF 0x31 // 下降沿噪声计数限制
#define MPR121_FDLF 0x32 // 下降沿滤波延迟计数
#define MPR121_NHDT 0x33 // 触摸状态噪声半增量
#define MPR121_NCLT 0x34 // 触摸状态噪声计数限制
#define MPR121_FDLT 0x35 // 触摸状态滤波延迟计数

// 基线滤波配置寄存器（ELEPROX接近电极）
#define MPR121_MHDR_PROX 0x36 // 接近电极上升沿最大半增量
#define MPR121_NHDR_PROX 0x37 // 接近电极上升沿噪声半增量
#define MPR121_NCLR_PROX 0x38 // 接近电极上升沿噪声计数限制
#define MPR121_FDLR_PROX 0x39 // 接近电极上升沿滤波延迟计数
#define MPR121_MHDF_PROX 0x3A // 接近电极下降沿最大半增量
#define MPR121_NHDF_PROX 0x3B // 接近电极下降沿噪声半增量
#define MPR121_NCLF_PROX 0x3C // 接近电极下降沿噪声计数限制
#define MPR121_FDLF_PROX 0x3D // 接近电极下降沿滤波延迟计数
#define MPR121_NHDT_PROX 0x3E // 接近电极触摸状态噪声半增量
#define MPR121_NCLT_PROX 0x3F // 接近电极触摸状态噪声计数限制
#define MPR121_FDLT_PROX 0x40 // 接近电极触摸状态滤波延迟计数

// 触摸/释放阈值寄存器（普通电极）
#define MPR121_TOUCH_THRESH_0 0x41    // ELE0触摸阈值
#define MPR121_RELEASE_THRESH_0 0x42  // ELE0释放阈值
#define MPR121_TOUCH_THRESH_1 0x43    // ELE1触摸阈值
#define MPR121_RELEASE_THRESH_1 0x44  // ELE1释放阈值
#define MPR121_TOUCH_THRESH_2 0x45    // ELE2触摸阈值
#define MPR121_RELEASE_THRESH_2 0x46  // ELE2释放阈值
#define MPR121_TOUCH_THRESH_3 0x47    // ELE3触摸阈值
#define MPR121_RELEASE_THRESH_3 0x48  // ELE3释放阈值
#define MPR121_TOUCH_THRESH_4 0x49    // ELE4触摸阈值
#define MPR121_RELEASE_THRESH_4 0x4A  // ELE4释放阈值
#define MPR121_TOUCH_THRESH_5 0x4B    // ELE5触摸阈值
#define MPR121_RELEASE_THRESH_5 0x4C  // ELE5释放阈值
#define MPR121_TOUCH_THRESH_6 0x4D    // ELE6触摸阈值
#define MPR121_RELEASE_THRESH_6 0x4E  // ELE6释放阈值
#define MPR121_TOUCH_THRESH_7 0x4F    // ELE7触摸阈值
#define MPR121_RELEASE_THRESH_7 0x50  // ELE7释放阈值
#define MPR121_TOUCH_THRESH_8 0x51    // ELE8触摸阈值
#define MPR121_RELEASE_THRESH_8 0x52  // ELE8释放阈值
#define MPR121_TOUCH_THRESH_9 0x53    // ELE9触摸阈值
#define MPR121_RELEASE_THRESH_9 0x54  // ELE9释放阈值
#define MPR121_TOUCH_THRESH_10 0x55   // ELE10触摸阈值
#define MPR121_RELEASE_THRESH_10 0x56 // ELE10释放阈值
#define MPR121_TOUCH_THRESH_11 0x57   // ELE11触摸阈值
#define MPR121_RELEASE_THRESH_11 0x58 // ELE11释放阈值

// 触摸/释放阈值寄存器（ELEPROX接近电极）
#define MPR121_TOUCH_THRESH_PROX 0x59   // ELEPROX触摸阈值
#define MPR121_RELEASE_THRESH_PROX 0x5A // ELEPROX释放阈值

// 去抖动寄存器
#define MPR121_DEBOUNCE 0x5B // D3~D1=DR释放去抖动次数（0~7），D0~D2=DT触摸去抖动次数（0~7）

// 滤波与全局CDC/CDT配置寄存器
#define MPR121_FILT_CDC_CFG 0x5C // D7~D2=CDC全局充电电流（0~63μA），D1~D0=FFI一级滤波采样数（6/10/18/34）
#define MPR121_FILT_CDT_CFG 0x5D // D7~D5=CDT全局充电时间（0~32μs），D4~D2=SFI二级滤波采样数（4/6/10/18），D1~D0=ESI采样间隔（1~128ms）

// 电极配置寄存器（运行模式/基线跟踪）
#define MPR121_ELE_CFG 0x5E // D7~D6=CL基线跟踪控制，D5~D4=ELEPROX使能（0~3=禁用/组合ELE0~ELE11），D3~D0=ELE_EN电极使能（0~15=禁用/ELE0~ELE11）

// 电极充电电流寄存器（0=使用全局CDC，1~63=独立电流μA）
#define MPR121_CDC_0 0x5F    // ELE0充电电流
#define MPR121_CDC_1 0x60    // ELE1充电电流
#define MPR121_CDC_2 0x61    // ELE2充电电流
#define MPR121_CDC_3 0x62    // ELE3充电电流
#define MPR121_CDC_4 0x63    // ELE4充电电流
#define MPR121_CDC_5 0x64    // ELE5充电电流
#define MPR121_CDC_6 0x65    // ELE6充电电流
#define MPR121_CDC_7 0x66    // ELE7充电电流
#define MPR121_CDC_8 0x67    // ELE8充电电流
#define MPR121_CDC_9 0x68    // ELE9充电电流
#define MPR121_CDC_10 0x69   // ELE10充电电流
#define MPR121_CDC_11 0x6A   // ELE11充电电流
#define MPR121_CDC_PROX 0x6B // ELEPROX充电电流

// 电极充电时间寄存器（0=使用全局CDT，1~7=独立时间μs）
#define MPR121_CDT_0_1 0x6C   // D3~D0=CDT0（ELE0），D7~D4=CDT1（ELE1）
#define MPR121_CDT_2_3 0x6D   // D3~D0=CDT2（ELE2），D7~D4=CDT3（ELE3）
#define MPR121_CDT_4_5 0x6E   // D3~D0=CDT4（ELE4），D7~D4=CDT5（ELE5）
#define MPR121_CDT_6_7 0x6F   // D3~D0=CDT6（ELE6），D7~D4=CDT7（ELE7）
#define MPR121_CDT_8_9 0x70   // D3~D0=CDT8（ELE8），D7~D4=CDT9（ELE9）
#define MPR121_CDT_10_11 0x71 // D3~D0=CDT10（ELE10），D7~D4=CDT11（ELE11）
#define MPR121_CDT_PROX 0x72  // CDT_PROX（ELEPROX）

// GPIO控制寄存器（仅ELE4~ELE11支持GPIO功能）
#define MPR121_GPIO_CTRL0 0x73  // GPIO控制寄存器0（ELE11~ELE4的CTL0配置）
#define MPR121_GPIO_CTRL1 0x74  // GPIO控制寄存器1（ELE11~ELE4的CTL1配置）
#define MPR121_GPIO_DATA 0x75   // GPIO数据寄存器（输出=寄存器值，输入=引脚电平）
#define MPR121_GPIO_DIR 0x76    // GPIO方向寄存器（1=输出，0=输入）
#define MPR121_GPIO_EN 0x77     // GPIO使能寄存器（1=启用GPIO，0=高阻）
#define MPR121_GPIO_SET 0x78    // GPIO置位寄存器（写1=置1，写0=无操作）
#define MPR121_GPIO_CLEAR 0x79  // GPIO清零寄存器（写1=置0，写0=无操作）
#define MPR121_GPIO_TOGGLE 0x7A // GPIO翻转寄存器（写1=翻转，写0=无操作）

// 自动配置寄存器
#define MPR121_AUTO_CFG0 0x7B // 自动配置控制0（D7~D6=FFI，D5~D4=RETRY重试次数，D3=BVA基线配置，D1=ARE重配置使能，D0=ACE配置使能）
#define MPR121_AUTO_CFG1 0x7C // 自动配置控制1（D7=SCTS跳过充电时间搜索，D2=OORIE超范围中断使能，D1=ARFIE重配置失败中断使能，D0=ACFIE配置失败中断使能）
#define MPR121_AUTO_USL 0x7D  // 自动配置上限值（电极数据上限）
#define MPR121_AUTO_LSL 0x7E  // 自动配置下限值（电极数据下限）
#define MPR121_AUTO_TL 0x7F   // 自动配置目标值（电极数据目标，需在USL与LSL之间）

// 软复位寄存器
#define MPR121_SOFT_RESET 0x80 // 软复位寄存器（写入0x63触发复位，不影响I2C模块）

// -------------------------- 外部变量声明（仅暴露必要接口） --------------------------
extern i2c_master_dev_handle_t mpr121_handle;

// -------------------------- 函数接口（规范返回值+明确功能） --------------------------
/**
 * @brief 初始化MPR121（含软复位、滤波配置、阈值配置、电极使能）
 * @return esp_err_t ESP_OK: 初始化成功；其他: 初始化失败（如I2C通信错误）
 */
esp_err_t mpr121_init(void);

/**
 * @brief 设置所有电极的触摸/释放阈值
 * @param touch 触摸阈值（0~0xFF，推荐0x04~0x10）
 * @param release 释放阈值（0~0xFF，需小于触摸阈值，推荐0x02~0x08）
 * @return esp_err_t ESP_OK: 配置成功；其他: 配置失败
 */
esp_err_t mpr121_set_thresholds(uint8_t touch, uint8_t release);

/**
 * @brief 读取所有电极的触摸状态
 * @param[out] touch_status 触摸状态（16位：bit0~bit11对应ELE0~ELE11，1=触摸，0=释放）
 * @return esp_err_t ESP_OK: 读取成功；其他: 读取失败
 */
esp_err_t mpr121_read_touch(uint16_t *touch_status);

/**
 * @brief 读取指定电极的滤波后电容数据（10位）
 * @param electrode 电极编号（0~11）
 * @param[out] filtered_val 滤波后数据（0~1023，与电容值成反比）
 * @return esp_err_t ESP_OK: 读取成功；其他: 读取失败（如电极编号无效）
 */
esp_err_t mpr121_read_filtered(uint8_t electrode, uint16_t *filtered_val);

/**
 * @brief 读取指定电极的基线值（8位，需左移2位后与滤波数据对比）
 * @param electrode 电极编号（0~11）
 * @param[out] baseline_val 基线值（0~255）
 * @return esp_err_t ESP_OK: 读取成功；其他: 读取失败（如电极编号无效）
 */
esp_err_t mpr121_read_baseline(uint8_t electrode, uint8_t *baseline_val);

#endif // MPR121_H
