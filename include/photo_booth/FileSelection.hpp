#pragma once

#include <filesystem>
#include <optional>

namespace photo_booth {

/**
 * @brief Prompts the user to select an image file.
 *
 * A graphical file dialog is used when available. In a headless environment,
 * the filename is requested from the terminal.
 *
 * @return The selected path, or std::nullopt if selection is cancelled.
 */
std::optional<std::filesystem::path> selectImageFile();

}  // namespace photo_booth
