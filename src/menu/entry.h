#include <ArduinoSTL.h>
#include "page.h"

class MenuEntry {
private:
  std::vector<MenuPage> items;
  int currentPage;

public:
  MenuEntry(std::initializer_list<MenuPage> pages) : items(pages) {
    this->currentPage = 0;
  }
  void next() {
    this->currentPage++;
    if (this->currentPage >= this->items.size()) {
      this->currentPage = 0;
    }
  }
  void prev() {
    this->currentPage--;
    if (this->currentPage < 0) {
      this->currentPage = this->items.size() - 1;
    }
  }
  void reset() { this->currentPage = 0; }
  void goTo(int page) {
    this->currentPage = page;
    if (this->currentPage >= this->items.size()) {
      this->currentPage = 0;
    }
  }
  MenuPage getCurrentPage() { return this->items[this->currentPage]; }
};
