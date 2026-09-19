#include "vfs.h"

#include <algorithm>
#include <cstring>

VFS::VFS() {
    create("/etc");
    create("/home");
    create("/tmp");

    const char* motd = "welcome to mxy-sandbox\n";
    write("/etc/motd", reinterpret_cast<const uint8_t*>(motd), std::strlen(motd));
}

std::string VFS::normalize(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;

    auto flush = [&]() {
        if (cur.empty()) return;
        if (cur == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (cur != ".") parts.push_back(cur);
        cur.clear();
    };

    for (char c : path) {
        if (c == '/') flush();
        else cur.push_back(c);
    }
    flush();

    std::string out = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        out += parts[i];
        if (i + 1 < parts.size()) out += "/";
    }
    return out;
}

bool VFS::create(const std::string& path) {
    std::string p = normalize(path);
    if (p == "/") return false;
    files_.emplace(p, std::vector<uint8_t>{});
    return true;
}

bool VFS::exists(const std::string& path) const {
    return files_.count(normalize(path)) > 0;
}

bool VFS::write(const std::string& path, const uint8_t* data, size_t n) {
    std::string p = normalize(path);
    if (p == "/") return false;
    auto& v = files_[p];
    v.assign(data, data + n);
    return true;
}

bool VFS::append(const std::string& path, const uint8_t* data, size_t n) {
    std::string p = normalize(path);
    if (p == "/") return false;
    auto& v = files_[p];
    v.insert(v.end(), data, data + n);
    return true;
}

std::vector<uint8_t> VFS::read(const std::string& path) const {
    auto it = files_.find(normalize(path));
    if (it == files_.end()) return {};
    return it->second;
}

bool VFS::remove(const std::string& path) {
    return files_.erase(normalize(path)) > 0;
}

std::vector<std::string> VFS::list(const std::string& dir) const {
    std::string d = normalize(dir);
    std::string prefix = (d == "/") ? "/" : d + "/";

    std::vector<std::string> out;
    for (const auto& kv : files_) {
        if (kv.first.size() <= prefix.size()) continue;
        if (kv.first.compare(0, prefix.size(), prefix) != 0) continue;

        std::string rest = kv.first.substr(prefix.size());
        size_t slash = rest.find('/');
        std::string name = (slash == std::string::npos) ? rest : rest.substr(0, slash);
        if (std::find(out.begin(), out.end(), name) == out.end())
            out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void VFS::clear() {
    files_.clear();
}