#include "deflate.h"
#include <string.h>

void bit_writer_init(BitWriter *writer, uint8_t *buffer, size_t size) {
    writer->buffer = buffer;
    writer->buffer_size = size;
    writer->byte_pos = 0;
    writer->bit_pos = 0;
    writer->current_byte = 0;
    if (buffer && size > 0) {
        memset(buffer, 0, size);
    }
}

void bit_writer_write_bit(BitWriter *writer, bool bit) {
    if (!writer || !writer->buffer) return;
    
    if (bit) {
        writer->current_byte |= (1 << writer->bit_pos);
    }
    writer->bit_pos++;
    
    if (writer->bit_pos == 8) {
        if (writer->byte_pos < writer->buffer_size) {
            writer->buffer[writer->byte_pos++] = writer->current_byte;
        } else {
            // Буфер переполнен - это ошибка
            return;
        }
        writer->current_byte = 0;
        writer->bit_pos = 0;
    }
}

void bit_writer_write_bits(BitWriter *writer, uint32_t value, uint8_t num_bits) {
    for (uint8_t i = 0; i < num_bits; i++) {
        bit_writer_write_bit(writer, (value >> i) & 1);
    }
}

void bit_writer_flush(BitWriter *writer) {
    if (writer->bit_pos > 0) {
        if (writer->byte_pos < writer->buffer_size) {
            writer->buffer[writer->byte_pos++] = writer->current_byte;
        }
        writer->current_byte = 0;
        writer->bit_pos = 0;
    }
}

size_t bit_writer_get_size(BitWriter *writer) {
    return writer->byte_pos + (writer->bit_pos > 0 ? 1 : 0);
}

void bit_reader_init(BitReader *reader, const uint8_t *buffer, size_t size) {
    reader->buffer = buffer;
    reader->buffer_size = size;
    reader->byte_pos = 0;
    reader->bit_pos = 0;
    reader->current_byte = 0;
}

bool bit_reader_read_bit(BitReader *reader) {
    if (reader->bit_pos == 0) {
        if (reader->byte_pos >= reader->buffer_size) {
            return false;
        }
        reader->current_byte = reader->buffer[reader->byte_pos++];
    }
    
    bool bit = (reader->current_byte >> reader->bit_pos) & 1;
    reader->bit_pos = (reader->bit_pos + 1) & 7;
    return bit;
}

uint32_t bit_reader_read_bits(BitReader *reader, uint8_t num_bits) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < num_bits; i++) {
        if (bit_reader_read_bit(reader)) {
            value |= (1 << i);
        }
    }
    return value;
}

