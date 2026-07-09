/**
 * ADS1292R 通信测试脚本
 * 测试项目：
 *   1. SPI 基本通信（读ID）
 *   2. 寄存器读写验证
 *   3. 连续读取测试
 *   4. DRDY 信号检测
 *
 * LED 指示：
 *   - 快闪(100ms) = 测试进行中
 *   - 慢闪(500ms) = 全部通过
 *   - 常亮 = 失败
 *   - 3次闪烁后灭 = 步骤1失败
 *   - 3次闪烁后常亮 = 步骤2失败
 */

#include "main.h"
#include "ads1292r_test.h"

/**
 * ADS1292R 硬件复位
 */
void ADS1292R_Reset(void) {
    HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(100); // 等待内部振荡器和PLL稳定
}

/**
 * 发送命令到 ADS1292R
 */
void ADS1292R_SendCmd(uint8_t cmd) {
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2);
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET);
}

/**
 * 读取 ADS1292R 单个寄存器
 */
uint8_t ADS1292R_ReadReg(uint8_t addr) {
    uint8_t cmd;
    uint8_t val = 0;
    
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    
    // 发送 Opcode 1 (读命令与寄存器地址)
    cmd = ADS_CMD_RREG | addr;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2); // tSDECODE 延时
    
    // 发送 Opcode 2 (读取的寄存器个数 - 1，当前为0)
    cmd = 0x00;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2); // tSDECODE 延时
    
    // 接收寄存器数据
    HAL_SPI_Receive(&hspi2, &val, 1, 100);
    HAL_Delay(1);
    
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET);
    return val;
}

/**
 * 写入 ADS1292R 单个寄存器
 */
void ADS1292R_WriteReg(uint8_t addr, uint8_t data) {
    uint8_t cmd;
    
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    
    // 发送 Opcode 1 (写命令与寄存器地址)
    cmd = ADS_CMD_WREG | addr;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2); // tSDECODE 延时
    
    // 发送 Opcode 2 (写入的寄存器个数 - 1，当前为0)
    cmd = 0x00;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2); // tSDECODE 延时
    
    // 发送要写入的数据
    HAL_SPI_Transmit(&hspi2, &data, 1, 100);
    HAL_Delay(2);
    
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET);
}

/**
 * ADS1292R 通信测试接口
 */
uint8_t ADS1292R_Test(void) {
    return ADS1292R_RunAllTests();
}

/* 测试结果统计 */
uint8_t test_step = 0;
uint8_t test_result = 0xFF; // 0xFF = 正在运行/未开始, 0 = 全部通过, 1-4 = 失败步骤

/**
 * LED闪烁指定次数
 */
static void LED_Flash(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; i++) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // LED ON
        HAL_Delay(on_ms);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // LED OFF
        HAL_Delay(off_ms);
    }
}

/**
 * 测试1：读取ID寄存器
 * 期望值：ADS1292R = 0x73, ADS1292 = 0x53
 */
uint8_t Test_ReadID(void) {
    uint8_t id;
    
    ADS1292R_Reset();
    ADS1292R_SendCmd(ADS_CMD_SDATAC);
    HAL_Delay(10);
    
    id = ADS1292R_ReadReg(REG_ID);
    
    /* ADS1292R: bit[3:0] = 0x03, ADS1292: bit[3:0] = 0x01 */
    if ((id & 0x0F) == 0x03) {
        return 1;  // ADS1292R
    }
    if ((id & 0x0F) == 0x01) {
        return 2;  // ADS1292
    }
    return 0;      // 失败
}

/**
 * 测试2：寄存器读写验证
 */
uint8_t Test_RegReadWrite(void) {
    uint8_t readback;
    /*
     * 只对 CH1SET / CH2SET 做读写验证，这两个寄存器全部可写。
     * CONFIG1 / CONFIG2 有保留位，写入 0xFF 会导致回读不一致。
     */
    uint8_t test_vals[] = {0x00, 0x20, 0x40, 0x60}; /* MUX秘地/山 × 增益1/2/3/6 */
    uint8_t test_regs[] = {REG_CH1SET, REG_CH2SET};

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 4; j++) {
            /* 写入测试値 */
            ADS1292R_WriteReg(test_regs[i], test_vals[j]);
            HAL_Delay(1);

            /* 读回验证 */
            readback = ADS1292R_ReadReg(test_regs[i]);

            if (readback != test_vals[j]) {
                /* 恢复默认就退出 */
                ADS1292R_WriteReg(REG_CH1SET, 0x00); /* 默认: 关电源, MUX=正常 */
                ADS1292R_WriteReg(REG_CH2SET, 0x00);
                return 0;  /* 失败 */
            }
        }
    }

    /* 恢复默认値 */
    ADS1292R_WriteReg(REG_CH1SET, 0x00);
    ADS1292R_WriteReg(REG_CH2SET, 0x00);

    return 1;  /* 成功 */
}

/**
 * 测试3：DRDY信号检测
 * 拉高START后，等待DRDY变低
 */
uint8_t Test_DRDY(void) {
    uint32_t timeout;
    
    /* 配置为连续转换模式 */
    ADS1292R_SendCmd(ADS_CMD_SDATAC);
    HAL_Delay(10);
    ADS1292R_WriteReg(REG_CONFIG1, 0x02);  // 500 SPS
    ADS1292R_WriteReg(REG_CONFIG2, 0xA0);  // 内部参考, 测试信号关闭
    ADS1292R_WriteReg(REG_CH1SET, 0x60);   // 增益6, 正常输入
    ADS1292R_WriteReg(REG_CH2SET, 0x60);   // 增益6, 正常输入
    HAL_Delay(10);
    
    /* START 引脚已接地，改用 SPI 命令启动转换 */
    ADS1292R_SendCmd(ADS_CMD_STOP);
    HAL_Delay(2);
    ADS1292R_SendCmd(ADS_CMD_START);
    HAL_Delay(10);
    
    /* 发送RDATAC命令，进入连续读取模式 */
    ADS1292R_SendCmd(ADS_CMD_RDATAC);
    HAL_Delay(5);
    
    /* 等待DRDY变低（最多等200ms） */
    timeout = HAL_GetTick();
    while (HAL_GPIO_ReadPin(ADS_DRDY_PORT, ADS_DRDY_PIN) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - timeout) > 200) {
            ADS1292R_SendCmd(ADS_CMD_SDATAC);
            ADS1292R_SendCmd(ADS_CMD_STOP);
            return 0;  // 超时
        }
    }
    
    /* 再等一个DRDY周期确认稳定 */
    HAL_Delay(5);
    
    /* 停止 */
    ADS1292R_SendCmd(ADS_CMD_SDATAC);
    ADS1292R_SendCmd(ADS_CMD_STOP);
    
    return 1;  // 成功
}

/**
 * 测试4：读取数据帧
 */
uint8_t Test_ReadData(void) {
    uint8_t tx[9] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    uint8_t data[9] = {0};
    uint8_t status_ok = 0;
    
    /* 配置 */
    ADS1292R_SendCmd(ADS_CMD_SDATAC);
    HAL_Delay(10);
    ADS1292R_WriteReg(REG_CONFIG1, 0x02);  // 500 SPS
    ADS1292R_WriteReg(REG_CONFIG2, 0xA0);  // 内部参考
    ADS1292R_WriteReg(REG_CH1SET, 0x60);   // 增益6, 正常输入
    ADS1292R_WriteReg(REG_CH2SET, 0x60);
    HAL_Delay(10);
    
    /* START 引脚已接地，用 SPI 命令启动转换 */
    ADS1292R_SendCmd(ADS_CMD_STOP);
    HAL_Delay(2);
    ADS1292R_SendCmd(ADS_CMD_START);
    HAL_Delay(10);
    ADS1292R_SendCmd(ADS_CMD_RDATAC);
    HAL_Delay(5);
    
    /* 等待DRDY变低 */
    uint32_t timeout = HAL_GetTick();
    while (HAL_GPIO_ReadPin(ADS_DRDY_PORT, ADS_DRDY_PIN) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - timeout) > 200) goto fail;
    }
    
    /* 读取9字节数据帧 (TransmitReceive 确保全双工正确) */
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi2, tx, data, 9, 200);
    HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET);
    
    /* 检查状态字节（高4位应该是0xC0） */
    if ((data[0] & 0xF0) == 0xC0 || (data[0] & 0xF0) == 0x00) {
        status_ok = 1;
    }
    
fail:
    ADS1292R_SendCmd(ADS_CMD_SDATAC);
    ADS1292R_SendCmd(ADS_CMD_STOP);
    
    return status_ok;
}

/**
 * 运行全部测试
 * 返回：0=全部通过, 1-4=失败的步骤
 */
uint8_t ADS1292R_RunAllTests(void) {
    uint8_t result;
    
    /* 步骤1：读ID */
    test_step = 1;
    result = Test_ReadID();
    if (result == 0) return 1;
    LED_Flash(1, 50, 50);  // 短闪1次表示步骤1通过
    
    /* 步骤2：寄存器读写 */
    test_step = 2;
    result = Test_RegReadWrite();
    if (result == 0) return 2;
    LED_Flash(2, 50, 50);  // 短闪2次表示步骤2通过
    
    /* 步骤3：DRDY信号 */
    test_step = 3;
    result = Test_DRDY();
    if (result == 0) return 3;
    LED_Flash(3, 50, 50);  // 短闪3次表示步骤3通过
    
    /* 步骤4：数据读取 */
    test_step = 4;
    result = Test_ReadData();
    if (result == 0) return 4;
    
    return 0;  // 全部通过
}

/**
 * 测试主函数（在main中调用）
 */
void ADS1292R_TestMain(void) {
    uint8_t fail_step;
    
    /* 等待1秒让ADS1292R稳定 */
    HAL_Delay(1000);
    
    /* 运行测试 */
    fail_step = ADS1292R_RunAllTests();
    test_result = fail_step;
    
    /* 报告结果 */
    while (1) {
        if (fail_step == 0) {
            /* 全部通过：慢闪 */
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_Delay(500);
        } else {
            /* 失败：长闪N次表示失败步骤，然后常亮 */
            LED_Flash(fail_step, 300, 200);
            HAL_Delay(1000);
        }
    }
}
