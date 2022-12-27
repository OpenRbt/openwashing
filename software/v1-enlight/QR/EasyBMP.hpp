#ifndef EasyBMP_H_
#define EasyBMP_H_


/*
Original repository: https://github.com/izanbf1803/EasyBMP
License: MIT
*/
#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

using namespace std;

namespace EasyBMP 
{
    class RGBColor 
    {
    public:
        RGBColor(); // Don't use void constructor, just for variable definition
        //~RGBColor();
        RGBColor(uint8_t _r, uint8_t _g, uint8_t _b);
        void SetColor(uint8_t _r, uint8_t _g, uint8_t _b);

        uint8_t r;
        uint8_t g;
        uint8_t b;
    };


    class Image 
    {
    public:
        ~Image();
        Image() { width = height = 0; }; // Don't use default constructor, just for variable definition
        Image(const string& _inFileName);
        Image(const string& _inFileName, const string& _outFileName);
        Image(int64_t _width, int64_t _height);
        Image(int64_t _width, int64_t _height, const string& _outFileName);
        Image(int64_t _width, int64_t _height, const RGBColor& _backgroundColor);
        Image(int64_t _width, int64_t _height, const string& _outFileName, const RGBColor& _backgroundColor);
        void SetPixel(int64_t x, int64_t y, const RGBColor& color, bool ignore_err);
        const RGBColor& GetPixel(int64_t x, int64_t y) const;
        void DrawLine(int64_t x0, int64_t y0, int64_t x1, int64_t y1, const RGBColor& color);
        void DrawCircle(int64_t x0, int64_t y0, int64_t r, const RGBColor& color, bool fill);
        void SetFileName(const string& _outFileName);
        void Write(const string& _outFileName);
        void Write();
        inline int64_t w() const { return width; }
        inline int64_t h() const { return height; }
        inline bool isValidCoordinate(int64_t x, int64_t y) const {
            return x >= 0 and y >= 0 and x < width and y < height;
        }

    private:
        void Init(int64_t _width, int64_t _height);
        void Setup();
        void FillBG();
        void DrawLineLow(int64_t x0, int64_t y0, int64_t x1, int64_t y1, const RGBColor& color);
        void DrawLineHigh(int64_t x0, int64_t y0, int64_t x1, int64_t y1, const RGBColor& color);
        void OpenSetup(const string& _inFileName);

        int64_t width;
        int64_t height;
        ofstream outFile;
        string outFileName;
        RGBColor backgroundColor;
        RGBColor** buffer;
    };

    // Load constant values
}
#endif