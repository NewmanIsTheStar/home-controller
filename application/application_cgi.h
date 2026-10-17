#ifndef APPLICATION_CGI_H
#define APPLICATION_CGI_H
void application_cgi_handler(struct fs_file *file, const char* uri, int iNumParams, 
                       char **pcParam, char **pcValue, void *connection_state); 
#endif
