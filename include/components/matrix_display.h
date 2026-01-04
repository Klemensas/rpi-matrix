#ifndef MATRIX_DISPLAY_H
#define MATRIX_DISPLAY_H

#include <string>
#include <led-matrix.h>

using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

class MatrixDisplay {
public:
    MatrixDisplay(int rows, int cols, int chain_length = 1, int parallel = 1,
                  const std::string& hardware_mapping = "regular");
    ~MatrixDisplay();

    bool isReady() const;
    int getWidth() const;
    int getHeight() const;

    void displayFrame(uint8_t *data, int width, int height);

private:
    void setup();
    void cleanup();

    int rows_;
    int cols_;
    int chain_length_;
    int parallel_;
    std::string hardware_mapping_;
    RGBMatrix *matrix_;
    FrameCanvas *canvas_;
};

#endif // MATRIX_DISPLAY_H
