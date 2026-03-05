#pragma once

#include <string.h> // Required for strlen and NULL

    // Example implementation of strlcpy
    size_t strlcpy(char *dst, const char *src, size_t dstsize) {
        size_t src_len = strlen(src);
        //size_t copied = src_len;

        // Check if the destination buffer is too small
        if (dstsize == 0) {
            return src_len; // Nothing can be copied
        }

        // Copy up to dstsize - 1 characters from src to dst
        size_t copy_len = (src_len < dstsize - 1) ? src_len : (dstsize - 1);
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0'; // NUL-terminate the string

        return src_len; // Return the length of the original source string
    }