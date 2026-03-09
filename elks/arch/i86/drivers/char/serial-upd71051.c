/*
 * uPD71051 ELKS Serial Driver (use as template for new serial drivers)
 *
 * Supports V53 VME board uPD70151 USART
 * 
 */
#include <linuxmt/config.h>
#include <linuxmt/wait.h>
#include <linuxmt/chqueue.h>
#include <linuxmt/config.h>
#include <linuxmt/sched.h>
#include <linuxmt/errno.h>
#include <linuxmt/mm.h>
#include <linuxmt/ntty.h>
#include <linuxmt/kdev_t.h>
#include <linuxmt/termios.h>
#include <linuxmt/debug.h>
#include <arch/io.h>
#include <arch/irq.h>
#include <arch/necv53.h>
#include <arch/ports.h>		/* definitions of USART/IRQ*/

/* ポート 0x80 を使わない、純粋なソフトウェアディレイ */
static inline void io_delay(void)
{
    int i;
    for (i = 0; i < 50; i++) {
        __asm__ __volatile__ ("nop");
    }
}

static struct tty *tty;

/* printk console out */
void rs_conout(dev_t dev, int c)
{
    //printk("rs_conout() start");
    while((inb(USART_STATUS) & 0x1) == 0);
    outb(c, USART_DATA);
    //printk("rs_conout() end");
}

/* serial write - busy loops until transmit buffer available */
static int rs_write(struct tty *tty)
{
    //printk("rs_write() start %d",tty->outq.len);
    int i = 0;

    while (tty->outq.len > 0) {
	    /* Wait until transmitter hold buffer empty */
        //printk("rs_write() USART_STATUS %d\n",inb(USART_STATUS));
        while((inb(USART_STATUS) & 0x1) == 0);
        //printk("rs_write() TXREADY\n");
	    outb((char)tty_outproc(tty), USART_DATA);
	    i++;
        //printk("rs_write() outq.len %d\n",tty->outq.len);
    }
    //printk("rs_write() end %d",i);
    return i;
}

/* serial interrupt routine, reads and queues received character */
void rs_irq(int irq, struct pt_regs *regs)
{
    //printk("R");
    struct ch_queue *q = &tty->inq;

    /* Read UART status */
    unsigned int status = inb(USART_STATUS);

    // μPD71051のステータスレジスタのビット割り当て
    // Bit 0: TxREADY (送信可能)
    // Bit 1: RxREADY (受信完了)
    // Bit 2: TxEMPTY (送信完了)
    // Bit 3: PE (パリティエラー)
    // Bit 4: OE (オーバーランエラー)
    // Bit 5: FE (フレーミングエラー)
    if (status & 0x38) {   /* Check for parity, framing or overrun errors */
        // discard parity, framing and overrun errors //
        outb(0x37, USART_CTRL); // Clear error flag
        return;
    }

    /* Read received data */
    unsigned char c = inb(USART_DATA);

    if (!tty_intcheck(tty, c)) {
        chq_addch(q, c);
    }
}

/* serial close */
static void rs_release(struct tty *tty)
{
    //printk("rs_release() start\n");
    if (--tty->usecount == 0) {
        // Bit 0 (TXE): 0 で送信禁止
        // Bit 2 (RXE): 0 で受信禁止
        outb(0x00, USART_CTRL);  	/* Disable all interrupts */
	    tty_freeq(tty);
    }
    //printk("rs_release() end\n");
}

/* update UART with current port termios settings*/
static void update_port(struct tty *tty)
{
    unsigned int cflags;	/* use smaller 16-bit width to save code*/
    unsigned divisor;       /* v53 Timer 2 count value */
    flag_t flags;

    /* set baud rate divisor (V53 Timer2 count) based on cflags */
    cflags = tty->termios.c_cflag & CBAUD;
 
    /* 1.2288MHz clock / 16 (USART mode) based divisors */
    switch (cflags) {
        case B38400: divisor = 2;  break;
        case B19200: divisor = 4;  break;
        case B9600:  divisor = 8;  break;
        case B4800:  divisor = 16; break;
        case B2400:  divisor = 32; break;
        case B1200:  divisor = 64; break;
        default:     divisor = 4;  break; // Default to 19200
    }

    /* 1. Set V53 Timer 2 count (Baud Rate Generator) */
	save_flags(flags);
	clr_irq();
    outb(0xb6, V53_TMR_CTRL);          /* Mode 3, LSB/MSB */
    outb((unsigned char)divisor, V53_TMR_CNT2);        /* LSB */
    outb((unsigned char)(divisor >> 8), V53_TMR_CNT2); /* MSB */
	restore_flags(flags);

    /* 2. Reset and Re-initialize uPD71051 USART */
    /* USART requires 0x00 x 4 then 0x40 for reliable internal reset */
    outb(0, USART_CTRL);
    io_delay();
    outb(0, USART_CTRL);
    io_delay();
    outb(0, USART_CTRL);
    io_delay();
    outb(0, USART_CTRL);
    io_delay();
    outb(0x40, USART_CTRL);            /* Internal Reset */
    io_delay();

    /* Set Mode: 8bit, No Parity, 1 Stop, x16 Clock */
    outb(0x4e, USART_CTRL); 
    io_delay();

    /* Set Command: RXE, TXE, Error Reset, DTR ON */
    outb(0x37, USART_CTRL);
    io_delay();

}

/* serial open */
static int rs_open(struct tty *tty)
{
    int err;

    /* increment use count, don't init if already open*/
    if (tty->usecount++)
	    return 0;

    /* FIXME for now, use 80 rather than 1024 RSINQ_SIZE unless SLIP in use */
    err = tty_allocq(tty, 80, RSOUTQ_SIZE);
    if (err) {
	    --tty->usecount;
	    //printk("rs_open() err %d\n",err);
        return err;
    }

    /* restart UART */
    update_port(tty);
  
    return 0;
}

/* initialize UART, interrupts off */
static void rs_init(struct serial_info *sp)
{
    // reset chip //

    // FIFO off, clear RX and TX FIFOs //

    // clear RX register //
}

void INITPROC serial_init(void)
{
    tty = ttys + NR_CONSOLES;

    /* Initial default settings via update_port */
    tty->termios.c_cflag = B19200 | CS8;
    update_port(tty);

    if (request_irq(EXT_USART_IRQ_RX, rs_irq, INT_GENERIC))
       printk("Can't get serial IRQ %d\n", EXT_USART_IRQ_RX);
    else {
       printk("ttyS0 at %x, irq %d\n", USART_DATA, EXT_USART_IRQ_RX);
    }
}

static int rs_ioctl(struct tty *tty, int cmd, char *arg)
{
    switch (cmd) {
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
	    update_port(tty);
	    break;
    default:
	    return -EINVAL;
    }
    return 0;
}

#ifdef CONFIG_BOOTOPTS
/* note: this function may be called prior to serial_init if serial console set*/
void INITPROC rs_setbaud(dev_t dev, unsigned long baud)
{
unsigned int b;
    if (baud == 38400) b = B38400;
    else if (baud == 19200) b = B19200;
    else if (baud == 9600) b = B9600;
    else if (baud == 4800) b = B4800;
    else if (baud == 2400) b = B2400;
    else if (baud == 1200) b = B1200;
    else return;

    tty->termios.c_cflag = b | CS8;
    update_port(tty);
}
#endif

struct tty_ops rs_ops = {
    rs_open,
    rs_release,
    rs_write,
    NULL,
    rs_ioctl,
    rs_conout
};
