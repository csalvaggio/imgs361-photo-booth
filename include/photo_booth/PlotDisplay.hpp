#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace photo_booth {

/**
 * Display one or more sampled curves in a Gnuplot window.
 *
 * Each row of data represents one curve and each column represents one
 * sample.
 *
 * For three-row data, the curves are displayed as blue, green, and red,
 * corresponding to the canonical BGR channel ordering used by the
 * application.
 *
 * The plot is automatically scaled vertically to the largest value in
 * the supplied data.
 *
 * @param data         Matrix containing the curves to display.
 * @param window_name  Name of the Gnuplot window.
 * @param x_label      Label for the horizontal axis.
 * @param y_label      Label for the vertical axis.
 */
void showPlot(const cv::Mat& data, const std::string& window_name,
              const std::string& x_label, const std::string& y_label);

/**
 * Close a plot window.
 *
 * This should only be called for a window that has previously been
 * displayed with showPlot().
 *
 * @param window_name Name of the Gnuplot window.
 */
void hidePlot(const std::string& window_name);

}  // namespace photo_booth
