#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <arch/csr.h>
#include <arch/timer.h>
#include <arch/plic.h>
#include <kernel/serial.h>

extern void trap_entry();

void trap_setup()
{
    /* Aponta o stvec para o assembly stub alinhado em 4 bytes */
    csr_write(CSR_STVEC, (u64)trap_entry);
}

void hart_irq_enable()
{
    csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void hart_irq_disable()
{
    csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
    u64 status = csr_read_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
    return (status & CSR_SSTATUS_SIE);
}

void hart_irq_restore(u64 flags)
{
    if (flags) {
        csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
    } else {
        csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
    }
}

void handle_irq()
{
    u64 cause = csr_read(CSR_SCAUSE);
    u64 irq = cause & ~TRAP_IRQ_BIT;

    if (irq == 5) {
        /* Supervisor Timer Interrupt */
        timer_irq();
    } else if (irq == 9) {
        /* Supervisor External Interrupt */
        u32 ext_irq = plic_hart_claim_irq(0);
        
        if (ext_irq == IRQ_SERIAL) {
            serial_irq();
        }
        
        if (ext_irq) {
            plic_hart_complete_irq(0, ext_irq);
        }
    } else {
        info("Unknown IRQ: %llu\n", irq);
    }
}

void handle_exception()
{
    u64 cause = csr_read(CSR_SCAUSE);
    u64 epc = csr_read(CSR_SEPC);
    u64 stval = csr_read(CSR_STVAL);
    panic("Exception %llu at SEPC 0x%llx, STVAL 0x%llx\n", cause, epc, stval);
}

void handle_trap()
{
    u64 cause = csr_read(CSR_SCAUSE);
    if (cause & TRAP_IRQ_BIT) {
        handle_irq();
    } else {
        handle_exception();
    }
}