#include <assert.h>
#include <string.h>

#include "../src/tuya_protocol.h"

static void test_pack_unpack_55aa_plain(void) {
    const uint8_t payload[] = "{}";
    uint8_t frame[64];
    size_t frame_len = sizeof(frame);
    assert(tuya_protocol_pack_55aa(1, TUYA_CMD_DP_QUERY, payload, sizeof(payload) - 1U,
                                   NULL, frame, &frame_len) == TUYA_OK);

    tuya_header_t header;
    assert(tuya_protocol_parse_header(frame, frame_len, &header) == TUYA_OK);
    assert(header.prefix == TUYA_PREFIX_55AA);
    assert(header.seqno == 1);
    assert(header.cmd == TUYA_CMD_DP_QUERY);

    uint8_t decoded[16];
    size_t decoded_len = sizeof(decoded);
    tuya_message_t msg;
    assert(tuya_protocol_unpack(frame, frame_len, NULL, decoded, &decoded_len, &msg) == TUYA_OK);
    assert(decoded_len == sizeof(payload) - 1U);
    assert(memcmp(decoded, payload, decoded_len) == 0);
}

int main(void) {
    test_pack_unpack_55aa_plain();
    return 0;
}
