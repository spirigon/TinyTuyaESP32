#include <assert.h>
#include <string.h>

#include "../src/tuya_crypto.h"

static void test_crc32_empty_and_ascii(void) {
    assert(tuya_crc32((const uint8_t *)"", 0) == 0x00000000UL);
    assert(tuya_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926UL);
}

int main(void) {
    test_crc32_empty_and_ascii();
    return 0;
}
