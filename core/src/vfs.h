#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class VFS {
public:
    VFS();

    bool create(const std::string& path);
    bool exists(const std::string& path) const;
    bool write(const std::string& path, const uint8_t* data, size_t n);
    bool append(const std::string& path, const uint8_t* data, size_t n);
    std::vector<uint8_t> read(const std::string& path) const;
    bool remove(const std::string& path);
    std::vector<std::string> list(const std::string& dir) const;
    void clear();

    static std::string normalize(const std::string& path);

private:
    std::unordered_map<std::string, std::vector<uint8_t>> files_;
};
