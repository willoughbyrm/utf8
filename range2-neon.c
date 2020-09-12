/*
 * Process 2x16 bytes in each iteration.
 * Comments removed for brevity. See range-neon.c for details.
 */
#ifdef __aarch64__

#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>

int utf8_naive(const unsigned char *data, int len);

static const uint8_t _first_len_tbl[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 3,
};

static const uint8_t _first_range_tbl[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8,
};

static const uint8_t _range_min_tbl[] = {
    0x00, 0x80, 0x80, 0x80, 0xA0, 0x80, 0x90, 0x80,
    0xC2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
static const uint8_t _range_max_tbl[] = {
    0x7F, 0xBF, 0xBF, 0xBF, 0xBF, 0x9F, 0xBF, 0x8F,
    0xF4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t _range_adjust_tbl[] = {
    2, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0,
};

/* Return 0 on success, -1 on error */
int utf8_range2(const unsigned char *data, int len)
{
    if (len >= 32) {
        uint8x16_t prev_input = vdupq_n_u8(0);
        uint8x16_t prev_first_len = vdupq_n_u8(0);

        const uint8x16_t first_len_tbl = vld1q_u8(_first_len_tbl);
        const uint8x16_t first_range_tbl = vld1q_u8(_first_range_tbl);
        const uint8x16_t range_min_tbl = vld1q_u8(_range_min_tbl);
        const uint8x16_t range_max_tbl = vld1q_u8(_range_max_tbl);
        const uint8x16x2_t range_adjust_tbl = vld2q_u8(_range_adjust_tbl);

        const uint8x16_t const_1 = vdupq_n_u8(1);
        const uint8x16_t const_2 = vdupq_n_u8(2);
        const uint8x16_t const_e0 = vdupq_n_u8(0xE0);

        uint8x16_t error1 = vdupq_n_u8(0);
        uint8x16_t error2 = vdupq_n_u8(0);
        uint8x16_t error3 = vdupq_n_u8(0);
        uint8x16_t error4 = vdupq_n_u8(0);

        while (len >= 32) {
            /******************* two blocks interleaved **********************/

            /* Forces a double load on Clang */
            const uint8x16x2_t input_pair = vld1q_u8_x2(data);
            const uint8x16_t input = input_pair.val[0];
            const uint8x16_t _input = input_pair.val[1];

            const uint8x16_t high_nibbles = vshrq_n_u8(input, 4);
            const uint8x16_t _high_nibbles = vshrq_n_u8(_input, 4);

            const uint8x16_t first_len =
                vqtbl1q_u8(first_len_tbl, high_nibbles);
            const uint8x16_t _first_len =
                vqtbl1q_u8(first_len_tbl, _high_nibbles);

            uint8x16_t range = vqtbl1q_u8(first_range_tbl, high_nibbles);
            uint8x16_t _range = vqtbl1q_u8(first_range_tbl, _high_nibbles);

            range =
                vorrq_u8(range, vextq_u8(prev_first_len, first_len, 15));
            _range =
                vorrq_u8(_range, vextq_u8(first_len, _first_len, 15));

            uint8x16_t tmp1, tmp2, _tmp1, _tmp2;
            tmp1 = vextq_u8(prev_first_len, first_len, 14);
            tmp1 = vqsubq_u8(tmp1, const_1);
            range = vorrq_u8(range, tmp1);

            _tmp1 = vextq_u8(first_len, _first_len, 14);
            _tmp1 = vqsubq_u8(_tmp1, const_1);
            _range = vorrq_u8(_range, _tmp1);

            tmp2 = vextq_u8(prev_first_len, first_len, 13);
            tmp2 = vqsubq_u8(tmp2, const_2);
            range = vorrq_u8(range, tmp2);

            _tmp2 = vextq_u8(first_len, _first_len, 13);
            _tmp2 = vqsubq_u8(_tmp2, const_2);
            _range = vorrq_u8(_range, _tmp2);

            uint8x16_t shift1 = vextq_u8(prev_input, input, 15);
            uint8x16_t pos = vsubq_u8(shift1, const_e0);
            range = vaddq_u8(range, vqtbl2q_u8(range_adjust_tbl, pos));

            uint8x16_t _shift1 = vextq_u8(input, _input, 15);
            uint8x16_t _pos = vsubq_u8(_shift1, const_e0);
            _range = vaddq_u8(_range, vqtbl2q_u8(range_adjust_tbl, _pos));

            uint8x16_t minv = vqtbl1q_u8(range_min_tbl, range);
            uint8x16_t maxv = vqtbl1q_u8(range_max_tbl, range);

            uint8x16_t _minv = vqtbl1q_u8(range_min_tbl, _range);
            uint8x16_t _maxv = vqtbl1q_u8(range_max_tbl, _range);

            error1 = vorrq_u8(error1, vcltq_u8(input, minv));
            error2 = vorrq_u8(error2, vcgtq_u8(input, maxv));

            error3 = vorrq_u8(error3, vcltq_u8(_input, _minv));
            error4 = vorrq_u8(error4, vcgtq_u8(_input, _maxv));

            /************************ next iteration *************************/
            prev_input = _input;
            prev_first_len = _first_len;

            data += 32;
            len -= 32;
        }
        error1 = vorrq_u8(error1, error2);
        error1 = vorrq_u8(error1, error3);
        error1 = vorrq_u8(error1, error4);

        if (vmaxvq_u8(error1))
            return -1;

        uint32_t token4;
        vst1q_lane_u32(&token4, vreinterpretq_u32_u8(prev_input), 3);

        const int8_t *token = (const int8_t *)&token4;
        int lookahead = 0;
        if (token[3] > (int8_t)0xBF)
            lookahead = 1;
        else if (token[2] > (int8_t)0xBF)
            lookahead = 2;
        else if (token[1] > (int8_t)0xBF)
            lookahead = 3;

        data -= lookahead;
        len += lookahead;
    }

    return utf8_naive(data, len);
}

#endif
