#include "entry.h"
#include "page.h"
#include <Arduino.h>

MenuEntry::MenuEntry(std::initializer_list<MenuPage> pages) : items(pages) {
  this->currentPage = 0;
}

bool MenuEntry::next() {
  this->currentPage++;
  if (this->currentPage >= this->items.len) {
    this->currentPage = 0;
    return false;
  }
  return true;
}

bool MenuEntry::prev() {
  this->currentPage--;
  if (this->currentPage < 0) {
    this->currentPage = this->items.len - 1;
    return false;
  }
  return true;
}

void MenuEntry::reset() { this->currentPage = 0; }
void MenuEntry::goTo(int page) {
  this->currentPage = page;
  if (this->currentPage >= this->items.len) {
    this->currentPage = 0;
  }
}
MenuPage MenuEntry::getCurrentPage() { return this->items[this->currentPage]; }
int MenuEntry::getCurrentPageInt() { return this->currentPage; }
