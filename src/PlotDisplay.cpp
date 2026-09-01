#include "photo_booth/PlotDisplay.hpp"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

  void update(const cv::Mat& data, const std::string& x_label,
              const std::string& y_label) {
    const std::string x_axis_label = quoteGnuplotString(x_label);
    const std::string y_axis_label = quoteGnuplotString(y_label);

    std::fprintf(pipe_, "set xlabel %s\n", x_axis_label.c_str());

    std::fprintf(pipe_, "set ylabel %s\n", y_axis_label.c_str());

    std::fprintf(pipe_, "set xrange [0:%d]\n", data.cols - 1);

    std::fputs("set autoscale y\n", pipe_);

    //
    // Each row of the cv::Mat is displayed as one data series.
    //

    std::fputs("plot ", pipe_);

    for (int row = 0; row < data.rows; ++row) {
      if (row > 0) {
        std::fputs(", ", pipe_);
      }

      std::fprintf(pipe_, "'-' using 1:2 with lines linecolor rgb '%s' notitle",
                   lineColor(row));
    }

    std::fputc('\n', pipe_);

    //
    // Stream each row directly to Gnuplot.
    //

    for (int row = 0; row < data.rows; ++row) {
      for (int column = 0; column < data.cols; ++column) {
        std::fprintf(pipe_, "%d %.17g\n", column, data.at<double>(row, column));
      }

      std::fputs("e\n", pipe_);
    }

    std::fflush(pipe_);
  }

 private:
  std::FILE* pipe_{nullptr};
};

using PlotMap = std::unordered_map<std::string, std::unique_ptr<GnuplotWindow>>;

PlotMap& plotWindows() {
  static PlotMap windows;

  return windows;
}

}  // namespace

void showPlot(const cv::Mat& data, const std::string& window_name,
              const std::string& x_label, const std::string& y_label) {
  if (data.empty()) {
    throw std::invalid_argument("Plot data cannot be empty.");
  }

  if (data.channels() != 1) {
    throw std::invalid_argument("Plot data must be a single-channel cv::Mat.");
  }

  if (data.cols < 2) {
    throw std::invalid_argument("Plot data must contain at least two samples.");
  }

  cv::Mat plot_data;

  data.convertTo(plot_data, CV_64F);

  auto& windows = plotWindows();

  auto iterator = windows.find(window_name);

  if (iterator == windows.end()) {
    auto window = std::make_unique<GnuplotWindow>(window_name);

    iterator = windows.emplace(window_name, std::move(window)).first;
  }

  iterator->second->update(plot_data, x_label, y_label);
}

void hidePlot(const std::string& window_name) {
  plotWindows().erase(window_name);
}

}  // namespace photo_booth
