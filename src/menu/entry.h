#include <ArduinoSTL.h>

class MenuEntry {
private:
  std::vector<MenuPage> items;
  int currentPage;

public:
  MenuEntry(std::initializer_list<MenuPage> pages);
  void next();
  void prev();
  void reset();
  void goTo(int page);
  MenuPage getCurrentPage();
};
