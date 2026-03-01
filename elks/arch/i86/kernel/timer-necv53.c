/*
 * NECV53's Integrated Timer/Counter Unit
 *
 * This file contains code used for the embedded NECV53 family only.
 * 
 */

#include <linuxmt/config.h>
#include <arch/io.h>
#include <arch/param.h> /* For the definition of HZ */
#include <arch/necv53.h>
#include <arch/ports.h>

/*
 * Main goal is to have Timer2 generating a 1ms period signal, so
 * calculate the appropriate value of the prescaler counter.
 * Note that 1kHz corresponds to 1ms period. Timer2 is clocked
 * by 1/4 of FCPU.
 *
 * Timer2Interval = (FCPU / 4) / 1kHz
 */
#define TIMER2_INTERVAL (CONFIG_8018X_FCPU * 1000000L / (4 * 1000))

/*
 * With a reference of 1ms (1kHz) coming into Timer1, calculate how
 * many counts are needed to achieve the "HZ" rate (usually 100Hz).
 *
 * Timer1Interval = 1kHz / HZ
 */
#define TIMER1_INTERVAL (1000 / HZ)

void enable_timer_tick(void)
{
    // TCKS: タイマクロック入力選択
    outb(0b00011100, V53_TCKS);  // Timer 1: TCLK端子入力使用

    // V53 TCU Timer 0
    // 1. コントロールレジスタに Mode 3 (LSB/MSB) を設定
    // 0x36 = 00(Counter 0) 11(LSB/MSB) 011(Mode 3) 0(Binary)
    outb(0x36, V53_TMR_CTRL);

    // 2. カウント値を LSB -> MSB の順で書き込む (100Hz設定)
    // 1.2288MHz / 100Hz = 12288 (0x3000)
    outb(0x00, V53_TMR_CNT0); // LSB
    outb(0x30, V53_TMR_CNT0); // MSB
}

void disable_timer_tick(void)
{
    // V53 TCU Timer 0
    // Mode 0 に設定することで、次のカウントがロードされるまで
    // 出力を静止状態（通常は高レベル）にできます
    outb(0x30, V53_TMR_CTRL); // Counter 0, LSB/MSB, Mode 0
}
