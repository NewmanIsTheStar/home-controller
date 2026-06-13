#ifndef SHELL_LONGPOLL_H
#define SHELL_LONGPOLL_H

void shell_print(const char* text);
void pico_send_async_text(const char* text);
void init_shell_backend(void);
 
#endif
