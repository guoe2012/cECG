/**
 * ECG 数据采集模块
 * ADS1292R 心电信号采集与LCD波形显示
 * 针对非接触ECG优化: 高增益 + 带通滤波 + 基线校正
 */

#include "main.h"
#include "ili9488.h"
#include "ecg_acquisition.h"
#include "ads_connect_test.h"
#include "uart_dump.h"
#include <stdio.h>
#include <string.h>


extern SPI_HandleTypeDef hspi2;

/* ============ 模式选择 ============
 * 取消注释下面这行切换到内部测试信号模式:
 *   #define ECG_USE_TEST_SIGNAL
 * 正常ECG采集时注释掉这行
 */
/* #define ECG_USE_TEST_SIGNAL */

volatile int32_t ecg_buffer_ch1[ECG_BUFFER_SIZE];
volatile int32_t ecg_buffer_ch2[ECG_BUFFER_SIZE];
volatile uint32_t ecg_sample_count = 0;
volatile uint8_t ecg_data_ready = 0;
volatile int32_t dbg_ch1_raw = 0;
volatile int32_t dbg_ch2_raw = 0;
volatile uint8_t dbg_status[3] = {0};
volatile uint8_t dbg_regs[12] = {0};

/* === 数据捕获缓冲区 (通过OpenOCD读取内存分析) === */
#define ECG_CAPTURE_SIZE  5000
volatile int32_t cap_buf_raw1[ECG_CAPTURE_SIZE];   /* 原始CH1 */
volatile int32_t cap_buf_flt1[ECG_CAPTURE_SIZE];   /* 滤波后CH1 */
volatile int32_t cap_buf_raw2[ECG_CAPTURE_SIZE];   /* 原始CH2 */
volatile int32_t cap_buf_flt2[ECG_CAPTURE_SIZE];   /* 滤波后CH2 */
volatile uint32_t cap_idx  = 0;   /* 当前写入位置 */
volatile uint32_t cap_done = 0;   /* =1 表示捕获完成，可读取 */

static volatile uint32_t waveform_x = 0;

/* ============ 数字滤波器 (非接触ECG专用) ============ */

/* 500 SPS采样率下的滤波器参数 */
#define FILTER_BYPASS      0  /* 旁路滤波(测试用) */
#define FILTER_ENABLE      1  /* 启用滤波 */

/* 一阶IIR高通滤波器 (截止频率0.5Hz, 500SPS)
 * 用于消除基线漂移
 * 传递函数: H(z) = (1 - z^-1) / (1 - 0.9968 * z^-1)
 */
#define HP_ALPHA  0.975f  /* 高通系数 - 截止频率约2Hz @ 500SPS (快速收敛, 去除呼吸伪迹) */
static float hp_state_ch1 = 0.0f;
static float hp_state_ch2 = 0.0f;

/* 一阶IIR低通滤波器 (截止频率35Hz, 500SPS)
 * α=0.644 → -3dB @ 35Hz，对50Hz有~4dB额外衰减
 * y[n] = α*y[n-1] + (1-α)*x[n]
 */
#define LP_ALPHA  0.80f   /* 截止频率约18Hz @ 500SPS (更激进去噪,保留QRS波群) */
static float lp_state_ch1 = 0.0f;
static float lp_state_ch2 = 0.0f;

/* 50Hz陷波滤波器 (500SPS, 二阶IIR, r=0.9)
 * 正确系数计算:
 *   ω0 = 2π×50/500 = 0.6283 rad, cos(ω0) = 0.809
 *   零点: z = e^(±jω0) → b1 = -2cos(ω0)×b0
 *   极点: z = 0.9×e^(±jω0) → a1 = -2×0.9×cos(ω0) = -1.4562
 *   a2 = r² = 0.81
 *   DC增益归一化: gain=(1+a1+a2)/(2-2cos(ω0))=0.3538/0.382=0.926
 *   b0=b2=0.926, b1=-0.926×1.618=-1.498
 * 差分方程: y=b0x+b1x1+b2x2 - A1*y1 - A2*y2
 */
static float notch_x1_ch1 = 0.0f, notch_x2_ch1 = 0.0f;
static float notch_y1_ch1 = 0.0f, notch_y2_ch1 = 0.0f;
static float notch_x1_ch2 = 0.0f, notch_x2_ch2 = 0.0f;
static float notch_y1_ch2 = 0.0f, notch_y2_ch2 = 0.0f;

#define NOTCH_A1  (-1.4562f)  /* -2*r*cos(ω0) = -2*0.9*0.809 */
#define NOTCH_A2  (0.81f)     /* r² = 0.9² */
#define NOTCH_B0  (0.9262f)   /* DC增益归一化 */
#define NOTCH_B1  (-1.4986f)  /* -2*cos(ω0)*b0 = -1.618*0.9262 */
#define NOTCH_B2  (0.9262f)

/* 基线跟踪 (滑动窗口中值) */
#define BASELINE_WINDOW  250  /* 0.5秒窗口 */
static int32_t baseline_buf_ch1[BASELINE_WINDOW];
static int32_t baseline_buf_ch2[BASELINE_WINDOW];
static uint16_t baseline_idx = 0;
static int32_t baseline_ch1 = 0;
static int32_t baseline_ch2 = 0;

static uint8_t filter_enable = FILTER_ENABLE;  /* 启用滤波器 */

/* ============ 非接触ECG信号质量检测 ============ */
#define ECG_SIGNAL_IDLE     0  /* 无信号(皮肤未靠近) */
#define ECG_SIGNAL_NOISY    1  /* 有信号但噪声太大 */
#define ECG_SIGNAL_VALID    2  /* 有效ECG信号 */
#define ECG_SIGNAL_SATURATE 3  /* 信号饱和(皮肤太近/静电) */

/* 信号幅度阈值: 非接触ECG QRS波群典型幅度范围 */
#define ECG_AMP_MIN       30    /* 滤波后信号RMS低于此值认为无信号(噪声基线约20-50) */
#define ECG_AMP_MAX       50000 /* 滤波后信号高于此值认为饱和 */
#define ECG_QRS_MIN       10    /* R波最小幅度(自动量程后) */
#define ECG_QRS_MAX       5000  /* R波最大幅度 */
#define ECG_AMP_MIN_SQ    ((int64_t)ECG_AMP_MIN * ECG_AMP_MIN)   /* 平方值,用于无sqrt比较 */
#define ECG_QRS_MAX_SQ    ((int64_t)ECG_QRS_MAX * ECG_QRS_MAX)

/* 带通能量估算(滑动窗口RMS, 用于判断ECG信号是否存在) */
#define ENERGY_WINDOW     125   /* 250ms窗口 @ 500SPS */
static float energy_buf_ch1[ENERGY_WINDOW];
static uint16_t energy_idx = 0;
static float energy_sum = 0.0f;   /* sum of squares */
static float energy_rms_sq = 0.0f; /* mean of squares (代替RMS) */
static uint8_t signal_quality = ECG_SIGNAL_IDLE;

/* 高通滤波器 (消除基线漂移)
 * 稳定实现: 用单极点LP估计DC，然后从输入中减去
 *   state[n] = α*state[n-1] + (1-α)*x[n]  ← 低通, |α|<1 绝对稳定
 *   y[n] = x[n] - state[n]                 ← 去DC得高通输出
 * α=0.9968 → 截止频率≈0.5Hz @ 500SPS
 */
static float ecg_highpass(float input, float *state) {
    *state = HP_ALPHA * (*state) + (1.0f - HP_ALPHA) * input;
    return input - (*state);
}

/* 低通滤波器 (消除高频噪声) */
static float ecg_lowpass(float input, float *state) {
    float output = LP_ALPHA * (*state) + (1.0f - LP_ALPHA) * input;
    *state = output;
    return output;
}

/* 50Hz陷波滤波器 */
static float ecg_notch(float input, float *x1, float *x2, float *y1, float *y2) {
    float output = NOTCH_B0 * input + NOTCH_B1 * (*x1) + NOTCH_B2 * (*x2)
                 - NOTCH_A1 * (*y1) - NOTCH_A2 * (*y2);
    *x2 = *x1;
    *x1 = input;
    *y2 = *y1;
    *y1 = output;
    return output;
}

/* 更新基线估计 (使用滑动最小值) */
static void ecg_update_baseline(int32_t ch1, int32_t ch2) {
    baseline_buf_ch1[baseline_idx] = ch1;
    baseline_buf_ch2[baseline_idx] = ch2;
    baseline_idx = (baseline_idx + 1) % BASELINE_WINDOW;

    /* 计算基线 (取最小值作为基线估计) */
    int32_t min1 = baseline_buf_ch1[0];
    int32_t min2 = baseline_buf_ch2[0];
    for (uint16_t i = 1; i < BASELINE_WINDOW; i++) {
        if (baseline_buf_ch1[i] < min1) min1 = baseline_buf_ch1[i];
        if (baseline_buf_ch2[i] < min2) min2 = baseline_buf_ch2[i];
    }
    baseline_ch1 = min1;
    baseline_ch2 = min2;
}

/* 完整滤波流程 */
static void ecg_filter(int32_t raw_ch1, int32_t raw_ch2, int32_t *out_ch1, int32_t *out_ch2) {
    if (filter_enable == FILTER_BYPASS) {
        *out_ch1 = raw_ch1;
        *out_ch2 = raw_ch2;
        return;
    }

    /* 1. 高通滤波 (去除基线漂移) */
    float f_ch1 = ecg_highpass((float)raw_ch1, &hp_state_ch1);
    float f_ch2 = ecg_highpass((float)raw_ch2, &hp_state_ch2);

    /* 2. 低通滤波 (去除高频噪声) */
    f_ch1 = ecg_lowpass(f_ch1, &lp_state_ch1);
    f_ch2 = ecg_lowpass(f_ch2, &lp_state_ch2);

    /* 3. 50Hz陷波 (去除工频干扰) */
    f_ch1 = ecg_notch(f_ch1, &notch_x1_ch1, &notch_x2_ch1, &notch_y1_ch1, &notch_y2_ch1);
    f_ch2 = ecg_notch(f_ch2, &notch_x1_ch2, &notch_x2_ch2, &notch_y1_ch2, &notch_y2_ch2);

    *out_ch1 = (int32_t)f_ch1;
    *out_ch2 = (int32_t)f_ch2;
}

#define CS_LOW()   HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET)
#define RST_LOW()  HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_RESET)
#define RST_HIGH() HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_SET)
#define START_LOW()  HAL_GPIO_WritePin(ADS_START_PORT, ADS_START_PIN, GPIO_PIN_RESET)
#define START_HIGH() HAL_GPIO_WritePin(ADS_START_PORT, ADS_START_PIN, GPIO_PIN_SET)

/* tSDECODE: 4个CLK周期 @ 2MHz MCO ≈ 2µs, 16MHz主频下32个NOP ≈ 2µs */
#define ADS_DELAY_2US() do { for (volatile int _i = 0; _i < 32; _i++) __NOP(); } while(0)

static uint8_t ADS_ReadReg(uint8_t addr) {
    uint8_t cmd;
    uint8_t val = 0;
    CS_LOW();
    ADS_DELAY_2US();
    cmd = ADS_CMD_RREG | addr;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    ADS_DELAY_2US();
    cmd = 0x00;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    ADS_DELAY_2US();
    HAL_SPI_Receive(&hspi2, &val, 1, 100);
    ADS_DELAY_2US();
    CS_HIGH();
    return val;
}

static void ADS_SendCmd(uint8_t cmd) {
    CS_LOW();
    ADS_DELAY_2US();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    ADS_DELAY_2US();
    CS_HIGH();
}

static void ADS_WriteReg(uint8_t addr, uint8_t data) {
    uint8_t cmd;
    CS_LOW();
    ADS_DELAY_2US();
    cmd = ADS_CMD_WREG | addr;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    ADS_DELAY_2US();
    cmd = 0x00;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    ADS_DELAY_2US();
    HAL_SPI_Transmit(&hspi2, &data, 1, 100);
    ADS_DELAY_2US();
    CS_HIGH();
}

/* 修复1: 用 FillRect 替代逐像素绘制，大幅减少SPI事务次数 */
static void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    LCD_FillRect(x, y, w, 1, color);
}

static void LCD_DrawVLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color) {
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    LCD_FillRect(x, y1, 1, y2 - y1 + 1, color);
}

void ECG_Init(void) {
    RST_HIGH();
    HAL_Delay(50);
    RST_LOW();
    HAL_Delay(100);
    RST_HIGH();
    HAL_Delay(2000);  /* 等待2秒: V_BIAS偏置电路(R20/R21+C28)充分稳定 */

    CS_LOW();
    HAL_Delay(1);
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 8; i++) {
        HAL_SPI_Transmit(&hspi2, &dummy, 1, 100);
    }
    HAL_Delay(1);
    CS_HIGH();
    HAL_Delay(10);

    /* 发送RESET命令复位芯片内部状态机 */
    ADS_SendCmd(ADS_CMD_RESET);
    HAL_Delay(50);

    ADS_SendCmd(ADS_CMD_SDATAC);
    HAL_Delay(50);

    /* 回读ID验证SPI通信 */
    dbg_regs[0] = ADS_ReadReg(REG_ID);

    /* 真实ECG模式: CH2接真实电极, CH1接V_BIAS(DC)作为噪声基线 */
    ADS_WriteReg(REG_CONFIG1, 0x02);  /* 连续模式, 500 SPS */

#ifdef ECG_USE_TEST_SIGNAL
    /* === 内部测试信号模式 ===
     * CONFIG2=0xA4: INT_TEST=1, 测试信号1Hz方波, 幅度±1mV
     * CH2SET=0x65:   MUX=101(内部测试信号输入) */
    ADS_WriteReg(REG_CONFIG2, 0xA4);
    ADS_WriteReg(REG_CH1SET, 0x05);   /* 增益6, MUX=101(输入短路) */
    ADS_WriteReg(REG_CH2SET, 0x65);   /* 增益6, MUX=101(测试信号) */
    ADS_WriteReg(REG_RLDSENS, 0x00);  /* 测试模式不需要RLD */
#else
    /* === 正常ECG模式 === */
    /* CONFIG2 = 0xA0:
     *   原理图确认无外部参考节点→ADS1292R使用内部参考
     *   PDB_LOFF_COMP=1, PDB_REFBUF=0(内部参考缓冲关, AVDD供电即可), VREF_4V=1 */
    ADS_WriteReg(REG_CONFIG2, 0xA0);  /* 内部参考, 测试信号OFF */
    ADS_WriteReg(REG_CH1SET, 0x01);   /* 增益6(默认), MUX=001(输入短路=噪声基线) */
    ADS_WriteReg(REG_CH2SET, 0x60);   /* 增益6, MUX=000(正常电极输入=真实ECG) */
    ADS_WriteReg(REG_RLDSENS, 0x58);  /* PDB_RLD=1(开启RLD), RLD2N=1, RLD2P=1 (CH2P+CH2N接入RLD) */
#endif

    ADS_WriteReg(REG_LOFFSENS, 0x00); /* 关闭脱落检测 */
    HAL_Delay(10);

    /* 回读寄存器验证 */
    dbg_regs[1] = ADS_ReadReg(REG_CONFIG1);
    dbg_regs[2] = ADS_ReadReg(REG_CONFIG2); /* 期望 0xA0 */
    dbg_regs[3] = ADS_ReadReg(REG_CH1SET);  /* 期望 0x01 (短路噪声基线) */
    dbg_regs[4] = ADS_ReadReg(REG_CH2SET);  /* 期望 0x60 (真实电极) */
    dbg_regs[5] = ADS_ReadReg(REG_RLDSENS); /* 期望 0x58 (RLD开启,CH2P+CH2N) */
}

void ECG_Start(void) {
    /* STOP -> START -> RDATAC 标准连续转换序列 */
    ADS_SendCmd(ADS_CMD_STOP);
    HAL_Delay(2);
    ADS_SendCmd(ADS_CMD_START);
    HAL_Delay(10);
    ADS_SendCmd(ADS_CMD_RDATAC);
    HAL_Delay(10);
}

void ECG_Stop(void) {
    ADS_SendCmd(ADS_CMD_SDATAC);
    ADS_SendCmd(ADS_CMD_STOP);
}

uint8_t ECG_ReadData(int32_t *ch1, int32_t *ch2) {
    uint8_t tx[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t rx[9] = {0};
    uint32_t timeout;

    /* 等待DRDY变低，表示新数据就绪 */
    timeout = HAL_GetTick();
    while (HAL_GPIO_ReadPin(ADS_DRDY_PORT, ADS_DRDY_PIN) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - timeout) > 100) {  /* 100ms: 容忍LCD绘制期间的延迟 */
            return 0;
        }
    }

    CS_LOW();
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 9, 200);
    CS_HIGH();

    /* ADS1292R数据帧: [Status3bytes][CH1_3bytes][CH2_3bytes] = 9字节 */
    *ch1 = (int32_t)(((uint32_t)rx[3] << 16) | ((uint32_t)rx[4] << 8) | rx[5]);
    if (rx[3] & 0x80) *ch1 |= 0xFF000000;

    *ch2 = (int32_t)(((uint32_t)rx[6] << 16) | ((uint32_t)rx[7] << 8) | rx[8]);
    if (rx[6] & 0x80) *ch2 |= 0xFF000000;

    dbg_ch1_raw = *ch1;
    dbg_ch2_raw = *ch2;
    dbg_status[0] = rx[0];
    dbg_status[1] = rx[1];
    dbg_status[2] = rx[2];

    return 1;
}

/* ECG 信号缩放
 * ADS1292R: VREF=2.4V, 增益=6, 满量程=±0.4V = ±8388607 LSB
 * 真实肢体导联心电: 峰值约 1~2mV @ 电极
 *   → 经增益6 → 6~12mV → ADC值约 ±209715 LSB (1mV)
 * 缩放: 1mV 对应 ~30像素 → 除以 6990
 * 动态调整: 使用滤波后信号，基线已由高通滤波器去除
 */
#define ECG_SCALE_DIV  6990
static int16_t ECG_ScaleToY(int32_t raw, uint16_t center_y, uint16_t height) {
    int16_t scaled = (int16_t)(raw / ECG_SCALE_DIV);
    int16_t y = center_y - scaled;
    int16_t half_h = (int16_t)(height / 2);
    if (y < (int16_t)(center_y - half_h)) y = (int16_t)(center_y - half_h);
    if (y > (int16_t)(center_y + half_h - 1)) y = (int16_t)(center_y + half_h - 1);
    return y;
}

void ECG_DisplayWaveform(void) {
    uint16_t ch1_center = 125;
    uint16_t ch2_center = 255;

    LCD_FillScreen(COLOR_BLACK);
    LCD_DrawString(10, 5,   "CH1(NOISE):", COLOR_DARKGRAY, COLOR_BLACK, 2);
    LCD_DrawString(10, 165, "CH2(ECG):", COLOR_YELLOW, COLOR_BLACK, 2);
    LCD_DrawHLine(0, ch1_center, LCD_WIDTH, COLOR_DARKGRAY);
    LCD_DrawHLine(0, ch2_center, LCD_WIDTH, COLOR_DARKGRAY);
}

void ECG_Run(void) {
    int32_t ch1, ch2;
    int32_t flt1, flt2;     /* 滤波后值 */
    int16_t y1, y2;
    uint16_t ch1_center = 125;
    uint16_t ch2_center = 255;
    uint16_t ch_height  = 120;
    int16_t prev_y1 = ch1_center;
    int16_t prev_y2 = ch2_center;
    uint32_t display_update_count = 0;
    char buf[32];

    ECG_Init();
    LCD_Init();
    ECG_DisplayWaveform();
    ECG_UART_Init();    /* 初始化UART数据导出 */
    ECG_Start();

    waveform_x = 0;
    ecg_sample_count = 0;

    /* 预热: 1秒让HP滤波器基线收敛 */
    uint32_t warmup_samples = 500;

    /* 基于滤波后信号的自动量程 (只在有效ECG信号范围内更新) */
    int32_t flt_max = 1, flt_min = -1;
    int32_t scale_div = 5;    /* 初始量程: 非接触ECG信号很弱,从最小量程开始 */
    uint32_t autorange_count = 0;

    uint32_t drdy_fail_count = 0;  /* 连续DRDY超时计数 */

    while (1) {
        if (!ECG_ReadData(&ch1, &ch2)) {
            /* DRDY超时: 只有连续多次超时才认为是真故障, 避免启动瞬间的
             * 一次性超时被永久卡在屏幕上误导判断 */
            drdy_fail_count++;
            if (drdy_fail_count >= 5) {
                LCD_DrawString(0, 0, "DRDY?", COLOR_RED, COLOR_BLACK, 1);
            }
            continue;
        }

        /* 读取成功: 清掉可能残留的DRDY警告 */
        if (drdy_fail_count > 0) {
            LCD_FillRect(0, 0, 40, 10, COLOR_BLACK);
            drdy_fail_count = 0;
        }

        ecg_sample_count++;

        /* 完整滤波链: HP(去基线漂移) → LP(去高频) → 50Hz陷波 */
        ecg_filter(ch1, ch2, &flt1, &flt2);

        /* UART 数据导出 (如果启用) */
        ECG_UART_DumpSample(ch2, flt2);

        /* === 数据捕获: 预热完成后录制 ECG_CAPTURE_SIZE 个点 === */
        if (ecg_sample_count >= warmup_samples && cap_done == 0) {
            if (cap_idx < ECG_CAPTURE_SIZE) {
                cap_buf_raw1[cap_idx] = ch1;
                cap_buf_flt1[cap_idx] = flt1;
                cap_buf_raw2[cap_idx] = ch2;
                cap_buf_flt2[cap_idx] = flt2;
                cap_idx++;
            } else {
                cap_done = 1;  /* 捕获完成, OpenOCD 可以读取了 */
            }
        }

        /* === 调试文字: 每100采样刷新 (采集期间跳过) === */
        display_update_count++;
        if (display_update_count >= 100 && !(cap_done == 0 && ecg_sample_count >= warmup_samples)) {
            display_update_count = 0;
            if (ecg_sample_count < warmup_samples) {
                /* 预热期间显示进度 */
                sprintf(buf, "WARM:%lu/%lu  ", (unsigned long)ecg_sample_count,
                        (unsigned long)warmup_samples);
                LCD_DrawString(10, 140, buf, COLOR_YELLOW, COLOR_BLACK, 1);
            } else {
                /* 清除预热提示 */
                LCD_DrawString(10, 140, "               ", COLOR_BLACK, COLOR_BLACK, 1);
            }
            /* 绿=原始RAW(ch2); 白=滤波后FLT(ch2); 黄=量程SC; 青=峰峰值PP */
            sprintf(buf, "R:%-9ld", (long)ch2);
            LCD_DrawString(255, 5,  buf, COLOR_GREEN,  COLOR_BLACK, 1);
            sprintf(buf, "F:%-9ld", (long)flt2);
            LCD_DrawString(255, 15, buf, COLOR_WHITE,  COLOR_BLACK, 1);
            sprintf(buf, "SC:%-8ld", (long)scale_div);
            LCD_DrawString(255, 25, buf, COLOR_YELLOW, COLOR_BLACK, 1);
            sprintf(buf, "PP:%-8ld", (long)(flt_max - flt_min));
            LCD_DrawString(255, 35, buf, COLOR_CYAN,   COLOR_BLACK, 1);
            sprintf(buf, "N:%-8lu",  (unsigned long)ecg_sample_count);
            LCD_DrawString(255, 45, buf, COLOR_WHITE,  COLOR_BLACK, 1);
            /* 信号质量状态 */
            {
                const char *sq_str;
                uint16_t sq_color;
                switch (signal_quality) {
                    case ECG_SIGNAL_VALID:    sq_str = "ECG OK  "; sq_color = COLOR_GREEN;   break;
                    case ECG_SIGNAL_NOISY:    sq_str = "NOISY   "; sq_color = COLOR_YELLOW;  break;
                    case ECG_SIGNAL_SATURATE: sq_str = "SATURATE"; sq_color = COLOR_RED;     break;
                    default:                  sq_str = "NO SIGNAL"; sq_color = COLOR_DARKGRAY; break;
                }
                LCD_DrawString(255, 55, sq_str, sq_color, COLOR_BLACK, 1);
            }
        }

        /* 预热期只运行滤波器，不绘制波形 */
        if (ecg_sample_count < warmup_samples) continue;

        /* === 信号质量检测 (使用ch2=真实ECG通道) === */
        {
            /* 更新滑动窗口能量 (均方值估算, 避免sqrt) */
            float old_val = energy_buf_ch1[energy_idx];
            float new_val = (float)flt2 * (float)flt2;  /* 使用flt2=真实ECG */
            energy_sum += new_val - old_val;
            energy_buf_ch1[energy_idx] = new_val;
            energy_idx = (energy_idx + 1) % ENERGY_WINDOW;

            float mean_sq = 0.0f;
            if (ecg_sample_count >= ENERGY_WINDOW) {
                mean_sq = energy_sum / ENERGY_WINDOW;
            }
            energy_rms_sq = mean_sq;

            /* 判断信号质量 */
            int32_t abs_flt = (flt2 < 0) ? -flt2 : flt2;  /* 使用flt2 */
            if (abs_flt > ECG_AMP_MAX) {
                signal_quality = ECG_SIGNAL_SATURATE;
            } else if (mean_sq < (float)ECG_AMP_MIN_SQ) {
                signal_quality = ECG_SIGNAL_IDLE;
            } else if (mean_sq > (float)ECG_QRS_MAX_SQ) {
                signal_quality = ECG_SIGNAL_NOISY;
            } else {
                signal_quality = ECG_SIGNAL_VALID;
            }
        }
        /* 自动量程: 跟踪真实峰峰值(ch2=真实ECG), 仅在饱和时暂停 */
        autorange_count++;
        if (signal_quality != ECG_SIGNAL_SATURATE) {
            if (flt2 > flt_max) flt_max = flt2;  /* 使用flt2 */
            if (flt2 < flt_min) flt_min = flt2;  /* 使用flt2 */
        }
        if (autorange_count >= 250) {
            int32_t pp = flt_max - flt_min;
            if (pp > 10) {
                /* 峰峰值占屏幕高度 80%，平滑更新 */
                int32_t new_div = pp / (int32_t)(ch_height * 8 / 10);
                if (new_div < 1) new_div = 1;
                if (new_div > 100) new_div = 100;  /* 防止DC偏移撑爆量程 */
                scale_div = (scale_div + new_div) / 2;
            }
            flt_max = 1;
            flt_min = -1;
            autorange_count = 0;
        }

        /* 缩放到屏幕Y坐标（始终绘制，即使是直线也是信息） */
        {
            int16_t s = (int16_t)(flt1 / scale_div);
            y1 = ch1_center - s;
            if (y1 < (int16_t)(ch1_center - ch_height/2)) y1 = ch1_center - ch_height/2;
            if (y1 > (int16_t)(ch1_center + ch_height/2-1)) y1 = ch1_center + ch_height/2-1;

            s = (int16_t)(flt2 / scale_div);
            y2 = ch2_center - s;
            if (y2 < (int16_t)(ch2_center - ch_height/2)) y2 = ch2_center - ch_height/2;
            if (y2 > (int16_t)(ch2_center + ch_height/2-1)) y2 = ch2_center + ch_height/2-1;
        }

        /* 每4采样刷新一列 (采集期间跳过, 保证采样率) */
        if (cap_done == 0 && ecg_sample_count >= warmup_samples) {
            /* 采集期间: 跳过波形绘制, 保证 500 SPS 采样率 */
        } else if (ecg_sample_count % 4 == 0) {
            LCD_DrawVLine(waveform_x, ch1_center-ch_height/2, ch1_center+ch_height/2-1, 0x1082);
            LCD_DrawVLine(waveform_x, ch2_center-ch_height/2, ch2_center+ch_height/2-1, 0x1082);
            LCD_DrawPixel(waveform_x, y1, COLOR_CYAN);
            LCD_DrawPixel(waveform_x, y2, COLOR_YELLOW);
            if (waveform_x > 0) {
                if (prev_y1 != y1) LCD_DrawVLine(waveform_x-1, prev_y1, y1, COLOR_CYAN);
                if (prev_y2 != y2) LCD_DrawVLine(waveform_x-1, prev_y2, y2, COLOR_YELLOW);
            }
            prev_y1 = y1;
            prev_y2 = y2;
            waveform_x++;
            if (waveform_x >= LCD_WIDTH) {
                waveform_x = 0;
                prev_y1 = ch1_center;
                prev_y2 = ch2_center;
            }
        }
    }
}

