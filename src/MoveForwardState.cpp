#include "MoveForwardState.h"

MoveForwardState::MoveForwardState(Car* car) {
  this->car = car;
}

MoveForwardState::~MoveForwardState() {
  delete car;
}

void MoveForwardState::moveCarForward() override {
  std::cout << "Already moving forward.\n";
}

void MoveForwardState::moveCarBackward() override {
  std::cout << "Switching from moving forward to backward.\n";
  car->setCurrentState(car->getMoveBackward());
}

void MoveForwardState::makeCarJump() override {
  std::cout << "Jumping while moving forward.\n";
  car->setCurrentState(car->getJumpUp());
}

void MoveForwardState::switchOnBoost() override {
  std::cout << "Activating boost while moving forward.\n";
  car->setCurrentState(car->getBoostOn());
}

void MoveForwardState::swtichOffBoost() override {
  std::cout << "Boost already off while moving forward.\n";
}
