#include "photo_booth/FileSelection.hpp"

#include <portable-file-dialogs.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace photo_booth {

namespace {

bool graphicalFileDialogAvailable() {
#if defined(__linux__)

  // On Linux, a dialog backend such as Zenity may be installed even when
  // the program is running in a headless SSH session.  Require both a
  // graphical session and an available PFD backend.
  const bool graphical_session = std::getenv("DISPLAY") != nullptr ||
                                 std::getenv("WAYLAND_DISPLAY") != nullptr;

  return graphical_session && pfd::settings::available();

#else

  return pfd::settings::available();

#endif
}

std::optional<std::filesystem::path> selectImageFileFromTerminal() {
  std::cout << "Image filename "
            << "(leave blank to cancel): ";

  std::string filename;
  std::getline(std::cin, filename);

  if (filename.empty()) {
    return std::nullopt;
  }

  return std::filesystem::path(filename);
}

}  // namespace

std::optional<std::filesystem::path> selectImageFile() {
  if (!graphicalFileDialogAvailable()) {
    return selectImageFileFromTerminal();
  }

  const auto selection =
      pfd::open_file("Select Image", ".",
                     {"Image Files", "*.png *.jpg *.jpeg *.bmp *.tif *.tiff",
                      "All Files", "*"})
          .result();

  if (selection.empty()) {
    return std::nullopt;
  }

  return std::filesystem::path(selection.front());
}

}  // namespace photo_booth
