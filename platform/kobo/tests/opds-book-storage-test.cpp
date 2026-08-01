#include <cstdlib>
#include <iostream>
#include <string>

#include "network/OpdsBookStorage.h"

namespace {

[[noreturn]] void fail(const char* const message) {
  std::cerr << "opds book storage test failed: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  const std::string authorTitle = OpdsBookStorage::downloadPath("srv", false, "entry-42", "A / title", "An: author");
  const std::string titleAuthor = OpdsBookStorage::downloadPath("srv", true, "entry-42", "A / title", "An: author");
  if (authorTitle.rfind("/Books/OPDS/srv/An_ author - A _ title-", 0) != 0) fail("author-title path");
  if (titleAuthor.rfind("/Books/OPDS/srv/A _ title - An_ author-", 0) != 0) fail("title-author path");
  if (authorTitle == titleAuthor) fail("filename format ignored");
  if (authorTitle.substr(authorTitle.size() - 5) != ".epub") fail("epub suffix missing");
  if (OpdsBookStorage::downloadPath("srv", false, "entry-42", "same", "author") !=
      OpdsBookStorage::downloadPath("srv", false, "entry-42", "same", "author")) {
    fail("path is not deterministic");
  }
  if (OpdsBookStorage::downloadPath("srv", false, "entry-42", "same", "author") ==
      OpdsBookStorage::downloadPath("srv", false, "entry-43", "same", "author")) {
    fail("stable entry suffix missing");
  }
  return EXIT_SUCCESS;
}
