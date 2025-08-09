#include "mock_arduino.hpp"
#include "../src/chess_engine.hpp"

MockSerial Serial;

int main(){
    initEngine();
    int score = evaluate();
    std::cout << "Initial score: " << score << std::endl;
    return 0;
}
