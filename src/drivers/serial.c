#include <kernel/serial.h>
#include <kernel/panic.h>
#include <arch/plic.h>
#include <arch/spinlock.h>
#include <arch/csr.h>

#define SERIAL_BUF_SIZE 256

struct serialdev {
    char buf[SERIAL_BUF_SIZE];
    size_t len;
    struct spinlock lock;
} dev;

/* Auxiliares de leitura/escrita em memória mapeada da serial */
static inline u8 serial_reg_read(u64 offset) {
    return *(volatile u8 *)(SERIAL_BASE + offset);
}

static inline void serial_reg_write(u64 offset, u8 val) {
    *(volatile u8 *)(SERIAL_BASE + offset) = val;
}

void serial_init()
{
    spin_init(&dev.lock);
    dev.len = 0;

    /* Desabilita todas as interrupções antes da configuração */
    serial_reg_write(SERIAL_IER, 0x00);
    
    /* Habilita FIFOs e limpa TX e RX buffers (FCR) */
    serial_reg_write(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR);
    
    /* Modo 8n1 (LCR) */
    serial_reg_write(SERIAL_LCR, 0x03);

    /* Habilita Interrupção de Received Data Available (RDA) */
    serial_reg_write(SERIAL_IER, SERIAL_IER_ERBFI);
}

void serial_irq_enable()
{
    /* Configura o hardware PLIC para a porta serial (IRQ 0xa) no hart 0 */
    plic_irq_set_priority(IRQ_SERIAL, 1);
    plic_hart_set_threshold(0, 0);
    plic_hart_enable_irq(0, IRQ_SERIAL);
    
    /* Habilita as External Interrupts globais do nível de supervisor */
    csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
    csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_putc(char c)
{
    /* Aguarda até que o Transmit Holding Register Empty flag esteja em 1 */
    while ((serial_reg_read(SERIAL_LSR) & SERIAL_LSR_THRE) == 0) {}
    serial_reg_write(SERIAL_THR, c);
}

void serial_puts(char *str)
{
    while (*str) {
        serial_putc(*str++);
    }
}

void serial_irq()
{
    /* Enquanto houver Data Ready (DTR) no registrador de status */
    while (serial_reg_read(SERIAL_LSR) & SERIAL_LSR_DTR) {
        char c = serial_reg_read(SERIAL_RBR);
        
        /* Protege as modificações no ring buffer */
        u64 flags = spin_lock_irqsave(&dev.lock);
        if (dev.len < SERIAL_BUF_SIZE) {
            dev.buf[dev.len++] = c;
        }
        spin_unlock_irqrestore(&dev.lock, flags);
    }
}

size_t serial_read(char *buf)
{
    u64 flags = spin_lock_irqsave(&dev.lock);
    size_t size = dev.len;
    for (size_t i = 0; i < size; i++) {
        buf[i] = dev.buf[i];
    }
    dev.len = 0;
    spin_unlock_irqrestore(&dev.lock, flags);
    
    return size;
}