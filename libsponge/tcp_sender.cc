#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (!_syn_flag) {
        _syn_flag = true;
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        _outstanding_segments.push(seg);
        _segments_out.push(seg);
    } else {
        uint64_t window_size = _window_size > 0 ? _window_size : 1;
        uint64_t remain_size;
        while ((remain_size = window_size - (_next_seqno - _ackno)) != 0 && !_fin_flag) {
            size_t size = min(TCPConfig::MAX_PAYLOAD_SIZE, remain_size);
            TCPSegment seg;
            string str = _stream.read(size);
            seg.payload() = Buffer(std::move(str));
            seg.header().seqno = wrap(_next_seqno, _isn);
            if (seg.length_in_sequence_space() < remain_size && _stream.eof()) {  // fin flag occupy 1 byte, so <
                _fin_flag = true;
                seg.header().fin = true;
            }
            if (seg.length_in_sequence_space() == 0)  // otherwise test will timeout. It must be placed after the fin
                                                      // check and before pushing to outstanding seg
                break;
            _next_seqno += seg.length_in_sequence_space();
            _bytes_in_flight += seg.length_in_sequence_space();
            _outstanding_segments.push(seg);
            _segments_out.push(seg);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _ackno);
    if (abs_ackno > _next_seqno)
        return;
    _window_size = window_size;
    if (abs_ackno <= _ackno)
        return;
    _ackno = abs_ackno;

    while (!_outstanding_segments.empty()) {
        auto front = _outstanding_segments.front();
        uint64_t front_length = unwrap(front.header().seqno, _isn, _ackno) + front.length_in_sequence_space();
        if (front_length <= abs_ackno) {
            _outstanding_segments.pop();
            _bytes_in_flight -= front.length_in_sequence_space();
        } else
            break;
    }
    fill_window();
    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    _retransmission_timer = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _retransmission_timer += ms_since_last_tick;
    if (_retransmission_timer >= _retransmission_timeout && !_outstanding_segments.empty()) {
        _segments_out.push(_outstanding_segments.front());
        _consecutive_retransmissions++;
        if (_window_size ||
            _outstanding_segments.front().header().syn == true) {  // syn seg's window size is zero but need timing
            _retransmission_timeout <<= 1;
        }
        _retransmission_timer = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment tcpseg;
    tcpseg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(tcpseg);
}
