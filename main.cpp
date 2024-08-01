#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include "picopng.cpp"
#include <windows.h>

static constexpr char DENSITY[93] { " `.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$Bg0MNWQ%&@" };

void cls(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SMALL_RECT scrollRect;
    COORD scrollTarget;
    CHAR_INFO fill;

    // Get the number of character cells in the current buffer.
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        return;
    }

    // Scroll the rectangle of the entire buffer.
    scrollRect.Left = 0;
    scrollRect.Top = 0;
    scrollRect.Right = csbi.dwSize.X;
    scrollRect.Bottom = csbi.dwSize.Y;

    // Scroll it upwards off the top of the buffer with a magnitude of the entire height.
    scrollTarget.X = 0;
    scrollTarget.Y = (SHORT)(0 - csbi.dwSize.Y);

    // Fill with empty spaces with the buffer's default text attribute.
    fill.Char.UnicodeChar = TEXT(' ');
    fill.Attributes = csbi.wAttributes;

    // Do the scroll
    ScrollConsoleScreenBuffer(hConsole, &scrollRect, NULL, scrollTarget, &fill);

    // Move the cursor to the top left corner too.
    csbi.dwCursorPosition.X = 0;
    csbi.dwCursorPosition.Y = 0;

    SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}

void loadFile(std::vector<unsigned char>& buffer, const std::string& filename)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);

    //get filesize
    std::streamsize size = 0;
    if (file.seekg(0, std::ios::end).good()) size = file.tellg();
    if (file.seekg(0, std::ios::beg).good()) size -= file.tellg();

    //read contents of the file into the vector
    if (size > 0)
    {
        buffer.resize(static_cast<std::size_t>(size));
        file.read(reinterpret_cast<char*>(&buffer[0]), size);
    }
    else buffer.clear();
}

std::vector<std::vector<unsigned char>> bilinearResized(const std::vector<std::vector<unsigned char>>& input, 
                                                        int inputWidth, int inputHeight,
                                                        int outputWidth, int outputHeight)
{
    std::vector<std::vector<unsigned char>> output(outputHeight, std::vector<unsigned char>(outputWidth));

    float x_ratio = static_cast<float>(inputWidth - 1) / outputWidth; 
    float y_ratio = static_cast<float>(inputHeight - 1) / outputHeight;
    float x_diff, y_diff;
    int x, y;

    for (int h = 0; h < outputHeight; ++h) {
        for (int w = 0; w < outputWidth; ++w) {
            x = static_cast<int>(x_ratio * w);
            y = static_cast<int>(y_ratio * h);
            x_diff = (x_ratio * w) - x;
            y_diff = (y_ratio * h) - y;

            //Edge processing
            x = min(x, inputWidth - 2);
            y = min(y, inputHeight - 2);

            //Get neighbours
            unsigned char A = input[y][x];
            unsigned char B = input[y][x + 1];
            unsigned char C = input[y + 1][x];
            unsigned char D = input[y + 1][x + 1];

            //Interpolate pixel
            unsigned char pixel =
                A * (1 - x_diff) * (1 - y_diff) +
                B * x_diff * (1 - y_diff) +
                C * (1 - x_diff) * y_diff +
                D * x_diff * y_diff;

            output[h][w] = pixel;
        }
    }

    return output;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "PNG file required!";
        return -1;
    }

    //load and decode
    std::vector<unsigned char> buffer, image;
    loadFile(buffer, argv[1]);
    unsigned long imageWidth, imageHeight;
    int error = decodePNG(image, imageWidth, imageHeight, buffer.empty() ? 0 : &buffer[0], buffer.size());

    //if there's an error, display it
    if (error != 0) std::cout << "error: " << error << std::endl;

    //get console size by winapi
    HANDLE hSTDOUT;

    hSTDOUT = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int consoleWidth, consoleHeight, returnWindowsBufferInfo;
    returnWindowsBufferInfo = GetConsoleScreenBufferInfo(hSTDOUT, &csbi);
    if (returnWindowsBufferInfo)
    {
        consoleWidth = csbi.dwSize.X;
        consoleHeight = csbi.dwSize.Y;
    }

    cls(hSTDOUT);

    //grayscaling image from RGBA
    std::vector<std::vector<unsigned char>> grayscaleImage(imageHeight, std::vector<unsigned char>(imageWidth, 0));
    std::size_t pixel = 0;
    for (int height = 0; height != imageHeight; height++) {
        for (int width = 0; width != imageWidth; width++) {
            grayscaleImage[height][width] = 0.299 * image[pixel] + 0.587 * image[pixel + 1] + 0.114 * image[pixel + 2]; //image[i+3] is alpha channel
            pixel += 4;
        }
    }

    //bilinear interpolation is more better step by step
    while (imageWidth / 2 >= consoleWidth || imageHeight / 2 >= consoleHeight) {
        grayscaleImage = bilinearResized(grayscaleImage, imageWidth, imageHeight, imageWidth / 2, imageHeight / 2);
        imageWidth /= 2;
        imageHeight /= 2;
    }

    //finishing resolution
    auto finishScale = max((float)imageWidth / (float)consoleWidth, (float)imageHeight / (float)consoleHeight);
    auto finishWidth = (int) ((float)imageWidth / finishScale);
    auto finishHeight = (int) ((float)imageHeight / finishScale);
    grayscaleImage = bilinearResized(grayscaleImage, imageWidth, imageHeight, finishWidth, finishHeight);

    std::string outputBuffer;
    outputBuffer.reserve(finishWidth * finishHeight);
    for (auto i = 0; i != finishHeight; i++) {
        for (auto j = 0; j != finishWidth; j++) {
            outputBuffer += DENSITY[ static_cast<int>( static_cast<float>(grayscaleImage[i][j]) / (255.0f / 93.0f) ) ]; //93 is density size
        }
        outputBuffer += '\n';
    }
    std::cout << outputBuffer;
    return 0;
}
