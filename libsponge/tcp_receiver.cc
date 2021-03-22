#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!(_syn_assigned || seg.header().syn))  // // only run after SYN
        return;
    if (seg.header().syn)  // syn seg
    {
        if (_syn_assigned)
            return;
        _syn_assigned = true;
        _ackno = 1;  // syn is a byte
        _abs_seqno = 1;
        _isn = seg.header().seqno.raw_value();  // set _isn
    }
    if (seg.header().fin)  // fin seg
    {
        if (_fin_assigned)  // only accept FIN once
            return;
        _fin_assigned = true;
    }

    if (!seg.header().syn)  // calculate absolute seqno
        _abs_seqno = unwrap(WrappingInt32(seg.header().seqno.raw_value()), WrappingInt32(_isn), _abs_seqno);

    _reassembler.push_substring(
        std::move(seg.payload().copy()), _abs_seqno - 1, seg.header().fin);  // absolute seqno start from 1 for payload.
    _ackno = _reassembler.index_written() + 1;                               // include SYN (+1)
    if (_reassembler.input_ended())  // Easy to neglect! all payload finished, add FIN.
        _ackno += 1;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_syn_assigned)
        return WrappingInt32(wrap(_ackno, WrappingInt32(_isn)));
    else
        return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
