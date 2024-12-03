#pragma once
#include "page.h"
#include <Arduino.h>
#include <Vector.h>

class MenuEntry {
private:
  Vector<MenuPage> items;
  int currentPage;

public:
  MenuEntry(std::initializer_list<MenuPage> pages);
  bool next();
  bool prev();
  void reset();
  void goTo(int page);
  MenuPage getCurrentPage();
  int getCurrentPageInt();
};
