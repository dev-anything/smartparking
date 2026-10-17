#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <mutex>
#include <opencv2/opencv.hpp>

#define ENTRY_CAM_INDEX 0
#define EXIT_CAM_INDEX 1



std::string buffer = {0};

std::mutex mtx;

void print_msg()
{
    int count = 1;
    std::string buf = "Hello, Multi threading world!";
    for (count; count <= 100; count++)
    {
        mtx.lock();
        buffer = buf;
        std::cout << "[" << count << "] " << buffer << std::endl;
        mtx.unlock();
        sleep(1);
    }
}

int main()
{
    std::thread t(print_msg);
    char buf[1024] = "Here is main thread.";
    t.join();

    for (int count = 1; count < 100; count++)
    {
        mtx.lock();
        buffer = buf;
        std::cout << "[" << count << "] " << buffer << std::endl;
        mtx.unlock();
        sleep(1);
    }


    return 0;
}