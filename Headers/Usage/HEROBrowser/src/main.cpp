#include "../include/HEROBrowser.h"

int main(int argc, char *argv[]) {
  HEROBrowser browser;
  if (browser.init())
    browser.run();
  return 0;
}
