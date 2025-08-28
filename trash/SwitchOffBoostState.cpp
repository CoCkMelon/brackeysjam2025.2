#include "SwitchOffBoostState.h"

SwitchOffBoostState::SwitchOffBoostState(Car* car) {
    this->car = car;
}

SwitchOffBoostState::~SwitchOffBoostState() {
    delete car;
}

void SwitchOffBoostState::moveCarForward() override {
    std::cout << "Switch from boost off to move car forward.\n";
    car->setCurrentState(car->getMoveForward());
}

void SwitchOffBoostState::moveCarBackward() override {
    std::cout << "Switch from boost off to move car backward.\n";
    car->setCurrentState(car->getMoveBackward());
}

void SwitchOffBoostState::makeCarJump() override {
    std::cout << "Switch from boost off to car jump.\n";
    car->setCurrentState(car->getJumpUp());
}

void SwitchOffBoostState::switchOnBoost() override {
    std::cout << "Switch from boost off to boost on.\n";
    car->setCurrentState(car->getBoostOn());
}

void SwitchOffBoostState::switchOffBoost() override {
    std::cout << "Boost is already off.\n";
}
