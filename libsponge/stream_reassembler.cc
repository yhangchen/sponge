#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    DataBlock datablock;  // record the data into DataBlock.
    std::set<DataBlock> _BlockToErase = {};
    if (_index_written + _capacity <= index) {  // data lies in "first unacceptable"
        return;
    }
    size_t toCut = 0;
    std::string cutData = "";
    if (index + data.length() <= _index_written) {  // data lies in "read already"
        goto postpush;
    }
    toCut = min(_index_written + _capacity - index, data.length());
    cutData = data.substr(0, toCut);
    if (index < _index_written) {
        datablock.data = cutData.substr(_index_written - index, cutData.length() - _index_written + index);
        datablock.begin = _index_written;
    } else {
        datablock.data = cutData;
        datablock.begin = index;
    }
    _unassembled_bytes += datablock.data.length();

    // Merge datablocks in _BlockCollector. DataBlocks in _BlockCollector do not intersect with each other.
    for (auto it = _BlockCollector.begin(); it != _BlockCollector.end(); it++) {
        size_t itBegin = (*it).begin;
        size_t itEnd = itBegin + (*it).data.length();
        size_t datablockBegin = datablock.begin;
        size_t datablockEnd = datablock.begin + datablock.data.length();
        if (((itEnd >= datablockBegin) && (itBegin <= datablockBegin)) ||
            ((itEnd >= datablockEnd) && (itBegin <= datablockEnd)) ||
            ((itEnd <= datablockEnd) && (itBegin >= datablockBegin))) {
            size_t toMerge = (*it).data.length() + datablock.data.length() -
                             (max(datablockEnd, itEnd) - min(datablockBegin, itBegin));
            _unassembled_bytes -= toMerge;
            datablock.begin = min(datablockBegin, itBegin);
            if (itBegin < datablockBegin) {
                std::string toAppend = (*it).data.substr(0, datablockBegin - itBegin);
                datablock.data = toAppend.append(datablock.data);
            }
            if (itEnd > datablockEnd) {
                std::string toAppend = (*it).data.substr(datablockEnd - itBegin, itEnd - datablockEnd);
                datablock.data = datablock.data.append(toAppend);
            }
            _BlockToErase.insert(*it);  // we cannot erase *it iteratively here, we need to use _BlockToErase.
        }
    }
    for (auto it = _BlockToErase.begin(); it != _BlockToErase.end(); it++) {
        _BlockCollector.erase(*it);
    }
    _BlockCollector.insert(datablock);
    if (!_BlockCollector.empty() && _BlockCollector.begin()->begin == _index_written) {
        const DataBlock BlocktoWrite = *_BlockCollector.begin();
        // modify _head_index and _unassembled_byte according to successful write to _output
        size_t write_bytes = _output.write(BlocktoWrite.data);
        _index_written += write_bytes;
        _unassembled_bytes -= write_bytes;
        DataBlock UnfinisedBlocktoWrite;
        size_t BlocktoWriteLength = BlocktoWrite.data.length();
        if (write_bytes < BlocktoWriteLength) {
            UnfinisedBlocktoWrite.begin = BlocktoWrite.begin + write_bytes;
            UnfinisedBlocktoWrite.data = BlocktoWrite.data.substr(write_bytes, BlocktoWriteLength - write_bytes);
            _BlockCollector.insert(UnfinisedBlocktoWrite);
        }
        _BlockCollector.erase(BlocktoWrite);
    }
postpush:
    if (eof) {
        _eof = true;
    }
    if (_eof && empty()) {
        _output.end_input();  // end the output
    }
    // in this part, we cannot use
    // if (eof && empty()) {_output.end_input();}
    // since data came last may not contain eof flag.
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
