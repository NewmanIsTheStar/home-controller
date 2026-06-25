#ifndef SHELL_LONGPOLL_H
#define SHELL_LONGPOLL_H

void shell_print_string(const char* text);
void shell_printf(const char *format, ...);
void pico_send_async_text(const char* text);
void init_shell_backend(void);
void dump_text_buffer(void);
int shell_hex_dump(char *filename);
 
#endif
