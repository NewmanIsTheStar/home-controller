#ifndef SHELL_LONGPOLL_H
#define SHELL_LONGPOLL_H

void shell_print_string(const char* text);
void shell_printf(const char *format, ...);
void pico_send_async_text(const char* text);
void init_shell_backend(void);
void dump_text_buffer(void);
int shell_hex_dump(char *filename);
int shell_ping(char *ipv4_string);
int shell_move(char *src_space_dst);
int shell_copy(char *src_space_dst);
 
#endif
