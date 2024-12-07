#pragma once
#include "page.h"
#include <Arduino.h>
#include <Vector.h>

class MenuEntry {
private:
  Vector<MenuPage> pages;
  int currentPage;

public:
  MenuEntry(std::initializer_list<MenuPage> pages);

public:
  inline void next() { currentPage = currentPage < pages.size() - 1 ? currentPage++ : 0; };
  inline void prev() { currentPage = currentPage >= 0 ? currentPage-- : 0; };

  inline void goTo(int page) { currentPage = currentPage < this->pages.size() ? page : 0; };
  inline MenuPage getCurrentPage() { return pages[this->currentPage]; };
  inline int getCurrentPageInt() { return this->currentPage; };
  inline void reset() { this->currentPage = 0; };
};
