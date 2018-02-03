#include "Mosquitto.h"

#include <signal.h>
#include <iostream>
#include <thread>
#include <unistd.h>


using namespace std;

volatile bool run = true;

void handle_signal(int s)
{
    run = false;
    clog << "signaled" << endl;
}

int main() {

    if (true) {
            {
            Mosquitto m3{"m3", "localhost", 1883, &run};
            m3.sendMessage("hello", "0", true);
        }
    }

    Mosquitto m2("m2", "localhost", 1883, &run);
//    m2.subscribe("hello");


    Mosquitto m("m", "localhost", 1883, &run);
    m.subscribe("hello");
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    std::thread th(&Mosquitto::run, &m, 500);
//    std::thread th2(&Mosquitto::run, &m2);
    m2.sendMessage("hello", "Hola");
    m2.sendMessage("hello", "Hola2");
    m2.sendMessage("hello", "Hola3");
    sleep(1);
    run= false;
    th.join();
//    th2.join();
    clog << "finished" << endl;
    return 0;
}
