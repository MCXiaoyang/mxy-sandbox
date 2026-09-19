#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Framebuffer {
public:
    static constexpr int    WIDTH  = 640;
    static constexpr int    HEIGHT = 480;
    static constexpr size_t PIXELS = static_cast<size_t>(WIDTH) * HEIGHT;
    static constexpr size_t BYTES  = PIXELS * 4;

    Framebuffer();

    void clear(uint32_t rgba);

    void setPixel(int x, int y, uint32_t rgba);
    void setPixelIndex(uint32_t index, uint32_t rgba);

    void fillRect(int x, int y, int w, int h, uint32_t rgba);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t rgba);
    void drawChar(int x, int y, char c, uint32_t rgba);
    void drawText(int x, int y, const std::string& text, uint32_t rgba);

    const uint8_t* data() const { return pixels_.data(); }
    uint8_t*       data()       { return pixels_.data(); }
    size_t         size() const { return pixels_.size(); }

private:
    std::vector<uint8_t> pixels_;
};
