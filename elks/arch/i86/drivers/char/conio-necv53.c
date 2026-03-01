/*
 * conio API for NEC V53 Headless Console
 *
 * This file contains code used for the embedded NEC V53 family only.
 * 
 */

#include <linuxmt/config.h>
#include <arch/io.h>
#include <arch/irq.h>
#include "conio.h"
#include <arch/ports.h>


/* initialize*/
void conio_init(void)
{
}

/*
 * Poll for console input available.
 * Return nonzero character received else 0 if none ready.
 * Called approximately every ~8/100 seconds.
 */
int conio_poll(void)
{
    /* S0STS bit 0x40 RI (receive interrupt) */
    if (inb(V53_SCU_SST) & 0x2) {
        return inb(V53_SCU_DATA); /* R0BUF */
    }
    return 0;
}

void conio_putc(byte_t c)
{
    while((inb(V53_SCU_SST) & 0x01) == 0);	/* TXREADY */
    outb(c, V53_SCU_DATA);			/* DATABUF */
}
