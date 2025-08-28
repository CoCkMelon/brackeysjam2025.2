#include "JumpUpState.h"

JumpUpState::JumpUpState(Car* car) {
    this->car = car;
}

JumpUpState::~JumpUpState() {
    delete car;
}

void JumpUpState::moveCarForward() override {
    std::cout << "Switching from jumping to moving forward.\n";
    car->setCurrentState(car->getMoveForward());
}

void JumpUpState::moveCarBackward() override {
    std::cout << "Switchin from jumping to moving backward.\n";
    car->setCurrentState(car->getMoveBackward());
}

void JumpUpState::makeCarJump() override {
    std::cout << "Already jumping.\n";
}

void JumpUpState::switchOnBoost() override {
    std::cout << "Switching from jumping to turning on boost.\n";
    car->setCurrentstate(car->getBoostOn());
}

void JumpUpState::switchOfBoost() override {
    std::cout << "Switching from jumping to turning off boost.\n";
    car->setCurrentState(car->getBoostOff());
}
