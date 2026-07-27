#include <stddef.h>

void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes = destination;
    size_t index;
    for (index = 0; index < count; ++index)
    {
        bytes[index] = (unsigned char)value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *to = destination;
    const unsigned char *from = source;
    size_t index;
    for (index = 0; index < count; ++index)
    {
        to[index] = from[index];
    }
    return destination;
}
