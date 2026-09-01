#include "photo_booth/SpectrumDisplay.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace photo_booth {

namespace {

using WindowSet = std::unordered_set<std::string>;

WindowSet& spectrumWindows() {
  static WindowSet windows;

  return windows;
}

void validateSpectrum(const cv::Mat& spectrum) {
  if (spectrum.empty()) {
    throw std::invalid_argument("Spectrum data cannot be empty.");
  }

  if (spectrum.type() != CV_32FC2) {
    throw std::invalid_argument("Spectrum data must be CV_32FC2.");
  }
}

cv::Mat shiftFourierOriginToCenter(const cv::Mat& image) {
  if (image.empty()) {
    return {};
  }

  cv::Mat horizontally_shifted;

  if (image.cols > 1) {
    const int column_split = image.cols - image.cols / 2;

    cv::hconcat(image.colRange(column_split, image.cols),
                image.colRange(0, column_split), horizontally_shifted);
  } else {
    horizontally_shifted = image.clone();
  }

  cv::Mat shifted;

  if (image.rows > 1) {
    const int row_split = image.rows - image.rows / 2;

    cv::vconcat(horizontally_shifted.rowRange(row_split, image.rows),
                horizontally_shifted.rowRange(0, row_split), shifted);
  } else {
    shifted = horizontally_shifted.clone();
  }

  return shifted;
}

cv::Mat createMagnitudeDisplay(const cv::Mat& real, const cv::Mat& imaginary) {
  cv::Mat magnitude;
  cv::magnitude(real, imaginary, magnitude);

  // Compress the large dynamic range of the Fourier magnitude spectrum.
  magnitude += 1.0F;
  cv::log(magnitude, magnitude);

  magnitude = shiftFourierOriginToCenter(magnitude);

  cv::normalize(magnitude, magnitude, 0.0, 255.0, cv::NORM_MINMAX);

  cv::Mat display;
  magnitude.convertTo(display, CV_8U);

  return display;
}

cv::Mat createPhaseDisplay(const cv::Mat& real, const cv::Mat& imaginary) {
  cv::Mat phase;
  cv::phase(real, imaginary, phase, false);

  phase = shiftFourierOriginToCenter(phase);

  cv::Mat display;
  phase.convertTo(display, CV_8U, 255.0 / (2.0 * CV_PI));

  return display;
}

cv::Mat createSpectrumDisplay(const cv::Mat& spectrum,
                              const SpectrumDisplayMode mode) {
  std::vector<cv::Mat> planes;
  cv::split(spectrum, planes);

  switch (mode) {
    case SpectrumDisplayMode::Magnitude:
      return createMagnitudeDisplay(planes[0], planes[1]);

    case SpectrumDisplayMode::Phase:
      return createPhaseDisplay(planes[0], planes[1]);
  }

  throw std::invalid_argument("Unknown spectrum display mode.");
}

}  // namespace

void showSpectrum(const cv::Mat& spectrum, const std::string& window_name,
                  const SpectrumDisplayMode mode) {
  validateSpectrum(spectrum);

  const cv::Mat display = createSpectrumDisplay(spectrum, mode);

  auto& windows = spectrumWindows();

  if (!windows.contains(window_name)) {
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
    windows.insert(window_name);
  }

  cv::imshow(window_name, display);
}

void hideSpectrum(const std::string& window_name) {
  auto& windows = spectrumWindows();
  const auto iterator = windows.find(window_name);

  if (iterator == windows.end()) {
    return;
  }

  cv::destroyWindow(window_name);
  windows.erase(iterator);
}

}  // namespace photo_booth
