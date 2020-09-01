#include "demo_tantalum.h"

int main(int argc, char** argv)
{
    DemoTantalum* window = new DemoTantalum();

    window->mainLoop();

    return 0;
}
