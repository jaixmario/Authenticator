#ifndef TOTP_H
#define TOTP_H

#include <stdint.h>

int totp(uint8_t *key, int len);

#endif