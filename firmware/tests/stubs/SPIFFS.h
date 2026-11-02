#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <limits>

#define FILE_READ "rb"
#define FILE_WRITE "wb"

namespace fs {
// Fault injection for persistence tests; defaults preserve normal file behavior.
inline bool mountAvailable = true;
inline bool formatRequested = false;
inline bool failWriteOpen = false;
inline size_t writeBudget = std::numeric_limits<size_t>::max();
inline size_t writeOpenCount = 0;
class File {
public:
  File() = default;
  File(const std::string& path, const char* mode) : path_(path) {
    std::ios::openmode flags = std::ios::binary;
    if (mode && std::string(mode) == FILE_WRITE) {
      ++writeOpenCount;
      if (failWriteOpen) return;
      flags |= std::ios::out | std::ios::trunc;
    }
    else flags |= std::ios::in;
    stream_.open(path_, flags);
  }

  explicit operator bool() const { return stream_.is_open() && stream_.good(); }
  size_t read(uint8_t* bytes, size_t count) {
    stream_.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(count));
    return static_cast<size_t>(stream_.gcount());
  }
  size_t write(const uint8_t* bytes, size_t count) {
    count = std::min(count, writeBudget);
    writeBudget -= count;
    stream_.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(count));
    return stream_ ? count : 0;
  }
  size_t size() const {
    std::error_code error;
    const auto bytes = std::filesystem::file_size(path_, error);
    return error ? 0 : static_cast<size_t>(bytes);
  }
  void flush() { stream_.flush(); }
  void close() { stream_.close(); }

private:
  std::string path_;
  std::fstream stream_;
};
} // namespace fs

class SPIFFSClass {
public:
  bool begin(bool formatOnFail) {
    fs::formatRequested = formatOnFail;
    return fs::mountAvailable;
  }
  fs::File open(const char* path, const char* mode) const {
    std::string relative = path ? path : "";
    while (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
    return fs::File(relative, mode);
  }
};

static SPIFFSClass SPIFFS;
