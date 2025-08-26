#include "MoveBackwardState.h"

MoveBackwardState::MoveBackwardState(Car* car) {
  this->car = car;
}

void MoveBackwardState::moveCarForward() override {
  std::cout << "Switching from moving backward to forward.\n";
  car->setCurrentState(car->getMoveForward());
}

void MoveBackwardState::moveCarBackward() override {
  std::cout << "Already moving backward.\n";
}

void MoveBackwardState::makeCarJump() override {
  std::cout << "Jumping while moving backward.\n";
  car->setCurrentState(car->getJumpUp());
}

void MoveBackwardState::switchOnBoost() override {
  std::cout << "Cannot boost while moving backward.\n";
}

void MoveBackwardState::switchOffBoost() override {
  std::cout << "Boost already off while moving backward.\n";
}
