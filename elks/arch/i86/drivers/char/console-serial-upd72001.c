/*
 * uPD72001 Headless Console
 *
 * Supports uPD72001 MPSC VME board
 *
 * Uses the internal Serial 0 port as tty console.
 * 
 */

#include <linuxmt/config.h>
#include <linuxmt/errno.h>
#include <linuxmt/kernel.h>
#include <linuxmt/sched.h>
#include <linuxmt/chqueue.h>
#include <linuxmt/ntty.h>
#include <arch/io.h>
#include <arch/irq.h>
#include <arch/necv53.h>
#include <arch/ports.h>
#include "console.h"

struct serial_info {
    const unsigned int  io_cmp;
    const unsigned int  io_ccon;
    const unsigned int  io_sts;
    const unsigned int  io_rxbuf;
    const unsigned int  io_txbuf;
    const unsigned char irq_rx;
    const unsigned char irq_tx;

    /**
     * The baud rate generator is composed of a 15-bit counter 
     * register (BxCNT) and a 15-bit compare register (BxCMP). BxCNT
     * is a free-running counter that is incremented by the baud
     * timebase clock. The baud timebase clock can be either the
     * internal CPU clock or an external clock applied to the BCLK pin.
     * For the Asynchronous Mode 1, BxCMP can be calculated as follows
     * BxCMP = (Fcpu / (Baudrate * 8)) - 1.
     * Note: This driver uses the internal CPU clock.
     */
    unsigned int  baudrate_compare;

    struct tty *tty;
};

/*
// Serial 0 (uPD72001 VME Board)
{
0,                 / Baud Rate register (モニタで設定済のためダミーで設定) /
USART_CTRL,    / Control register /
USART_STATUS,  / Status register /
USART_DATA,    / Receive buffer (Read) /
USART_DATA,    / Transmit buffer (Write) /
EXT_USART_IRQ_RX,  / RX IRQ (11) /
EXT_USART_IRQ_TX,  / TX IRQ (12) /
0,                 / Flags /
&ttys[0]           / Associated TTY /
},
*/
static struct serial_info ports[1] = {
    /* SIO Serial J1 */ { 0, MPSC1_A_CTRL, MPSC1_A_CTRL, MPSC1_A_DATA, MPSC1_A_DATA, EXT_MPSC1_IRQ_RX, EXT_MPSC1_IRQ_TX, 0, &ttys[0] },
    /* SIO Serial J3 * { 0, MPSC2_CTRL, MPSC2_CTRL, MPSC2_DATA, MPSC2_DATA, EXT_MPSC2_IRQ_RX, EXT_MPSC2_IRQ_TX, 0, &ttys[0] }, */
};

/* UART clock baudrate_compares per baud rate */
static const unsigned int baudrate_compares[] = {
    0,  /*  0 = B0      */
    (CONFIG_8018X_FCPU * 1000000UL / (50UL * 8UL)) - 1,     /*  1 = B50     */
    (CONFIG_8018X_FCPU * 1000000UL / (75UL * 8UL)) - 1,     /*  2 = B75     */
    (CONFIG_8018X_FCPU * 1000000UL / (110UL * 8UL)) - 1,    /*  3 = B110    */
    (CONFIG_8018X_FCPU * 1000000UL / (134UL * 8UL)) - 1,    /*  4 = B134    */
    (CONFIG_8018X_FCPU * 1000000UL / (150UL * 8UL)) - 1,    /*  5 = B150    */
    (CONFIG_8018X_FCPU * 1000000UL / (200UL * 8UL)) - 1,    /*  6 = B200    */
    (CONFIG_8018X_FCPU * 1000000UL / (300UL * 8UL)) - 1,    /*  7 = B300    */
    (CONFIG_8018X_FCPU * 1000000UL / (600UL * 8UL)) - 1,    /*  8 = B600    */
    (CONFIG_8018X_FCPU * 1000000UL / (1200UL * 8UL)) - 1,   /*  9 = B1200   */
    (CONFIG_8018X_FCPU * 1000000UL / (1800UL * 8UL)) - 1,   /* 10 = B1800   */
    (CONFIG_8018X_FCPU * 1000000UL / (2400UL * 8UL)) - 1,   /* 11 = B2400   */
    (CONFIG_8018X_FCPU * 1000000UL / (4800UL * 8UL)) - 1,   /* 12 = B4800   */
    (CONFIG_8018X_FCPU * 1000000UL / (9600UL * 8UL)) - 1,   /* 13 = B9600   */
    (CONFIG_8018X_FCPU * 1000000UL / (19200UL * 8UL)) - 1,  /* 14 = B19200  */
    (CONFIG_8018X_FCPU * 1000000UL / (38400UL * 8UL)) - 1,  /* 15 = B38400  */
    (CONFIG_8018X_FCPU * 1000000UL / (57600UL * 8UL)) - 1,  /* 16 = B57600  */
    (CONFIG_8018X_FCPU * 1000000UL / (115200UL * 8UL)) - 1, /* 17 = B115200 */
    (CONFIG_8018X_FCPU * 1000000UL / (230400UL * 8UL)) - 1, /*  0 = B230400 */
};

/* serial receive interrupt routine, reads and queues received character */
void irq_rx(int irq, struct pt_regs *regs)
{
    struct serial_info *sp = &ports[0];

    /* デバッグ用：シリアル受信が来るたびにアスタリスクを表示 */
    //printk("*");

    /* Read UART status */
    unsigned int status = inb(sp->io_sts);

    /* Read received data */
    unsigned char c = inb(sp->io_rxbuf);

    if (!tty_intcheck(sp->tty, c)) {
        chq_addch(&sp->tty->inq, c);
    }
}

/* serial transmit interrupt routine, doesn't do anything */
void irq_tx(int irq, struct pt_regs *regs)
{
}

/* busy-loop and write a character to the UART */
static void serial_putc(const struct serial_info *sp, byte_t c)
{
    /* Test for TXE bit set on the status register */
    while((inb(sp->io_sts) & MPSC_TX_READY) == 0);    // D2: MPSC Tx Buffer Empty?
    /* Write the character */
    outb(c, sp->io_txbuf);
}

/* update UART with current port termios settings */
static void update_port(struct serial_info *port)
{
}

/* ソフトウェアディレイ */
static inline void io_delay(void)
{
    int i;
    for (i = 0; i < 50; i++) {
        __asm__ __volatile__ ("nop");
    }
}

// =============================================================
// mpsc_write_both（MPSC#1と#2のチャンネルAのみ設定）
//   入力: reg = レジスタ番号, val = 書き込み値
// =============================================================
static void mpsc_write_both(int reg, int val)
{
    outb(reg, MPSC1_A_CTRL);
    outb(val, MPSC1_A_CTRL);
    outb(reg, MPSC2_A_CTRL);
    outb(val, MPSC2_A_CTRL);
}

/* Called from main.c! */
void INITPROC console_init(void)
{
    struct serial_info *sp = &ports[0]; /* TODO: add support for Serial 1 */

    /* setup the UART 0 (the only one that has IRQs) */
    request_irq(sp->irq_rx, irq_rx, INT_GENERIC);

    /*
     * The serial port might have been not be configured until this point,
     * so set it up just in case.
     */
    
    /* 2. Reset and Re-initialize uPD72001 MPSC */
    /* MPSC requires 0x00 x 2 then 0x18 for reliable internal reset */
    /* --- MPSC#1 チャンネルA --- */
    outb(0, MPSC1_A_CTRL);      // CR0選択（ポインタリセット）
    outb(0, MPSC1_A_CTRL);      // CR0選択（ポインタリセット）
    outb(0x18, MPSC1_A_CTRL);   // Channel Reset コマンド

    /* --- MPSC#2 チャンネルA --- */
    outb(0, MPSC2_A_CTRL);      // CR0選択（ポインタリセット）
    outb(0, MPSC2_A_CTRL);      // CR0選択（ポインタリセット）
    outb(0x18, MPSC2_A_CTRL);   // Channel Reset コマンド

    /* リセット後安定待ち */
    io_delay();

    outb(1, MPSC1_A_CTRL);      // CR1
    outb(0x10, MPSC1_A_CTRL);   // 受信割り込み可能
    //outb(1, MPSC2_A_CTRL);      // CR1
    //outb(0x10, MPSC2_A_CTRL);   // 受信割り込み可能

    // =========================================================
    // Phase 3: CR2A - 割り込み/DMA設定
    // CR2 = 0xe0
    //  D5=1: ベクタモード
    //  D4-D3=10: 86応答モード
    //  D1-D0=00: 両チャンネルとも割り込み
    //  D2=0: A/BチャネルのPriority Select
    // =========================================================
    mpsc_write_both(2, 0xe0);

    // =========================================================
    // Phase 4: CR4 - 送受信共通フォーマット設定
    // 非同期モード, x16クロック, ストップビット1, パリティなし
    // CR4 = 0x44: D7-D6=00(パリティなし), D5-D4=01(1stop),
    //             D3-D2=00(Async), D1-D0=01(x16)
    // ※ CR4はCR3/CR5より先に設定すること
    // =========================================================
    mpsc_write_both(4, 0x44);
    
    // =========================================================
    // Phase 5: CR12/CR13 - BRGタイムコンスタント設定
    // ボーレート9600bps, CLKソース, x16分周の場合:
    //   カウント値 = 0x001E (30)
    // CR12: タイムコンスタント下位8ビットと上位8ビットを送受信別に書き込む
    // =========================================================
 
    /* MPSC#1 チャンネルA */
    outb(12, MPSC1_A_CTRL);     // CR12選択
    outb(1, MPSC1_A_CTRL);      // 受信BRGレジスタセットモード有効 (D0=1)
    outb(0x1e, MPSC1_A_CTRL);   // 下位バイトの値
    outb(0x00, MPSC1_A_CTRL);   // 上位バイトの値
 
    outb(12, MPSC1_A_CTRL);     // CR12選択
    outb(2, MPSC1_A_CTRL);      // 送信BRGレジスタセットモード有効
    outb(0x1e, MPSC1_A_CTRL);   // 下位バイトの値
    outb(0x00, MPSC1_A_CTRL);   // 上位バイトの値

    /* MPSC#2 チャンネルA */
    outb(12, MPSC2_A_CTRL);     // CR12選択
    outb(1, MPSC2_A_CTRL);      // 受信BRGレジスタセットモード有効 (D0=1)
    outb(0x1e, MPSC2_A_CTRL);   // 下位バイトの値
    outb(0x00, MPSC2_A_CTRL);   // 上位バイトの値

    outb(12, MPSC2_A_CTRL);     // Select CR12
    outb(2, MPSC2_A_CTRL);      // 送信BRGレジスタセットモード有効
    outb(0x1e, MPSC2_A_CTRL);   // 下位バイトの値
    outb(0x00, MPSC2_A_CTRL);   // 上位バイトの値
 
    // =========================================================
    // Phase 6: CR15 - クロックソースとピン機能選択
    // CR15 = 0x56: DPLL入力はBRG, RTSCとTTLCはBRG出力
    // =========================================================
    mpsc_write_both(15, 0x56);
 
    // =========================================================
    // Phase 7: CR14 - BRG動作許可
    // CR14 = 0x07: BRGソース=システムCLK, 送受信BRGカウント有効
    // D2=1(BR CLK=SYS CLK), D1=1(送信BRG有効), D0=1(受信BRG有効)
    // =========================================================
    mpsc_write_both(14, 0x07);

    // =========================================================
    // Phase 8: CR11 - 外部ステータス割り込み要因設定
    // CR11 = 0h : すべての拡張E/S割り込み要因を完全に禁止する
    // =========================================================
    mpsc_write_both(11, 0x00);

    // =========================================================
    // Phase 9: CR3 - 受信設定・イネーブル
    // CR3 = 0xC1: D7-D6=11(8ビット受信), D0=1(受信イネーブル)
    // =========================================================
    mpsc_write_both(3, 0xC1);
 
    // =========================================================
    // Phase 10: CR5 - 送信設定・イネーブル
    // CR5 = 0xEA: D7=1(DTRアサート), D6-D5=11(8ビット送信),
    //            D3=1(送信イネーブル), D1=1(RTSアサート)
    // DTR/RTSを初期からアサートしない場合は 0x68 を使用:
    //   CR5 = 0x68: D6-D5=11(8ビット), D3=1(TX EN), DTR/RTS=0
    // =========================================================
    mpsc_write_both(5, 0xEA);

    // =========================================================
    // Phase 12: チャンネルＢの設定
    // =========================================================
    outb(0x00, MPSC1_B_CTRL);   // ポインタリセット
    outb(0x00, MPSC1_B_CTRL);   // ポインタリセット
    //outb(0x00, MPSC2_B_CTRL);   // ポインタリセット

    outb(0x18, MPSC1_B_CTRL);   // Channel Reset コマンド
    //outb(0x18, MPSC2_B_CTRL);   // Channel Reset コマンド

    // リセット後安定待ち
    io_delay();

    // =========================================================
    // Phase 13: CR2B - 割り込みベクタ設定（チャンネルBで設定必要）
    // ベクタベース = MPSC #1 0x28, MPSC #2 0x30
    // =========================================================
    outb(2, MPSC1_B_CTRL);      // CR2Bの設定
    outb(2, MPSC2_B_CTRL);
        
    outb(0x28, MPSC1_B_CTRL);   // MPSC#1のベクタ設定
    outb(0x30, MPSC2_B_CTRL);   // MPSC#2のベクタ設定

    // =========================================================
    // Phase 14: CR1B - 割り込み/DMA設定
    // CR1B = 0x00: D4-D3=0(受信割り込み完全禁止)
    //             D2=0(固定ベクタ)
    //             D1=0(送信割り込み禁止), D0=0(外部割り込み禁止)
    // =========================================================
    outb(1, MPSC1_B_CTRL);      // CR1Bの設定
    outb(1, MPSC2_B_CTRL);
    
    outb(0x00, MPSC1_B_CTRL);   // ベクタ修飾無効、Bチャネル全割り込み禁止
    outb(0x00, MPSC2_B_CTRL);   // ベクタ修飾無効、Bチャネル全割り込み禁止
 
    // =========================================================
    // Phase 15: CR11B - 外部ステータス割り込み要因設定
    // CR11 = 0h : すべての拡張E/S割り込み要因を完全に禁止する
    // =========================================================
    outb(11, MPSC1_B_CTRL);     // CR11Bの設定
    outb(11, MPSC2_B_CTRL);

    outb(0x00, MPSC1_B_CTRL);   // Bチャネルの外部ステータス割り込み禁止
    outb(0x00, MPSC2_B_CTRL);   // Bチャネルの外部ステータス割り込み禁止

    //request_irq(sp->irq_tx, irq_tx, INT_GENERIC);
    
    /* Set the baudrate, and enable the UART receiver */
    //update_port(sp);

    //printk("console_init: 8018X UART\n");
    //printk("console_init: V53 SCU\n");
    printk("console_init: uPD72001 MPSC\n");
}
 


void bell(void)
{
    serial_putc(&ports[0], 7);	/* send ^G to the Serial 0 */
}

static void sercon_conout(dev_t dev, int Ch)
{
    struct serial_info *sp = &ports[0]; /* TODO: add support for Serial 1 */

    if (Ch == '\n') {
        serial_putc(sp, '\r');
    }
    serial_putc(sp, Ch);
}

static int sercon_ioctl(struct tty *tty, int cmd, char *arg)
{
    //struct serial_info *port = &ports[0]; /* TODO: add support for Serial 1 */
    /*
    switch (cmd) {
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        update_port(port);
    break;

    default:
        return -EINVAL;
    }
    */
    return 0;
}

static int sercon_write(struct tty *tty)
{
    struct serial_info *port = &ports[0]; /* TODO: add support for Serial 1 */
    int cnt = 0;

    while (tty->outq.len > 0) {
        serial_putc(port, (byte_t)tty_outproc(tty));
        cnt++;
    }
    return cnt;
}

static void sercon_release(struct tty *tty)
{
    ttystd_release(tty);
}

static int sercon_open(struct tty *tty)
{
    return ttystd_open(tty);
}

struct tty_ops necv53con_ops = {
    sercon_open,
    sercon_release,
    sercon_write,
    NULL,
    sercon_ioctl,
    sercon_conout
};
