
#include "bio.hpp"
#include <fstream>

using namespace ck::bio;

int main() {

    std::ifstream fi("H:/test2.ckt",std::ios::binary);
    auto rd = make_reader(fi);
    char tag[3];
    rd.read(tag,3);
    bool compressed = false;
    rd.read(compressed);

    std::ofstream fo("H:/test4.ckt",std::ios::binary);
    auto wt = make_writer(fo);
    wt.write("CKT");
    wt.write(compressed);

    return 0;
}
