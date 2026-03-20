#pragma once

#include <glad/glad.h>
#include <string>

namespace ssm {

// Simple 8x8 bitmap font rendered via OpenGL texture
// Covers ASCII 32-126 (printable characters)
class BitmapFont {
public:
    bool init();
    void shutdown();

    // Draw text at screen-space pixel coordinates
    // scale: 1.0 = 8px tall, 2.0 = 16px tall, etc.
    void drawText(float x, float y, const std::string& text,
                  float r, float g, float b, float a = 1.0f, float scale = 2.0f);

    // Measure text width in pixels at given scale
    float measureText(const std::string& text, float scale = 2.0f) const;

    // Must be called with an orthographic screen-space projection active
    // on the font shader before drawText calls
    void beginText(int windowWidth, int windowHeight);
    void endText();

    float getCharHeight(float scale = 2.0f) const { return 8.0f * scale; }

private:
    GLuint textureId = 0;
    GLuint shaderProgram = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint uProjection = -1;
    GLint uTexture = -1;
    GLint uColor = -1;

    static const int FONT_CHARS = 95; // ASCII 32-126
    static const int CHAR_W = 8;
    static const int CHAR_H = 8;
    static const int ATLAS_COLS = 16;
    static const int ATLAS_ROWS = 6; // ceil(95/16)

    void generateFontTexture();
};

} // namespace ssm
