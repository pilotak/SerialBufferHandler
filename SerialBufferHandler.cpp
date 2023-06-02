#include "SerialBufferHandler.h"

SerialBufferHandler::SerialBufferHandler(FileHandle *fh, EventQueue &queue): _fileHandle(fh), _queue(queue) {
    _fileHandle->set_blocking(false);
    _fileHandle->sigio(Callback<void()>(this, &SerialBufferHandler::event));
    _start_time = Kernel::Clock::now();
}

void SerialBufferHandler::attach(Callback<void()> cb, milliseconds timeout) {
    _cb = cb;
    _timeout = timeout;

}

void SerialBufferHandler::flush() {
    reset_buffer();

    while (fill_buffer(false)) {
        reset_buffer();
    }
}

size_t SerialBufferHandler::read_bytes(uint8_t *buf, size_t len) {
    size_t max = _recv_len - _recv_pos;
    size_t to_read =  len > max ? max : len;

    if (len == 0 || to_read == 0) {
        return 0;
    }

    lock();
    memcpy(buf, _buffer + _recv_pos, to_read);
    unlock();

    _recv_pos = to_read;
    rewind_buffer();

    return to_read;
}

size_t SerialBufferHandler::rewind_until(uint8_t until) {
    size_t i = 0;

    for (i = _recv_pos; i < _recv_len; i++) {
        if (_buffer[i] == until) {
            break;
        }
    }

    _recv_pos = i;

    rewind_buffer();

    return _recv_len;
}

size_t SerialBufferHandler::rewind_until(const uint8_t *find, size_t len) {
    size_t position = SIZE_MAX;

    for (size_t i = 0; i < sizeof(_buffer) - len; i++) {
        if (memcmp(_buffer + i, find, len) == 0) {
            position = i;
            break;
        }
    }

    if (position == SIZE_MAX) { // handle not found
        position = _recv_len;
    }


    _recv_pos = position;

    rewind_buffer();

    return _recv_len;
}

size_t SerialBufferHandler::available_bytes() {
    return _recv_len - _recv_pos;
}

uint8_t SerialBufferHandler::check_byte(size_t index) {
    return _buffer[index + _recv_pos];
}

void SerialBufferHandler::event() {
    if (_event_id == 0) {
        _event_id = _queue.call(Callback<void()>(this, &SerialBufferHandler::read));
    }
}

void SerialBufferHandler::read() {
    if (_fileHandle->readable() || (_recv_pos < _recv_len)) {
        // printf("Serial readable %d,  %u, %u \n", _fileHandle->readable(), _recv_len, _recv_pos);

        while (true) {
            if (!(_fileHandle->readable() || (_recv_pos < _recv_len))) {
                break; // we have nothing to read anymore
            }

            if (!fill_buffer()) {
                break;
            }

            _start_time = Kernel::Clock::now();
        }

        if (_cb) {
            _cb.call();
        }
    }

    _event_id = 0;
}

bool SerialBufferHandler::fill_buffer(bool wait_for_timeout) {
    // Reset buffer when full
    if (sizeof(_buffer) == _recv_len) {
        printf("Overflow\n");
        reset_buffer();
    }

    pollfh fhs;
    fhs.fh = _fileHandle;
    fhs.events = POLLIN;
    int count = poll(&fhs, 1, poll_timeout(wait_for_timeout));

    if (count > 0 && (fhs.revents & POLLIN)) {
        lock();
        ssize_t len = _fileHandle->read(_buffer + _recv_len, sizeof(_buffer) - _recv_len);
        unlock();

        if (len > 0) {
            _recv_len += len;
            return true;
        }
    }

    return false;
}

void SerialBufferHandler::rewind_buffer() {
    lock();

    if (_recv_pos > 0 && _recv_len >= _recv_pos) {
        _recv_len -= _recv_pos;
        // move what is not read to beginning of buffer
        memmove(_buffer, _buffer + _recv_pos, _recv_len);
        _recv_pos = 0;
    }

    unlock();
}

int SerialBufferHandler::poll_timeout(bool wait_for_timeout) {
    duration<int, std::milli> timeout;

    if (wait_for_timeout) {
        auto now = Kernel::Clock::now();

        if (now >= _start_time + _timeout) {
            timeout = 0s;

        } else if (_start_time + _timeout - now > timeout.max()) {
            timeout = timeout.max();

        } else {
            timeout = _start_time + _timeout - now;
        }

    } else {
        timeout = 0s;
    }

    return timeout.count();
}

void SerialBufferHandler::reset_buffer() {
    _recv_pos = 0;
    _recv_len = 0;
}

void SerialBufferHandler::lock() {
    _mutex.lock();
    _start_time = Kernel::Clock::now();
}

void SerialBufferHandler::unlock() {
    if (_fileHandle->readable()) {
        _event_id = _queue.call(Callback<void()>(this, &SerialBufferHandler::read));
    }

    _mutex.unlock();
}