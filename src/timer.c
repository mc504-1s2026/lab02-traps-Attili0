#include <arch/timer.h>
#include <arch/csr.h>
#include <kernel/serial.h>

/* Frequência do clock: 10MHz (conforme definido em tick.h ou instruções) */
#ifndef TIMER_FREQ
#define TIMER_FREQ 10000000ULL
#endif

u64 timer_read()
{
    return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
    csr_set(CSR_SIE, CSR_SIE_STIE);
}

void timer_irq_disable()
{
    csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
    u64 now = timer_read();
    u64 tick_in_future = now + secs * TIMER_FREQ;
    csr_write(CSR_STIMECMP, tick_in_future);
    timer_irq_enable();
}

void timer_irq()
{
    /* Desabilita interrupções de timer futuras definindo stimecmp para MAX_UINT64 */
    csr_write(CSR_STIMECMP, 0xFFFFFFFFFFFFFFFFULL);
    
    /* Imprime o alarme e devolve a prompt pro usuário de maneira clean */
    serial_puts("\nalarm\n> ");
}