/*
 * elks/include/arch/necv53.h
 * NEC V53 Internal Peripheral Definitions for ELKS
 */

/* serial, serial-necv53.c */
#define EXT_USART_IRQ_RX  11  /* 外部PIC IR3(IRQ 11): USART RxREADY */
#define EXT_USART_IRQ_TX  12  /* 外部PIC IR4(IRQ 12): USART TxREADY */

/* V53 SCU IRQ */
#define EXT_SCU_IRQ_RX  13  /* 外部PIC IR5(IRQ 13): V53 SCU RxREADY */
#define EXT_SCU_IRQ_TX  14  /* 外部PIC IR6(IRQ 14): V53 SCU TxREADY */

/* μPD71059 (PIC) */
//#define EXT_PIC_IMR     0x00CA  /* 外部スレーブ IMR (Slave) */
//#define EXT_PIC_OCW2    0x00C8  /* 外部スレーブ EOI用 */

/* μPD71051 (USART) */
//#define EXT_USART_DATA    0x00D8
//#define EXT_USART_STATUS  0x00DA
//#define EXT_USART_CTRL    0x00DA  // 8251ではStatusと同一アドレスの書き込み

// =====================================================
// V53 System Control Registers (Fixed at FF00H-FFFFH)
// =====================================================
#define V53_SCTL    0x0FFFE
#define V53_OPSEL   0x0FFFD
#define V53_OPHA    0x0FFFC
#define V53_DULA    0x0FFFB
#define V53_IULA    0x0FFFA
#define V53_TULA    0x0FFF9
#define V53_SULA    0x0FFF8
#define V53_WCY4    0x0FFF6
#define V53_WCY3    0x0FFF5
#define V53_WCY2    0x0FFF4
#define V53_WMB1    0x0FFF3
#define V53_RFC     0x0FFF2
#define V53_SBCR    0x0FFF1
#define V53_TCKS    0x0FFF0
#define V53_WAC     0x0FFED
#define V53_WCY0    0x0FFEC
#define V53_WCY1    0x0FFEB
#define V53_WMB0    0x0FFEA
#define V53_BRC     0x0FFE9
#define V53_BADR    0x0FFE1
#define V53_BSEL    0x0FFE0
#define V53_XAM     0x0FF80
#define V53_PGR     0x0FF00

