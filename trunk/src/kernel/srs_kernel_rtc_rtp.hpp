/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2020 Winlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SRS_KERNEL_RTC_RTP_HPP
#define SRS_KERNEL_RTC_RTP_HPP

#include <srs_core.hpp>

#include <srs_kernel_buffer.hpp>
#include <srs_kernel_codec.hpp>

#include <string>

class SrsRtpPacket2;

// The RTP packet max size, should never exceed this size.
const int kRtpPacketSize        = 1500;

const int kRtpHeaderFixedSize   = 12;
const uint8_t kRtpMarker        = 0x80;

// H.264 nalu header type mask.
const uint8_t kNalTypeMask      = 0x1F;

// @see: https://tools.ietf.org/html/rfc6184#section-5.2
const uint8_t kStapA            = 24;

// @see: https://tools.ietf.org/html/rfc6184#section-5.2
const uint8_t kFuA              = 28;

// @see: https://tools.ietf.org/html/rfc6184#section-5.8
const uint8_t kStart            = 0x80; // Fu-header start bit
const uint8_t kEnd              = 0x40; // Fu-header end bit


class SrsBuffer;
class SrsRtpRawPayload;
class SrsRtpFUAPayload2;
class SrsSharedPtrMessage;

// TODO: FIXME: Merge with srs_rtp_seq_distance
bool SnCompare(uint16_t current_sn, uint16_t last_sn);
bool SnRollback(uint16_t current_sn, uint16_t last_sn);
int32_t SnDiff(uint16_t current_sn, uint16_t last_sn);

class SrsRtpHeader
{
private:
    uint8_t padding_length;
    bool extension;
    uint8_t cc;
    bool marker;
    uint8_t payload_type;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[15];
    uint16_t extension_length;
    // TODO:extension field.
public:
    SrsRtpHeader();
    virtual ~SrsRtpHeader();
public:
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual int nb_bytes();
public:
    void set_marker(bool v);
    bool get_marker() const;
    void set_payload_type(uint8_t v);
    uint8_t get_payload_type() const;
    void set_sequence(uint16_t v);
    uint16_t get_sequence() const;
    void set_timestamp(uint32_t v);
    uint32_t get_timestamp() const;
    void set_ssrc(uint32_t v);
    uint32_t get_ssrc() const;
    void set_padding(uint8_t v);
    uint8_t get_padding() const;
};

class ISrsRtpPayloader : public ISrsCodec
{
public:
    ISrsRtpPayloader();
    virtual ~ISrsRtpPayloader();
public:
    virtual ISrsRtpPayloader* copy() = 0;
};

class ISrsRtpPacketDecodeHandler
{
public:
    ISrsRtpPacketDecodeHandler();
    virtual ~ISrsRtpPacketDecodeHandler();
public:
    // We don't know the actual payload, so we depends on external handler.
    virtual void on_before_decode_payload(SrsRtpPacket2* pkt, SrsBuffer* buf, ISrsRtpPayloader** ppayload) = 0;
};

class SrsRtpPacket2
{
// RTP packet fields.
public:
    SrsRtpHeader header;
    ISrsRtpPayloader* payload;
// Helper fields.
public:
    // The first byte as nalu type, for video decoder only.
    SrsAvcNaluType nalu_type;
    // The original shared message, all RTP packets can refer to its data.
    SrsSharedPtrMessage* shared_msg;
    // The frame type, for RTMP bridger or SFU source.
    SrsFrameType frame_type;
// Fast cache for performance.
private:
    // The cached payload size for packet.
    int cached_payload_size;
    // The helper handler for decoder, use RAW payload if NULL.
    ISrsRtpPacketDecodeHandler* decode_handler;
public:
    SrsRtpPacket2();
    virtual ~SrsRtpPacket2();
public:
    // Set the padding of RTP packet.
    void set_padding(int size);
    // Increase the padding of RTP packet.
    void add_padding(int size);
    // Set the decode handler.
    void set_decode_handler(ISrsRtpPacketDecodeHandler* h);
    // Whether the packet is Audio packet.
    bool is_audio();
    // Copy the RTP packet.
    SrsRtpPacket2* copy();
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

// Single payload data.
class SrsRtpRawPayload : public ISrsRtpPayloader
{
public:
    // The RAW payload, directly point to the shared memory.
    // @remark We only refer to the memory, user must free its bytes.
    char* payload;
    int nn_payload;
public:
    SrsRtpRawPayload();
    virtual ~SrsRtpRawPayload();
// interface ISrsRtpPayloader
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual ISrsRtpPayloader* copy();
};

// Multiple NALUs, automatically insert 001 between NALUs.
class SrsRtpRawNALUs : public ISrsRtpPayloader
{
private:
    // We will manage the samples, but the sample itself point to the shared memory.
    std::vector<SrsSample*> nalus;
    int nn_bytes;
    int cursor;
public:
    SrsRtpRawNALUs();
    virtual ~SrsRtpRawNALUs();
public:
    void push_back(SrsSample* sample);
public:
    uint8_t skip_first_byte();
    // We will manage the returned samples, if user want to manage it, please copy it.
    srs_error_t read_samples(std::vector<SrsSample*>& samples, int packet_size);
// interface ISrsRtpPayloader
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual ISrsRtpPayloader* copy();
};

// STAP-A, for multiple NALUs.
class SrsRtpSTAPPayload : public ISrsRtpPayloader
{
public:
    // The NRI in NALU type.
    SrsAvcNaluType nri;
    // The NALU samples, we will manage the samples.
    // @remark We only refer to the memory, user must free its bytes.
    std::vector<SrsSample*> nalus;
public:
    SrsRtpSTAPPayload();
    virtual ~SrsRtpSTAPPayload();
public:
    SrsSample* get_sps();
    SrsSample* get_pps();
// interface ISrsRtpPayloader
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual ISrsRtpPayloader* copy();
};

// FU-A, for one NALU with multiple fragments.
// With more than one payload.
class SrsRtpFUAPayload : public ISrsRtpPayloader
{
public:
    // The NRI in NALU type.
    SrsAvcNaluType nri;
    // The FUA header.
    bool start;
    bool end;
    SrsAvcNaluType nalu_type;
    // The NALU samples, we manage the samples.
    // @remark We only refer to the memory, user must free its bytes.
    std::vector<SrsSample*> nalus;
public:
    SrsRtpFUAPayload();
    virtual ~SrsRtpFUAPayload();
// interface ISrsRtpPayloader
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual ISrsRtpPayloader* copy();
};

// FU-A, for one NALU with multiple fragments.
// With only one payload.
class SrsRtpFUAPayload2 : public ISrsRtpPayloader
{
public:
    // The NRI in NALU type.
    SrsAvcNaluType nri;
    // The FUA header.
    bool start;
    bool end;
    SrsAvcNaluType nalu_type;
    // The payload and size,
    char* payload;
    int size;
public:
    SrsRtpFUAPayload2();
    virtual ~SrsRtpFUAPayload2();
// interface ISrsRtpPayloader
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual ISrsRtpPayloader* copy();
};

#endif
