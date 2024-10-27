#include <iostream>
#include <getopt.h>
#include <string>

extern char *optarg;

std::string mk(const std::string x) {
    size_t pos = 0;
    std::string y = x;

    // if x is not just an integer (index), wrap it in quotes
    if (y.find_first_not_of("0123456789") != std::string::npos) {
        y = "'" + y + "'";
    }
}


int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"global",    required_argument, 0, 'G'},  // gg ggplot()
        {0, 0, 0, 0}
    };
    const char* short_options = ":G:x:y:c:s:";

    int layer_count = 0;
    int opt_ix = 0;
    int opt;
    int ret;
    char current_geom;
    while ((opt = getopt_long(argc, argv, short_options, long_options, &opt_ix)) != -1) {
        switch (opt) {
            case 'G':
                break;
            case 'x':
                std::cout << "x: " << mk(optarg) << std::endl;
                break;
            case 'y':
                std::cout << "y: " << mk(optarg) << std::endl;
                break;
        }
    }
}