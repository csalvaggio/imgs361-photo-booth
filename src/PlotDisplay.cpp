#include "photo_booth/PlotDisplay.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace photo_booth {

namespace {

std::string quoteGnuplotString(const std::string& text) {
  std::string result{"'"};

  for (const char character : text) {
    if (character == '\\' || character == '\'') {
      result += '\\';
    }

    result += character;
  }

  result += '\'';

  return result;
}

const char* lineColor(const int row) {
  constexpr const char* colors[] = {"blue",    "green", "red",
                                    "magenta", "cyan",  "black"};

  constexpr int color_count =
      static_cast<int>(sizeof(colors) / sizeof(colors[0]));

  return colors[row % color_count];
}

void validatePlotData(const cv::Mat& data) {
  if (data.empty()) {
    throw std::invalid_argument("Plot data cannot be empty.");
  }

  if (data.channels() != 1) {
    throw std::invalid_argument("Plot data must be a single-channel cv::Mat.");
  }

  if (data.cols < 2) {
    throw std::invalid_argument("Plot data must contain at least two samples.");
  }
}

int plotWindowHeight(const std::size_t plot_count) {
  //
  // Allocate the same vertical space to every subplot so that a square plot
  // remains the same physical size as the analysis changes between one, two,
  // and three plots. Three or more plots are capped at a 960-pixel window.
  //
  constexpr int kHeightPerPlot = 320;
  constexpr int kMaximumPlotWindowHeight = 960;

  const int requested_height =
      kHeightPerPlot * static_cast<int>(plot_count);

  return std::min(requested_height, kMaximumPlotWindowHeight);
}

class GnuplotWindow {
 public:
  explicit GnuplotWindow(const std::string& window_name) {
    const std::string command = "\"" + std::string(GNUPLOT_EXECUTABLE) + "\"";

    pipe_ = popen(command.c_str(), "w");

    if (pipe_ == nullptr) {
      throw std::runtime_error("Unable to start gnuplot");
    }

    const std::string title = quoteGnuplotString(window_name);

    std::fprintf(pipe_, "set term qt noraise title %s\n", title.c_str());
    std::fputs("set key off\n", pipe_);
    std::fputs("set style textbox opaque noborder\n", pipe_);
    std::fflush(pipe_);
  }

  ~GnuplotWindow() {
    if (pipe_ != nullptr) {
      std::fputs("exit\n", pipe_);
      std::fflush(pipe_);
      ::pclose(pipe_);
    }
  }

  GnuplotWindow(const GnuplotWindow&) = delete;
  GnuplotWindow& operator=(const GnuplotWindow&) = delete;

  void update(const std::vector<Plot>& plots,
              const std::string& window_title) {
    const std::string quoted_window_title =
        quoteGnuplotString(window_title);

    //
    // Keep the window large enough that vertically stacked square plots remain
    // useful, while avoiding an excessively tall window for three plots.
    //
    // Do not resend the terminal size on every refresh. The Qt terminal handles
    // window resizing asynchronously, so repeatedly requesting the same size can
    // briefly redraw a multiplot using stale terminal dimensions. Only change
    // the terminal geometry when the number of subplots changes. Likewise, only
    // update the operating-system window title when the requested title changes.
    //
    constexpr int kPlotWindowWidth = 600;

    const bool plot_count_changed = plots.size() != plot_count_;
    const bool window_title_changed = window_title != window_title_;

    if (plot_count_changed) {
      const int window_height = plotWindowHeight(plots.size());

      std::fprintf(pipe_,
                   "set term qt size %d,%d noraise title %s\n",
                   kPlotWindowWidth, window_height,
                   quoted_window_title.c_str());

      plot_count_ = plots.size();
      window_title_ = window_title;
    } else if (window_title_changed) {
      std::fprintf(pipe_, "set term qt noraise title %s\n",
                   quoted_window_title.c_str());
      window_title_ = window_title;
    }

    //
    // Clear the previous frame before entering multiplot mode. This is
    // particularly important when the number of stacked plots changes.
    // Start from normal sizing; each subplot is explicitly made square below
    // after multiplot has assigned that subplot's origin and nominal size.
    //
    std::fputs("clear\n", pipe_);
    std::fputs("unset title\n", pipe_);
    std::fputs("set size nosquare\n", pipe_);
    std::fprintf(pipe_, "set multiplot layout %zu,1 rowsfirst\n",
                 plots.size());

    for (const auto& plot : plots) {
      const std::string subplot_title = quoteGnuplotString(plot.title);
      const std::string x_axis_label = quoteGnuplotString(plot.x_label);
      const std::string y_axis_label = quoteGnuplotString(plot.y_label);

      //
      // Put the subplot name inside the graph rather than reserving vertical
      // space above it. The boxed label uses Gnuplot's opaque textbox style so
      // plot curves do not make the label difficult to read.
      //
      std::fputs("unset title\n", pipe_);

      //
      // A multiplot layout establishes a fresh nominal size for each subplot.
      // Apply square sizing here, after that layout geometry has been selected,
      // so every subplot is square on every refresh.
      //
      std::fputs("set size square\n", pipe_);

      if (plot.title.empty()) {
        std::fputs("unset label 1\n", pipe_);
      } else {
        std::fprintf(pipe_,
                     "set label 1 %s at graph 0.97,0.95 right front boxed\n",
                     subplot_title.c_str());
      }

      std::fprintf(pipe_, "set xlabel %s\n", x_axis_label.c_str());
      std::fprintf(pipe_, "set ylabel %s\n", y_axis_label.c_str());
      std::fprintf(pipe_, "set xrange [0:%d]\n", plot.data.cols - 1);

      if (plot.y_min && plot.y_max) {
        std::fprintf(pipe_, "set yrange [%.17g:%.17g]\n", *plot.y_min,
                     *plot.y_max);
      } else if (plot.y_min) {
        std::fprintf(pipe_, "set yrange [%.17g:*]\n", *plot.y_min);
      } else if (plot.y_max) {
        std::fprintf(pipe_, "set yrange [*:%.17g]\n", *plot.y_max);
      } else {
        std::fputs("set autoscale y\n", pipe_);
      }

      //
      // Each row of the cv::Mat is displayed as one data series.
      //
      std::fputs("plot ", pipe_);

      for (int row = 0; row < plot.data.rows; ++row) {
        if (row > 0) {
          std::fputs(", ", pipe_);
        }

        std::fprintf(
            pipe_, "'-' using 1:2 with lines linecolor rgb '%s' notitle",
            lineColor(row));
      }

      std::fputc('\n', pipe_);

      //
      // Stream each row directly to Gnuplot.
      //
      for (int row = 0; row < plot.data.rows; ++row) {
        for (int column = 0; column < plot.data.cols; ++column) {
          std::fprintf(pipe_, "%d %.17g\n", column,
                       plot.data.at<double>(row, column));
        }

        std::fputs("e\n", pipe_);
      }

      std::fputs("unset label 1\n", pipe_);
    }

    std::fputs("unset multiplot\n", pipe_);
    std::fputs("set size nosquare\n", pipe_);
    std::fflush(pipe_);
  }

 private:
  std::FILE* pipe_{nullptr};
  std::size_t plot_count_{0};
  std::string window_title_;
};

using PlotMap = std::unordered_map<std::string, std::unique_ptr<GnuplotWindow>>;

PlotMap& plotWindows() {
  static PlotMap windows;

  return windows;
}

}  // namespace

void showPlots(const std::vector<Plot>& plots, const std::string& window_name,
               const std::string& window_title) {
  if (plots.empty()) {
    throw std::invalid_argument("At least one plot must be supplied.");
  }

  std::vector<Plot> converted_plots;
  converted_plots.reserve(plots.size());

  for (const auto& plot : plots) {
    validatePlotData(plot.data);

    Plot converted_plot = plot;
    plot.data.convertTo(converted_plot.data, CV_64F);

    converted_plots.push_back(std::move(converted_plot));
  }

  auto& windows = plotWindows();

  auto iterator = windows.find(window_name);

  if (iterator == windows.end()) {
    auto window = std::make_unique<GnuplotWindow>(window_name);

    iterator = windows.emplace(window_name, std::move(window)).first;
  }

  iterator->second->update(converted_plots, window_title);
}

void showPlot(const cv::Mat& data, const std::string& window_name,
              const std::string& x_label, const std::string& y_label) {
  showPlots({Plot{data, "", x_label, y_label, std::nullopt, std::nullopt}},
            window_name, window_name);
}

void hidePlot(const std::string& window_name) {
  plotWindows().erase(window_name);
}

}  // namespace photo_booth
