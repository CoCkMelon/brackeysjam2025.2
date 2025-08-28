#include "GameMediator.h"

GameMediator::GameMediator(Car* car) {
    this->car = car;
}

void GameMediator::notify(std::string event) override {
    if (event == "CarDamaged") {
        std::cout << "[Mediator] Car was hit! Updating UI.\n";
    }
}
