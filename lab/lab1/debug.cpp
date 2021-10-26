#include "server.h"

void print_debug_htp(HttpHeaderP httpHeader)
{
    puts("httpHeader:");
    puts(httpHeader->method);
    puts(httpHeader->url);
    puts(httpHeader->host);
    puts("httpHeader End");
}
void print_debug_buffer(char *Buffer)
{
    puts("Buffer:");
    puts(Buffer);
    puts("Buffer End");
}
