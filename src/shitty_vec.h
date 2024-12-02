#pragma once
#include <Arduino.h>

template <typename T>
class ShittyVec {
public:
  uint len;

  ShittyVec(std::initializer_list<T> items);

  const T& operator[](int index) const;
  //Shark really wanted this we never actually use it... ._.
  void push(T) const;
private:
  T items[16];
};

