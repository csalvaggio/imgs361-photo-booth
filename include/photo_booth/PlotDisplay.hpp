#pragma once

#include <opencv2/core.hpp>
#include <optional>
#include <string>
#include <vector>

namespace photo_booth {

/**
 * Description of one sampled plot in a Gnuplot window.
 *
 * Each row of data represents one curve and each column represents one
 * sample. For three-row data, the curves are displayed as blue, green, and
 * red, corresponding to the canonical BGR channel ordering used by the
 * application.
 *
 * The plot title is drawn inside the graph near the upper-right corner using
 * an opaque textbox so it does not consume space above the plot.
 *
 * If y_min or y_max is not specified, that bound is automatically scaled by
 * Gnuplot.
 */
struct Plot {
  cv::Mat data;

  std::string title;
  std::string x_label;
  std::string y_label;

  std::optional<double> y_min;
  std::optional<double> y_max;
};

/**
 * Display one or more sampled plots stacked vertically in a Gnuplot window.
 *
 * Each graph is displayed with a square aspect ratio. The window title may be
 * changed independently of window_name so callers can keep one persistent
 * Gnuplot window while updating its descriptive operating-system title.
 *
 * @param plots         Plot descriptions to display.
 * @param window_name   Stable name used to identify the Gnuplot window.
 * @param window_title  Title displayed by the operating-system window.
 */
void showPlots(const std::vector<Plot>& plots, const std::string& window_name,
               const std::string& window_title);

/**
 * Display one or more sampled curves in a Gnuplot window.
 *
 * This convenience function preserves the original single-plot interface.
 * Each row of data represents one curve and each column represents one
 * sample.
 *
 * For three-row data, the curves are displayed as blue, green, and red,
 * corresponding to the canonical BGR channel ordering used by the
 * application.
 *
 * The plot is automatically scaled vertically to the largest value in
 * the supplied data and is displayed with a square aspect ratio.
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
 * displayed with showPlot() or showPlots().
 *
 * @param window_name Name of the Gnuplot window.
 */
void hidePlot(const std::string& window_name);

}  // namespace photo_booth
