#include "shitty_vec.h"
#include <Arduino.h>
#include <algorithm>


template <typename T> ShittyVec<T>::ShittyVec(std::initializer_list<T> items) {
    this.len = items.size();
    this.items = items._M_array;
}

template <typename T> const void ShittyVec<T>::push(T item) const {
    this.items[this.len++] = item;
}

template <typename T> const T& ShittyVec<T>::operator[](int index) const {
    return this->items[index];
}