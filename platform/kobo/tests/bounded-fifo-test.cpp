#include <cstdlib>

#include "BoundedFifo.h"

struct Item {
  int value = 0;
};

int main() {
  BoundedFifo<Item, 8> queue;
  for (int value = 0; value < 8; ++value) {
    if (!queue.push({value})) return EXIT_FAILURE;
  }
  if (queue.push({8}) || queue.size() != 8) return EXIT_FAILURE;

  for (int value = 0; value < 8; ++value) {
    Item item{};
    if (!queue.pop(item) || item.value != value) return EXIT_FAILURE;
  }

  Item item{};
  return queue.pop(item) || queue.size() != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
