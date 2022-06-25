#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <memory>

struct Color {
    union {
        struct {
            char r;
            char g;
            char b;
        };
        char data[3];
    };
};

char ColorDistance(Color const &a, Color const &b) {
    return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
}

struct Image {
    Image(std::string const &filename) {
        std::ifstream in(filename, std::ios::binary | std::ios::in);
        std::string header;
        int depth;
        in >> header >> width >> height >> depth;
        data = std::make_unique<Color[]>(width * height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                in.read(data[y * width + x].data, 3);
            }
        }
    }

    Image(int width, int height)
        : width(width), height(height)
    {
        data = std::make_unique<Color[]>(width * height);
    }

    void write(std::string const &filename) {
        std::ofstream o(filename, std::ios::binary | std::ios::out);
        o << "P6\n" << width << '\n' << height << "\n255\n";
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                o.write(data[y * width + x].data, 3);
            }
        }
    }

    int width = 0;
    int height = 0;
    std::unique_ptr<Color[]> data;
};

Image ImageDiff(Image const &a, Image const &b) {
    Image res(a.width, a.height);
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            auto diff = ColorDistance(a.data[y * a.width + x], b.data[y * a.width + x]);
            res.data[y * a.width + x] = { diff, diff, diff };
        }
    }
    return res;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        throw std::runtime_error("Incorrect number of argumets");
    }

    Image a(argv[1]);
    Image b(argv[2]);
    Image diff = ImageDiff(a, b);
    diff.write(argv[3]);
    return 0;
}
