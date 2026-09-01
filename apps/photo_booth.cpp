#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <string>

#include "photo_booth/AppConfig.hpp"
#include "photo_booth/ImageCapture.hpp"
#include "photo_booth/ImageProcessing.hpp"
#include "photo_booth/PlotDisplay.hpp"

namespace {

constexpr char kHistogramWindowName[] = "Photo Booth - Histogram";

constexpr double kFpsUpdateIntervalSeconds = 1.0;
constexpr int kHistogramUpdateInterval = 10;

struct ProcessingState {
  bool inversion_enabled{false};
  bool histogram_enabled{false};
  bool performance_overlay_enabled{false};
};

cv::Mat processFrame(const cv::Mat& frame,
                     const photo_booth::ProcessingConfig& config,
                     const ProcessingState& state) {
  cv::Mat processed_frame = frame.clone();

  /**
   * Image-processing operations
   *
   * Operations are placed in the pipeline according to the image
   * representation they expect and the desired processing order, not according
   * to whether they are controlled by configuration or interactively.
   */

  if (state.inversion_enabled) {
    processed_frame = photo_booth::invertImage(processed_frame);
  }

  if (config.channel_swap_enabled) {
    processed_frame = photo_booth::swapRedBlueChannels(processed_frame);
  }

  return processed_frame;
}

void showPreviewFrame(const cv::Mat& frame,
                      const photo_booth::PreviewConfig& config,
                      const bool performance_overlay_enabled,
                      const double current_fps) {
  cv::Mat preview_frame = frame.clone();

  switch (config.rotation) {
    case 0:
      break;

    case 90:
      cv::rotate(preview_frame, preview_frame, cv::ROTATE_90_CLOCKWISE);
      break;

    case 180:
      cv::rotate(preview_frame, preview_frame, cv::ROTATE_180);
      break;

    case 270:
      cv::rotate(preview_frame, preview_frame, cv::ROTATE_90_COUNTERCLOCKWISE);
      break;
  }

  if (config.mirror) {
    cv::flip(preview_frame, preview_frame, 1);
  }

  //
  // Add display-only performance information after preview transformations.
  // This overlay does not modify the processed image.
  //
  if (performance_overlay_enabled) {
    const std::string fps_text = cv::format("FPS: %.1f", current_fps);
    const cv::Point text_origin{20, 40};

    //
    // Draw a dark outline first so the text remains readable over both
    // bright and dark image regions.
    //
    cv::putText(preview_frame, fps_text, text_origin, cv::FONT_HERSHEY_SIMPLEX,
                0.8, cv::Scalar(0, 0, 0), 4, cv::LINE_AA);

    cv::putText(preview_frame, fps_text, text_origin, cv::FONT_HERSHEY_SIMPLEX,
                0.8, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
  }

  cv::imshow(config.window_name, preview_frame);
}

void resetProcessingState(ProcessingState& state) {
  //
  // Close any auxiliary windows associated with the current runtime state.
  //
  if (state.histogram_enabled) {
    photo_booth::hidePlot(kHistogramWindowName);
  }

  //
  // Restore all runtime processing, analysis, and display settings to their
  // initial values. Configuration-controlled baseline operations are unchanged.
  //
  state = ProcessingState{};

  std::cout << "Photo Booth RESET: All runtime processing and display options "
               "OFF\n";
}

void printControls() {
  std::cout << "\nPhoto Booth controls:\n"
            << "\n"
            << "  Processing\n"
            << "    n      Toggle image negative/inversion\n"
            << "\n"
            << "  Analysis / display\n"
            << "    h      Toggle histogram display\n"
            << "    p      Toggle performance overlay\n"
            << "\n"
            << "  Application\n"
            << "    Space  Capture image\n"
            << "    Esc    Reset to startup state\n"
            << "    ?      Show controls\n"
            << "    q      Quit\n"
            << '\n';
}

bool handleKey(const int key, ProcessingState& state) {
  switch (key) {
    //
    // Application controls.
    //
    case 'q':
    case 'Q':
      return false;

    case 27:
      resetProcessingState(state);
      break;

    case '?':
      printControls();
      break;

    //
    // Basic image processing.
    //
    case 'n':
    case 'N':
      state.inversion_enabled = !state.inversion_enabled;

      std::cout << "Image inversion: "
                << (state.inversion_enabled ? "ON" : "OFF") << '\n';
      break;

    //
    // Analysis and display.
    //
    case 'h':
    case 'H':
      state.histogram_enabled = !state.histogram_enabled;

      std::cout << "Histogram display: "
                << (state.histogram_enabled ? "ON" : "OFF") << '\n';

      if (!state.histogram_enabled) {
        photo_booth::hidePlot(kHistogramWindowName);
      }
      break;

    case 'p':
    case 'P':
      state.performance_overlay_enabled = !state.performance_overlay_enabled;

      std::cout << "Performance overlay: "
                << (state.performance_overlay_enabled ? "ON" : "OFF") << '\n';
      break;

    default:
      break;
  }

  return true;
}

std::string makeTimestampFilename() {
  const auto now = std::chrono::system_clock::now();

  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

  std::tm utc_time{};
#if defined(_WIN32)
  gmtime_s(&utc_time, &now_time);
#else
  gmtime_r(&now_time, &utc_time);
#endif

  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch()) %
      1000;

  std::ostringstream filename;

  filename << std::put_time(&utc_time, "%Y-%m-%dT%H-%M-%S") << '.'
           << std::setw(3) << std::setfill('0') << milliseconds.count()
           << ".png";

  return filename.str();
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    //
    // Determine which configuration file to use.
    //
    std::filesystem::path config_path{"config.toml"};

    if (argc > 2) {
      std::cerr << "Usage: " << argv[0] << " [config.toml]\n";

      return EXIT_FAILURE;
    }

    if (argc == 2) {
      const std::string argument{argv[1]};

      if (argument == "-h" || argument == "--help") {
        std::cout << "Usage: " << argv[0] << " [config.toml]\n\n"
                  << "Runs the semester photo-booth "
                     "image-processing application.\n";

        return EXIT_SUCCESS;
      }

      config_path = argument;
    }

    //
    // Load the application configuration.
    //
    const auto config = photo_booth::loadConfig(config_path);

    //
    // Configure and open the camera.
    //
    photo_booth::ImageCapture camera(
        photo_booth::makeImageCaptureConfiguration(config.camera));

    if (!camera.open()) {
      std::cerr << camera.errorMessage() << '\n';

      return EXIT_FAILURE;
    }

    std::cout << camera << '\n';

    //
    // Create the save directory if it does not already exist.
    //
    const std::filesystem::path save_directory{config.capture.save_directory};

    std::filesystem::create_directories(save_directory);
    std::cout << "Capture directory: " << save_directory << '\n';

    //
    // Create the preview window.
    //
    cv::namedWindow(config.preview.window_name, cv::WINDOW_AUTOSIZE);

    //
    // Runtime state for optional processing, analysis, and display operations.
    //
    ProcessingState processing_state;

    //
    // Display the available keyboard controls.
    //
    printControls();

    //
    // State used to control the histogram display refresh rate.
    //
    // The histogram plot is intentionally refreshed less frequently than the
    // camera preview because updating the external Gnuplot display every frame
    // can block the main application loop. Initialize the counter so the first
    // histogram is displayed immediately when the feature is enabled.
    //
    int histogram_update_counter = kHistogramUpdateInterval - 1;

    //
    // State used to measure the effective application frame rate.
    //
    auto fps_interval_start = std::chrono::steady_clock::now();
    int fps_frame_count = 0;
    double current_fps = 0.0;

    //
    // Main application loop.
    //
    while (true) {
      if (!camera.read()) {
        std::cerr << camera.errorMessage() << '\n';

        return EXIT_FAILURE;
      }

      //
      // Apply the image-processing pipeline.
      //
      cv::Mat processed_frame =
          processFrame(camera.image(), config.processing, processing_state);

      //
      // Calculate and display the histogram, if enabled.
      //
      // The histogram plot is refreshed only once every
      // kHistogramUpdateInterval frames so the external Gnuplot display does
      // not unnecessarily limit the camera-processing frame rate.
      //
      if (processing_state.histogram_enabled) {
        ++histogram_update_counter;

        if (histogram_update_counter >= kHistogramUpdateInterval) {
          const cv::Mat histogram = photo_booth::calcHist(processed_frame);

          photo_booth::showPlot(histogram, kHistogramWindowName,
                                "Digital Count", "Number of Pixels");

          histogram_update_counter = 0;
        }
      } else {
        //
        // Prime the counter so the histogram is updated immediately the next
        // time the display is enabled.
        //
        histogram_update_counter = kHistogramUpdateInterval - 1;
      }

      //
      // Update the measured application frame rate.
      //
      ++fps_frame_count;

      const auto now = std::chrono::steady_clock::now();

      const double elapsed =
          std::chrono::duration<double>(now - fps_interval_start).count();

      if (elapsed >= kFpsUpdateIntervalSeconds) {
        current_fps = fps_frame_count / elapsed;

        fps_frame_count = 0;
        fps_interval_start = now;
      }

      //
      // Display the processed frame and optional performance overlay.
      //
      showPreviewFrame(processed_frame, config.preview,
                       processing_state.performance_overlay_enabled,
                       current_fps);

      //
      // Process keyboard input.
      //
      const int key = cv::waitKey(1);

      if (!handleKey(key, processing_state)) {
        break;
      }

      if (key == ' ') {
        const auto filename = save_directory / makeTimestampFilename();

        if (cv::imwrite(filename.string(), processed_frame)) {
          std::cout << "Captured: " << filename << '\n';
        } else {
          std::cerr << "Failed to save image: " << filename << '\n';
        }
      }
    }

    cv::destroyAllWindows();
  } catch (const cv::Exception& error) {
    std::cerr << "OpenCV error: " << error.what() << '\n';

    return EXIT_FAILURE;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';

    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
