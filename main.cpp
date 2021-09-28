#include "Timer.hpp"
#include <iostream>

using namespace timer;

int main()
{   std::cout << "pocztake " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;
    SingleShotTimer::call(3000, []{std::cout << "dupa " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;});
    SingleShotTimer::call(100, []{std::cout << "dupa2 " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;});
    SingleShotTimer::call(300, []{std::cout << "dupa3 " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;});
    SingleShotTimer::call(10, []{std::cout << "dupa4 " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;});
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    return 0;
}
