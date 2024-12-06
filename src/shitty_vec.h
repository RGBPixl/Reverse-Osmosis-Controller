#pragma once
#include <Arduino.h>

template <typename T> class ShittyVec {
private:
  T items[16];

public:
  uint len = 0;

  ShittyVec(std::initializer_list<T> items) : items({}) {
    for (auto item : items) {
      this->push(item);
    }
  };

  T &operator[](int index) { return this->items[index]; };
  // Shark really wanted this we never actually use it... ._.
  void push(T item) { this->items[this->len++] = item; };
};
