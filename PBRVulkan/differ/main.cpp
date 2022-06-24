#include <fstream>
#include <stdexcept>
#include <algorithm>

int main(int argc, char *argv[])
{
    if (argc != 4) {
        throw std::runtime_error("Incorrect number of argumets");
    }

    std::string header = "";
    int width = 0;
    int height = 0;
    int res = 0;

    std::ifstream i1(argv[1], std::ios::binary | std::ios::in);
    i1 >> header >> width >> height >> res;

    std::ifstream i2(argv[2], std::ios::binary | std::ios::in);
    i2 >> header >> width >> height >> res;

    std::ofstream o(argv[3], std::ios::binary | std::ios::out);
    o << header << '\n' << width << '\n' << height << "\n255\n";

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            char ip1[3] = {};
            i1.read(ip1, 3);
            char ip2[3] = {};
            i2.read(ip2, 3);

            char op[3] = {};
            for (int i = 0; i < 3; ++i) {
                op[i] = (char)(std::abs((int)ip1[i] - (int)ip2[i]));
            }
            o.write(op, 3);
        }
    }

    return 0;
}
