#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h> /* Assumindo strncmp presente */

extern int _hartid[];

void kmain()
{
    printk_set_level(LOG_DEBUG);
    info("entered S-mode\n");
    info("booting on hart %d\n", _hartid[0]);
    info("setting up virtual memory...\n");
    vm_init();

    info("enabling traps...\n");
    trap_setup();
    info("enabling timer...\n");
    timer_irq_enable();
    info("enabling serial...\n");
    serial_init();
    serial_irq_enable();

    /* Habilitamos interrupções de fato */
    hart_irq_enable();

    /* Shell initialization */
    char input_buf[256];
    size_t input_pos = 0;
    
    serial_puts("\n> ");

    while (1) {
        char chunk[256];
        size_t bytes = serial_read(chunk);

        for (size_t i = 0; i < bytes; i++) {
            char c = chunk[i];

            /* Carriage Return (\r == 0x0D) detectado */
            if (c == '\r') {
                serial_puts("\n");            
                input_buf[input_pos] = '\0';
                
                /* Lógica de Comandos */
                if (strncmp(input_buf, "uptime", 6) == 0 && (input_buf[6] == ' ' || input_buf[6] == '\0')) {
                    u64 secs = timer_read() / TIMER_FREQ;
                    
                    /* Converte o inteiro para string manualmente para evitar printk */
                    char num_buf[32];
                    int idx = 0;
                    if (secs == 0) {
                        num_buf[idx++] = '0';
                    } else {
                        u64 temp = secs;
                        while (temp > 0) {
                            num_buf[idx++] = (char)((temp % 10) + '0');
                            temp /= 10;
                        }
                        /* Inverte a string, pois pegamos os dígitos de trás para frente */
                        for (int j = 0; j < idx / 2; j++) {
                            char t = num_buf[j];
                            num_buf[j] = num_buf[idx - 1 - j];
                            num_buf[idx - 1 - j] = t;
                        }
                    }
                    num_buf[idx] = '\0';
                    
                    serial_puts(num_buf);
                    serial_puts("s\n");
                    
                } else if (strncmp(input_buf, "echo ", 5) == 0) {
                    serial_puts(&input_buf[5]);
                    serial_puts("\n");
                    
                } else if (strncmp(input_buf, "alarm ", 6) == 0) {
                    u64 time = 0;
                    char *p = &input_buf[6];
                    while (*p >= '0' && *p <= '9') {
                        time = time * 10 + (*p - '0');
                        p++;
                    }
                    timer_set_alarm(time);
                    
                } else if (input_pos > 0) {
                    serial_puts("Unknown command: ");
                    serial_puts(input_buf);
                    serial_puts("\n");
                }

                input_pos = 0;
                serial_puts("> ");
            
            /* Suporte rudimentar a Backspace (0x7F ou '\b') para não arruinar buffer do lab */
            } else if (c == 127 || c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                    serial_puts("\b \b");
                }
            } else {
                /* Echo e armazenamento do char */
                if (input_pos < sizeof(input_buf) - 1) {
                    input_buf[input_pos++] = c;
                    serial_putc(c); 
                }
            }
        }
    }
}