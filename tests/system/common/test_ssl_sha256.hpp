/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2013
   Author(s): Christophe Grosjean, Meng Tan
*/


#include "test_only/test_framework/redemption_unit_tests.hpp"


RED_AUTO_TEST_CASE(TestSslSha256)
{
    uint8_t sig[SslSha256::DIGEST_LENGTH];

    uint8_t data[512] = {
        /* 0000 */ 0x86, 0x79, 0x05, 0x32, 0x6c, 0x24, 0x43, 0x02,  // .y.2l$C.
        /* 0008 */ 0x15, 0xc6, 0x44, 0x58, 0x29, 0x07, 0x8f, 0x73,  // ..DX)..s
        /* 0010 */ 0xda, 0x67, 0x62, 0x9f, 0xa7, 0x6b, 0xaf, 0x23,  // .gb..k.#
        /* 0018 */ 0xa3, 0xc7, 0x6b, 0x2d, 0x53, 0xea, 0xb1, 0xfb,  // ..k-S...
        /* 0020 */ 0x92, 0x19, 0xfa, 0xc5, 0x98, 0x55, 0xfb, 0x34,  // .....U.4
        /* 0028 */ 0x79, 0xec, 0xad, 0x46, 0xaa, 0xe9, 0xe8, 0xcc,  // y..F....
        /* 0030 */ 0x6c, 0x13, 0x08, 0x6b, 0x3a, 0x45, 0xe6, 0xe8,  // l..k:E..
        /* 0038 */ 0x4e, 0x9c, 0x25, 0x6d, 0xe7, 0x79, 0x36, 0xb3,  // N.%m.y6.
        /* 0040 */ 0x76, 0x35, 0x5d, 0x00, 0x54, 0xfc, 0x28, 0xb2,  // v5].T.(.
        /* 0048 */ 0x9e, 0x41, 0xfa, 0xd4, 0xc8, 0x85, 0x57, 0x28,  // .A....W(
        /* 0050 */ 0x89, 0x6a, 0x56, 0xe5, 0x89, 0x79, 0xb5, 0x1b,  // .jV..y..
        /* 0058 */ 0x96, 0x38, 0xe8, 0x54, 0x7b, 0xd4, 0x16, 0xd7,  // .8.T{...
        /* 0060 */ 0xbc, 0x50, 0xdc, 0xfa, 0x3b, 0xb9, 0xdb, 0x3e,  // .P..;..>
        /* 0068 */ 0x34, 0xd2, 0x80, 0xc3, 0xaf, 0x8b, 0xe0, 0xb7,  // 4.......
        /* 0070 */ 0x66, 0x94, 0xfa, 0x95, 0x2b, 0xa7, 0x4a, 0x1c,  // f...+.J.
        /* 0078 */ 0xf1, 0x71, 0x28, 0x55, 0x35, 0x43, 0xeb, 0xd5,  // .q(U5C..
        /* 0080 */ 0xf3, 0xc0, 0x8f, 0x69, 0x32, 0x19, 0x0d, 0x88,  // ...i2...
        /* 0088 */ 0x24, 0x79, 0x9a, 0xba, 0xd0, 0x75, 0xbc, 0x42,  // $y...u.B
        /* 0090 */ 0x84, 0x36, 0x3f, 0x29, 0xa9, 0x07, 0x68, 0x4f,  // .6?)..hO
        /* 0098 */ 0x68, 0x2f, 0xbf, 0x95, 0x9a, 0x75, 0xdd, 0x7c,  // h/...u.|
        /* 00a0 */ 0x30, 0x37, 0xef, 0xed, 0x2f, 0xd9, 0xdd, 0x8f,  // 07../...
        /* 00a8 */ 0x31, 0x34, 0x4a, 0x15, 0xc4, 0x22, 0x76, 0x33,  // 14J.."v3
        /* 00b0 */ 0xa1, 0x60, 0xa2, 0x5b, 0x2e, 0x95, 0xce, 0x41,  // .`.[...A
        /* 00b8 */ 0x98, 0x1a, 0xea, 0xe6, 0x4b, 0x4a, 0x53, 0xc8,  // ....KJS.
        /* 00c0 */ 0x80, 0x8f, 0x94, 0x5e, 0x03, 0xab, 0xc8, 0x72,  // ...^...r
        /* 00c8 */ 0xc8, 0xdd, 0xca, 0x79, 0xf5, 0x78, 0x4e, 0x43,  // ...y.xNC
        /* 00d0 */ 0x32, 0x0d, 0xc8, 0x19, 0x60, 0xe0, 0x19, 0xe0,  // 2...`...
        /* 00d8 */ 0x12, 0xe4, 0xdc, 0x23, 0x65, 0x57, 0x2d, 0xb6,  // ...#eW-.
        /* 00e0 */ 0x19, 0x4b, 0x0f, 0xc1, 0x4a, 0xb9, 0x22, 0x24,  // .K..J."$
        /* 00e8 */ 0x17, 0x2a, 0xad, 0xe7, 0x70, 0xcd, 0xe3, 0x62,  // .*..p..b
        /* 00f0 */ 0xe0, 0x1d, 0x01, 0xf3, 0xc6, 0xbe, 0x80, 0x36,  // .......6
        /* 00f8 */ 0xac, 0x50, 0x5a, 0xb0, 0xa9, 0xf4, 0xd6, 0x13,  // .PZ.....
        /* 0100 */ 0x6c, 0xcf, 0x82, 0xe3, 0x9b, 0xe2, 0xb6, 0x1f,  // l.......
        /* 0108 */ 0xbb, 0xe3, 0x58, 0xfe, 0x8e, 0xdd, 0x3b, 0x27,  // ..X...;'
        /* 0110 */ 0xc7, 0xa5, 0x4f, 0x3b, 0xf6, 0xbb, 0x15, 0x1e,  // ..O;....
        /* 0118 */ 0x65, 0x04, 0x85, 0x45, 0x9e, 0x7c, 0x53, 0x88,  // e..E.|S.
        /* 0120 */ 0x39, 0x87, 0x0d, 0x4d, 0x21, 0xb6, 0x8e, 0x01,  // 9..M!...
        /* 0128 */ 0xc4, 0xc7, 0x6f, 0x7f, 0x57, 0x94, 0xef, 0x2a,  // ..o.W..*
        /* 0130 */ 0x06, 0x02, 0x0a, 0xd5, 0x8d, 0x0c, 0xd9, 0x8d,  // ........
        /* 0138 */ 0xe2, 0x40, 0x3b, 0x99, 0x83, 0x28, 0xd5, 0xef,  // .@;..(..
        /* 0140 */ 0x7c, 0xf4, 0x4b, 0xb8, 0xb8, 0x53, 0xe1, 0x25,  // |.K..S.%
        /* 0148 */ 0x44, 0x38, 0x0d, 0x35, 0xca, 0x4f, 0xde, 0x08,  // D8.5.O..
        /* 0150 */ 0xc6, 0xdc, 0x5f, 0x16, 0xc6, 0x90, 0x24, 0x4e,  // .._...$N
        /* 0158 */ 0xd1, 0x39, 0x32, 0x29, 0x35, 0xf7, 0x55, 0x1c,  // .92)5.U.
        /* 0160 */ 0x58, 0xe5, 0x73, 0x24, 0x7d, 0xc5, 0x17, 0xc0,  // X.s$}...
        /* 0168 */ 0x6f, 0xf6, 0x30, 0x9e, 0x48, 0x12, 0xf9, 0xe0,  // o.0.H...
        /* 0170 */ 0x5a, 0x7e, 0x00, 0xd8, 0x04, 0x0c, 0x3a, 0x6e,  // Z~....:n
        /* 0178 */ 0xa1, 0xf1, 0x96, 0xa8, 0x27, 0x83, 0x5b, 0xc6,  // ....'.[.
        /* 0180 */ 0x53, 0xd4, 0x46, 0xad, 0x9e, 0x00, 0x87, 0x1e,  // S.F.....
        /* 0188 */ 0x20, 0x3c, 0x5e, 0x2e, 0x67, 0x56, 0xe7, 0x9b,  //  <^.gV..
        /* 0190 */ 0x24, 0x8b, 0x86, 0x25, 0x02, 0x3a, 0x30, 0x18,  // $..%.:0.
        /* 0198 */ 0x90, 0x13, 0x72, 0x56, 0x68, 0xcc, 0xdf, 0x87,  // ..rVh...
        /* 01a0 */ 0x7e, 0xf7, 0x16, 0x99, 0xe1, 0x59, 0x54, 0xc9,  // ~....YT.
        /* 01a8 */ 0x88, 0x4e, 0x97, 0xf1, 0x99, 0x97, 0x06, 0x3c,  // .N.....<
        /* 01b0 */ 0x60, 0x55, 0xeb, 0x5e, 0xd4, 0xc7, 0xba, 0xf9,  // `U.^....
        /* 01b8 */ 0xa3, 0x99, 0x3d, 0xa0, 0xc2, 0x2e, 0x42, 0x3b,  // ..=...B;
        /* 01c0 */ 0x42, 0x24, 0xbe, 0x69, 0x72, 0x87, 0xc3, 0x2c,  // B$.ir..,
        /* 01c8 */ 0xaa, 0xc5, 0xdc, 0xb0, 0x61, 0xb0, 0x6c, 0xd0,  // ....a.l.
        /* 01d0 */ 0xb9, 0xa8, 0x61, 0x86, 0x2f, 0x86, 0xc5, 0x27,  // ..a./..'
        /* 01d8 */ 0x86, 0xe1, 0x73, 0x41, 0xfc, 0xcf, 0x04, 0x93,  // ..sA....
        /* 01e0 */ 0x07, 0xcd, 0xc2, 0x4e, 0x4b, 0x28, 0xf6, 0xfd,  // ...NK(..
        /* 01e8 */ 0x28, 0x17, 0x62, 0x4b, 0x07, 0x5d, 0x89, 0x24,  // (.bK.].$
        /* 01f0 */ 0xd2, 0x14, 0xbd, 0x13, 0xe8, 0x0a, 0xe9, 0x85,  // ........
        /* 01f8 */ 0x8d, 0x47, 0x39, 0x5c, 0x98, 0xd3, 0x1f, 0x56,  // .G9....V
    };

    {
        SslSha256 sha256;

        sha256.update(make_array_view(data));
        sha256.final(sig);
        //hexdump96_c(sig, sizeof(sig));

        RED_CHECK(
            make_array_view(sig) ==
            "\xc5\x4b\xf3\x03\xb9\x09\xfc\x19"
            "\x1e\x2b\x6e\xf6\x8f\x0d\x7e\xc2"
            "\x25\x48\xb0\x85\x04\xfb\x36\xa8"
            "\xf5\xc4\xca\x7a\x28\x29\x4f\x6f"_av
        );
    }

    {
        SslSha256 sha256;

        sha256.update({data, 128});
        sha256.update({data + 128, 128});
        sha256.update({data + 256, 128});
        sha256.update({data + 384, 128});
        sha256.final(sig);
        //hexdump96_c(sig, sizeof(sig));

        RED_CHECK(
            make_array_view(sig) ==
            "\xc5\x4b\xf3\x03\xb9\x09\xfc\x19"
            "\x1e\x2b\x6e\xf6\x8f\x0d\x7e\xc2"
            "\x25\x48\xb0\x85\x04\xfb\x36\xa8"
            "\xf5\xc4\xca\x7a\x28\x29\x4f\x6f"_av
        );
    }

}

RED_AUTO_TEST_CASE(TestSslHmacSHA256)
{
    SslHMAC_Sha256 hmac(cstr_array_view("key"));

    hmac.update(cstr_array_view("The quick brown fox jumps over the lazy dog"));

    uint8_t sig[SslSha256::DIGEST_LENGTH];
    hmac.final(sig);

    RED_CHECK_EQUAL(SslSha256::DIGEST_LENGTH, 32);

    RED_CHECK(
        make_array_view(sig) ==
        "\xf7\xbc\x83\xf4\x30\x53\x84\x24\xb1\x32\x98\xe6\xaa\x6f\xb1\x43"
        "\xef\x4d\x59\xa1\x49\x46\x17\x59\x97\x47\x9d\xbc\x2d\x1a\x3c\xd8"_av
    );
    //hexdump96_c(sig, sizeof(sig));
}

RED_AUTO_TEST_CASE(TestSslHmacSHA256Delayed)
{
    SslHMAC_Sha256_Delayed hmac;

    hmac.init(cstr_array_view("key"));

    hmac.update(cstr_array_view("The quick brown fox jumps over the lazy dog"));

    uint8_t sig[SslSha256::DIGEST_LENGTH];
    hmac.final(sig);

    RED_CHECK_EQUAL(SslSha256::DIGEST_LENGTH, 32);

    RED_CHECK(
        make_array_view(sig) ==
        "\xf7\xbc\x83\xf4\x30\x53\x84\x24\xb1\x32\x98\xe6\xaa\x6f\xb1\x43"
        "\xef\x4d\x59\xa1\x49\x46\x17\x59\x97\x47\x9d\xbc\x2d\x1a\x3c\xd8"_av
    );
    //hexdump96_c(sig, sizeof(sig));
}