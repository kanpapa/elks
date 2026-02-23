/*
 * 8018X's Integrated Interrupt Controller Unit
 *
 * This file contains code used for the embedded 8018X family only.
 * 
 * 16 May 21 Santiago Hormazabal
 */

#include <linuxmt/config.h>
#include <linuxmt/errno.h>
#include <linuxmt/sched.h>
#include <linuxmt/types.h>

#include <arch/ports.h>
#include <arch/8018x.h>
#include <arch/io.h>
#include <arch/irq.h>

/* プロトタイプ宣言（シリアルドライバ側で static を外したもの） */
extern void irq_rx(int irq, struct pt_regs *regs);

/*
外部PIC (Slave / μPD71059):
    IR0: V53 TCU Timer 0
    IR3: USART RxREADY (μPD71051)
    IR4: USART TxREADY (μPD71051)
    （このINT出力が、マスタのINTP7へ接続）

内部ICU (Master / V53 Internal):
    INTP7: 外部PICからの集約信号
*/


/*
 *  Low level interrupt handling for the X86 8018X platforms
 */

void initialize_irq(void)
{
    /* Mask all interrupt sources on the IMASK register */
    //outw(0xfd, PCB_IMASK);
    // V53
    /* 外部PICを有効にするためマスタのINTP7をオープンにする */
    enable_irq(CASCADE_IRQ);
}

struct irq_logical_map {
    unsigned int irq;           /* logical IRQ from ELKS */
    unsigned int config_word;   /* config word for the Interrupt control register */
    unsigned int pcb_register;  /* interrupt control register on the PCB */
    int irq_vector;             /* CPU IRQ Vector number */
} logical_map[] = {
    /* 8018x: Timer Interrupt unmasked, priority 7, Interrupt type (vector) 18. */
    //{ TIMER_IRQ, 0x7, PCB_TCUCON, CPU_VEC_TIMER1 },
    // V53
    { TIMER_IRQ, 0xfe, V53_ICU_IMR , CPU_VEC_V53_INT39 },
    /**
     * 8018x: Enabling Serial Interrupts will enable the RX and TX IRQs
     * at the same time, this is a CPU limitation.
     * Serial Interrupts unmasked, priority 1, Interrupt type (vector) 20 and 21.
     */
    { EXT_USART_IRQ_RX, 0x1, PCB_SCUCON, CPU_VEC_S0_RX },
    { EXT_USART_IRQ_TX, 0x1, PCB_SCUCON, CPU_VEC_S0_TX },
#if CONFIG_8018X_INT0 + 0 > 0
    { CONFIG_8018X_INT0, 0x6, PCB_I0CON, CPU_VEC_INT0 },
#endif
#if CONFIG_8018X_INT1 + 0 > 0
    { CONFIG_8018X_INT1, 0x6, PCB_I1CON, CPU_VEC_INT1 },
#endif
#if CONFIG_8018X_INT2 + 0 > 0
    { CONFIG_8018X_INT2, 0x6, PCB_I2CON, CPU_VEC_INT2 },
#endif
#if CONFIG_8018X_INT3 + 0 > 0 
    { CONFIG_8018X_INT3, 0x6, PCB_I3CON, CPU_VEC_INT3 },
#endif
#if CONFIG_8018X_INT4 + 0 > 0
    { CONFIG_8018X_INT4, 0x6, PCB_I4CON, CPU_VEC_INT4 },
#endif
};

struct irq_logical_map* get_from_logical_irq(unsigned int irq)
{
    size_t i;
    for (i = 0; i < (sizeof(logical_map)/sizeof(logical_map[0])); ++i)
    {
        if (logical_map[i].irq == irq)
        {
            return &logical_map[i];
        }
    }

    return NULL;
}

void enable_irq(unsigned int irq)
{
    /*
    struct irq_logical_map* map;
    map = get_from_logical_irq(irq);
    if (map) {
        // set the priority mask and clear the MSK bit (bit 4)
        //outw(map->config_word & 0x7, map->pcb_register);
        // V53
        outb(map->config_word, map->pcb_register);
    }
    */
    unsigned char mask;

    mask = ~(1 << (irq & 7));
    if (irq < 8) {
        /* 内部ICU直結デバイスの制御 */
        outb(inb(V53_ICU_IMR) & mask, V53_ICU_IMR);
    } else {
        /* 外部PIC接続デバイス (IRQ 8-15) の制御 */
        outb(inb(EXT_PIC_IMR) & mask, EXT_PIC_IMR);

        /* 親となるINTP7を有効化（一度行えばOKだが安全のため実行） */
        unsigned char master_mask = ~(1 << CASCADE_IRQ);
        outb(inb(V53_ICU_IMR) & master_mask, V53_ICU_IMR);
    }
}

int remap_irq(int irq)
{
    /* no remaps */
    //return irq;
    
    /* V53: IRQ 0-15 をそのまま使用するため、単純に範囲チェックのみ行う */
    if ((unsigned int)irq > 15) return -1;
    return irq;
}

void disable_irq(unsigned int irq)
{
    /*
    struct irq_logical_map* map;
    map = get_from_logical_irq(irq);
    if (map) {
        // set the priority mask and set the MSK bit (bit 4)
        //outw(0x8 | (map->config_word & 0x7), map->pcb_register);
        // V53
        outb(0xff, map->pcb_register);
    }
    */
    
    flag_t flags;
    unsigned char mask = 1 << (irq & 7);

    save_flags(flags);
    clr_irq();
    if (irq < 8) {
        // V53: IRQ 0-15 をそのまま使用するため、単純にマスクビットをセットするだけでOK
        outb(inb(V53_ICU_IMR) | mask, V53_ICU_IMR);
    } else {
        outb(inb(EXT_PIC_IMR) | mask, EXT_PIC_IMR);
    }
    restore_flags(flags);
}

// Get interrupt vector from IRQ
int irq_vector(int irq)
{
    /*
    struct irq_logical_map* map;

    map = get_from_logical_irq(irq);
    if (map) {
        return map->irq_vector;
    }

    return -EINVAL;
    */
    /* V53: ベースを0x20とした連続的なマッピング */
    return irq + 0x20;
}

void v53_external_pic_dispatcher(int irq, struct pt_regs *regs)
{
    unsigned char irr;

    // 1. PICのISR (In-Service Register) を読む ---
    // OCW3: 0000_1011 (0x0B) -> RR=1, RIS=1 (Read ISR)
    // IRR (0x0A): 「今、どのピンに割り込み信号が届いているか」を示します。
    // ISR (0x0B): 「今、どの割り込みをCPUが実行中か」を示します。
    outb(0x0B, EXT_PIC_OCW2);
    irr = inb(EXT_PIC_OCW2);    // AL = IRR

    // 2. 要因判定と分岐 ---
    // タイマー処理の判定 (IR0)
    if (irr & 0x01) {
        timer_tick(TIMER_IRQ, regs);
    }
    // シリアル受信の判定 (IR3)
    if (irr & 0x08) {
        irq_rx(EXT_USART_IRQ_RX, regs);
    }
}
