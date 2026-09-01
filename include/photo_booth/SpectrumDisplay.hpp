#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace photo_booth {

/**
 * Select which Fourier-domain quantity is displayed.
 */
enum class SpectrumDisplayMode { Magnitude, Phase };

/**
 * Display a complex Fourier spectrum in an OpenCV window.
 *
 * The spectrum must be a two-channel CV_32FC2 matrix, with the real and
 * imaginary Fourier coefficients stored in the two channels. The selected
 * display is shifted so that zero spatial frequency appears at the center.
 *
 * Magnitude is logarithmically scaled before display. Phase is displayed over
 * the range [0, 2*pi).
 *
 * @param spectrum     Complex Fourier spectrum to display.
 * @param window_name  Name of the OpenCV display window.
 * @param mode         Fourier-domain quantity to display.
 */
void showSpectrum(const cv::Mat& spectrum, const std::string& window_name,
                  SpectrumDisplayMode mode);

/**
 * Close a spectrum window.
 *
 * This should only be called for a window that has previously been displayed
 * with showSpectrum().
 *
 * @param window_name Name of the OpenCV display window.
 */
void hideSpectrum(const std::string& window_name);

}  // namespace photo_booth
