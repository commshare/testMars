// Copyright (C) 2013, The Toft Authors.
// Author: An Qin <anqin.qin@gmail.com>
//
// Description:

// toft/storage/recordio/reverse_recordio.cc

#include "utils/toft_storage_recordio_reverse_recordio.h"

#include "platform/base_error.h"

namespace bubblefs {
namespace mytoft {

ReverseRecordWriter::ReverseRecordWriter(File *file)
    : m_file(file) {
    assert(m_file != NULL);
}

ReverseRecordWriter::~ReverseRecordWriter() {}

bool ReverseRecordWriter::WriteMessage(const ::google::protobuf::Message& message) {
    std::string output;
    if (!message.IsInitialized()) {
        PRINTF_WARN("Missing required fields. %s\n", message.InitializationErrorString().c_str());
        return false;
    }
    if (!message.AppendToString(&output)) {
        return false;
    }
    if (!WriteRecord(output.data(), output.size())) {
        return false;
    }
    return true;
}

bool ReverseRecordWriter::WriteRecord(const char *data, uint32_t size) {
    if (!Write(data, size)) {
        return false;
    }
    if (!Write(reinterpret_cast<char*>(&size), sizeof(size))) {
        return false;
    }
    return true;
}

bool ReverseRecordWriter::WriteRecord(const std::string& data) {
    return WriteRecord(data.data(), data.size());
}

bool ReverseRecordWriter::WriteRecord(const StringPiece& data) {
    return WriteRecord(data.data(), data.size());
}

bool ReverseRecordWriter::Write(const char *data, uint32_t size) {
    uint32_t write_size = 0;
    while (write_size < size) {
        int32_t ret = m_file->Write(data + write_size, size - write_size);
        if (ret == -1) {
            PRINTF_ERROR("ReverseRecordWriter error.\n");
            return false;
        }
        write_size += ret;
    }
    return true;
}


ReverseRecordReader::ReverseRecordReader(File *file)
    : m_file(file),
      m_buffer_size(1 * 1024 * 1024) {
    assert(m_file != NULL);
    m_buffer.reset(new char[m_buffer_size]);
    Reset();
}

ReverseRecordReader::~ReverseRecordReader() {}

bool ReverseRecordReader::Reset() {
    if (!m_file->Seek(0, SEEK_END)) {
        PRINTF_ERROR("ReverseRecordReader Reset error.\n");
        return false;
    }
    return true;
}

int ReverseRecordReader::Next() {
    // read size
    int64_t ret = m_file->Tell();
    if (ret == -1) {
        PRINTF_ERROR("Tell error.\n");
        return -1;
    }

    if (ret == 0) {
        return 0;
    } else if (ret >= static_cast<int64_t>(sizeof(m_data_size))) { // NO_LINT
        if (!Read(reinterpret_cast<char*>(&m_data_size), sizeof(m_data_size))) {
            PRINTF_ERROR("Read size error.\n");
            return -1;
        }
    }

    // read data
    ret = m_file->Tell();
    if (ret == -1) {
        PRINTF_ERROR("Tell error.\n");
        return -1;
    }

    if (ret == 0 && ret != m_data_size) {
        PRINTF_ERROR("read error.\n");
        return -1;
    } else if (ret >= m_data_size) { // NO_LINT
        if (m_data_size > m_buffer_size) {
            while (m_data_size > m_buffer_size) {
                m_buffer_size *= 2;
            }
            m_buffer.reset(new char[m_buffer_size]);
        }

        if (!Read(m_buffer.get(), m_data_size)) {
            PRINTF_ERROR("Read data error.\n");
            return -1;
        }
    }

    return 1;
}

bool ReverseRecordReader::ReadMessage(::google::protobuf::Message *message) {
    if (!message->ParseFromArray(m_buffer.get(), m_data_size)) {
        PRINTF_WARN("Missing required fields.\n");
        return false;
    }
    return true;
}

bool ReverseRecordReader::ReadNextMessage(::google::protobuf::Message *message) {
    while (Next() == 1) {
        std::string str(m_buffer.get(), m_data_size);
        if (message->ParseFromArray(m_buffer.get(), m_data_size)) {
            return true;
        }
    }
    return false;
}

bool ReverseRecordReader::ReadRecord(const char **data, uint32_t *size) {
    *data = m_buffer.get();
    *size = m_data_size;
    return true;
}

bool ReverseRecordReader::ReadRecord(std::string *data) {
    data->assign(m_buffer.get(), m_data_size);
    return true;
}

bool ReverseRecordReader::ReadRecord(StringPiece* data) {
    const char* buffer = NULL;
    uint32_t size;
    if (!ReadRecord(&buffer, &size)) {
        return false;
    }
    data->set(buffer, size);
    return true;
}

bool ReverseRecordReader::Read(char *data, uint32_t size) {
    // After reading, file pointer need to seek back to the start position of
    // data. Prepare for next reading.
    // Seek back
    int32_t offset = -static_cast<int32_t>(size);
    if (!m_file->Seek(offset, SEEK_CUR)) {
        PRINTF_ERROR("Seek error before read.\n");
        return false;
    }

    // Read
    uint32_t read_size = 0;
    while (read_size < size) {
        int64_t ret = m_file->Read(data + read_size, size - read_size);
        if (ret == -1) {
            PRINTF_ERROR("Read error.\n");
            return false;
        }
        read_size += ret;
    }

    // Seek back
    if (!m_file->Seek(offset, SEEK_CUR)) {
        PRINTF_ERROR("Seek error after read.\n");
        return false;
    }

    return true;
}

} // namespace mytoft
} // namespace bubblefs