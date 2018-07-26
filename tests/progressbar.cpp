#include <iostream>
#include <thread>
#include "progressbar.h"

int main() {
    const std::size_t n = 1000;
    progressbar::ProgressBar bar(n, "test");
    for (std::size_t i = 0; i < n; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (i == 500) {
            bar.println("Hello, world!");
        }
        ++bar;
    }
    bar.close();
    std::cout << "done" << std::endl;
    return 0;
}
