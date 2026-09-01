# Photo Booth

`Photo Booth` is the starting C++20 project for the semester-long photo booth assignment in **IMGS.361 Image Processing**. The starter code provides camera acquisition, typed TOML configuration, reusable display/file-selection support, a minimal live-preview example, and a closely related photo-booth application with an explicit image-processing pipeline for students to extend throughout the semester.

The project intentionally separates reusable support code from application-specific behavior. Camera acquisition, configuration loading, starter image-processing functions, file selection, and reusable plot/spectrum display support live in the `photo_booth` namespace; the applications contain their own control flow and runtime state.

## Starter applications

The project builds three executables:

- `live_preview` - minimal camera acquisition and display example
- `photo_booth` - semester project baseline with an explicit processing pipeline
- `capture_single_image` - diagnostic/example utility that saves one acquired frame

### `live_preview`

`apps/live_preview.cpp` intentionally has no `processFrame()` function. It acquires each camera frame, applies only display-oriented preview rotation/mirroring, and displays the result:

```text
camera -> display
```

This provides a simple reference showing that an explicit processing stage is not required merely to acquire and display camera imagery.

### `photo_booth`

`apps/photo_booth.cpp` inserts an explicit processing stage between acquisition and display:

```text
camera -> processFrame() -> processed frame -> display
```

The starter pipeline demonstrates two different kinds of processing controls:

- **Baseline operation:** `swapRedBlueChannels()` is controlled by `processing.channel_swap_enabled` in `config.toml` and, when enabled, is applied automatically while the application runs.
- **Optional runtime operation:** `invertImage()` is toggled interactively by pressing `n`. Its runtime state is independent of other optional operations that may be added later.
- **Analysis operation:** `calcHist()` calculates the histogram of the completed processed frame. Pressing `h` toggles a live histogram display using the supplied `PlotDisplay` component.

These simple operations and the supplied histogram analysis are intended to demonstrate architecture rather than serve as substantive course algorithms.

Whether an operation is baseline or optional determines **how it is controlled**, not where it belongs in the processing pipeline. Pipeline order should instead be chosen according to the meaning of each operation, the image representation it expects, and the representation it produces.

The application also demonstrates two display-oriented features that do not modify the processed image. Pressing `h` toggles a live histogram calculated from the completed `processed_frame`, while pressing `p` toggles the performance overlay. Additional analysis and visualization operations can similarly inspect the completed `processed_frame` after `processFrame()` without becoming part of the image-transformation pipeline itself.

The available keyboard controls are printed when `photo_booth` starts and can be displayed again at any time by pressing `?`. Pressing `Esc` resets all runtime processing, analysis, and display state to its startup values; configuration-controlled baseline operations remain as specified in `config.toml`.

## Image representation convention

Successful `ImageCapture::read()` calls provide **8-bit, three-channel BGR images (`CV_8UC3`)**. This is the canonical image representation for the Photo Booth processing pipeline.

Image-processing operations should accept and return this canonical `CV_8UC3` representation unless there is a specific reason not to. Even an operation that conceptually produces grayscale imagery should normally return a three-channel image, with the grayscale value replicated in the B, G, and R channels. Maintaining one image representation throughout the processing portion of the pipeline simplifies composition of operations.

An operation that intentionally changes channel meaning is different. For example, `swapRedBlueChannels()` exchanges the blue and red channel values. Because later OpenCV operations would still interpret a three-channel `cv::Mat` as BGR, an operation of this kind should generally be placed after processing operations that depend on the canonical BGR interpretation.

OpenCV's property is named `CAP_PROP_CONVERT_RGB`, but normal decoded OpenCV color imagery uses BGR channel order. `ImageCapture` requests this conversion internally and verifies the returned frame type; it is not exposed as a TOML option.

## Project organization

```text
imgs361-photo-booth/
|-- CMakeLists.txt
|-- config.toml
|-- LICENSE
|-- README.md
|-- apps/
|   |-- capture_single_image.cpp
|   |-- live_preview.cpp
|   `-- photo_booth.cpp
|-- include/
|   `-- photo_booth/
|       |-- AppConfig.hpp
|       |-- FileSelection.hpp
|       |-- ImageCapture.hpp
|       |-- ImageProcessing.hpp
|       |-- PlotDisplay.hpp
|       `-- SpectrumDisplay.hpp
|-- media/
|   |-- stills/
|   `-- videos/
|-- src/
|   |-- AppConfig.cpp
|   |-- FileSelection.cpp
|   |-- ImageCapture.cpp
|   |-- ImageProcessing.cpp
|   |-- PlotDisplay.cpp
|   `-- SpectrumDisplay.cpp
`-- tools/
    |-- start_virtual_camera.sh
    `-- stop_virtual_camera.sh
```

`ImageCapture` wraps OpenCV's `cv::VideoCapture`. `AppConfig` defines and loads the typed application configuration. `FileSelection` provides image-file selection using a graphical dialog when available and a terminal fallback for headless sessions. `ImageProcessing` contains reusable image-processing and analysis functions. `PlotDisplay` provides a generic interface for displaying one-dimensional sampled data using Gnuplot. `SpectrumDisplay` displays complex two-dimensional Fourier-domain data using OpenCV.

`PlotDisplay` is intentionally generic so that it can later be reused for histograms, probability density functions, cumulative distribution functions, image profiles, and other sampled data. `SpectrumDisplay` provides corresponding display support for Fourier magnitude and phase visualizations.

## Student-facing files

Most image-processing work during the semester should be concentrated in a small part of the project:

```text
apps/photo_booth.cpp
include/photo_booth/ImageProcessing.hpp
src/ImageProcessing.cpp
config.toml
```

When a new processing operation needs a persistent configuration value, students will also modify:

```text
include/photo_booth/AppConfig.hpp
src/AppConfig.cpp
```

`ImageCapture`, `FileSelection`, `PlotDisplay`, `SpectrumDisplay`, and most of the top-level CMake configuration can be treated as supplied infrastructure unless a project extension specifically requires changes there. The starter `ImageProcessing` implementation also supplies `calcHist()` as an example analysis function. Students may use `PlotDisplay` when an operation produces one-dimensional sampled data that should be visualized and `SpectrumDisplay` when displaying complex Fourier-domain image data, but ordinary image-processing operations should not require changes to either display component.

Ordinary processing operations added to the existing `ImageProcessing.cpp` do **not** require a CMake change.

## Requirements

- CMake 3.30 or later
- a C++20 compiler
- OpenCV 4.x or 5.x with `core`, `videoio`, `highgui`, `imgcodecs`, and `imgproc`
- Boost with the `program_options` component
- Eigen3
- Gnuplot with Qt terminal support
- toml++ 3.4.0, downloaded automatically by CMake using `FetchContent`
- portable-file-dialogs, downloaded automatically by CMake using `FetchContent`

Boost.Program_options and Eigen are intentionally pre-provisioned for student applications even though the starter applications do not yet use them directly. This allows students to use either library later in the semester without changing the project build environment.

Gnuplot is used by `PlotDisplay` for one-dimensional scientific plots such as histograms, probability density functions, cumulative distribution functions, and image profiles. CMake verifies that the `gnuplot` executable is available when the project is configured.

portable-file-dialogs is used by `FileSelection` to select image files when a graphical desktop is available. In a headless session, such as an SSH login without a graphical display, `FileSelection` instead prompts for a filename in the terminal.

## Build

From the project root:

```sh
cmake -S . -B build
cmake --build build
```

Compiled executables are placed in `build/bin`:

```text
build/bin/
|-- capture_single_image
|-- live_preview
`-- photo_booth
```

The project requests strict ISO C++20 (`CXX_EXTENSIONS OFF`) and enables common compiler warnings for both the core library and all student-facing applications.

For single-configuration generators, the project defaults to a `RelWithDebInfo` build when `CMAKE_BUILD_TYPE` is not specified. This provides compiler optimization while retaining debugging information, which is useful for a real-time image-processing application that students will also need to debug.

To configure an unoptimized debugging build explicitly:

```sh
rm -fr build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

To configure a fully optimized release build explicitly:

```sh
rm -fr build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Configuration

All applications accept one optional positional argument: the TOML configuration filename. When omitted, `config.toml` in the current working directory is used.

The supplied configuration is:

```toml
[camera]
device = 0
width = 1280
height = 720
fps = 30.0
fourcc = ""

[preview]
mirror = false
rotation = 0
window_name = "Photo Booth"

[capture]
save_directory = "captures"
warmup_frames = 10

[processing]
channel_swap_enabled = true
```

Missing values use defaults declared in the typed C++ configuration structures. Unknown sections, unknown options, incorrect value types, and invalid values are reported as errors.

## Run the minimal live preview

```sh
./build/bin/live_preview
```

or:

```sh
./build/bin/live_preview webcam.toml
```

Press `Esc` or `q` to quit.

## Run the photo booth

```sh
./build/bin/photo_booth
```

or:

```sh
./build/bin/photo_booth webcam.toml
```

Controls:

- `n` - toggle image negative/inversion on/off
- `h` - toggle the live histogram display on/off
- `p` - toggle the performance overlay on/off
- `Space` - save the current processed image
- `Esc` - reset all runtime processing, analysis, and display options to their startup state
- `?` - display the keyboard controls
- `q` - quit

The controls are printed when the application starts and can be displayed again with `?`. The `Esc` reset affects runtime state only; baseline processing controlled by `config.toml`, such as `processing.channel_swap_enabled`, remains unchanged.

The histogram display is calculated from the completed processed image using the supplied `calcHist()` function and displayed with `PlotDisplay`. `calcHist()` returns a `3 x 256` `CV_32S` matrix whose rows contain the blue, green, and red channel histograms. To avoid allowing the external Gnuplot window to limit the camera loop unnecessarily, the histogram is refreshed less frequently than the image preview.

The performance overlay displays the effective application frame rate in the preview window. The reported frame rate reflects the complete acquisition, processing, analysis, and display loop, so computationally expensive operations may reduce the displayed FPS. The overlay is added only to the preview image and does not modify the processed image.

Captured images are written to the directory specified by `capture.save_directory` using UTC timestamp-based filenames that include milliseconds.

Future operations should be added to `processFrame()` according to the image representation they expect and the desired processing order, rather than according to whether they are configured at startup or controlled interactively.

## Adding an Operation

The starter project intentionally uses a simple, explicit extension pattern. Processing algorithms are ordinary free functions, while `processFrame()` determines which image transformations run and in what order. Avoid introducing a class hierarchy or general-purpose processing framework until the growing application provides a clear reason to do so.

### Choosing pipeline order

Do not place operations according to whether they are baseline or optional. Those labels describe how an operation is controlled, not when it should run.

Instead, choose pipeline order according to what each operation expects and produces:

1. Operations that establish or normalize the canonical image representation belong near the beginning.
2. Image-processing operations that expect `CV_8UC3` BGR input belong in the processing portion of the pipeline, in the order needed for the desired result.
3. Operations that intentionally change channel meaning or otherwise change the output representation belong after processing operations that depend on the canonical representation.
4. Analysis and visualization operations that inspect the completed image without modifying it generally belong after `processFrame()`.

For example, a grayscale conversion normally expects a BGR image because the blue, green, and red channels contribute differently to luminance. A red/blue channel swap should therefore occur after such an operation. Putting the swap first would cause a later BGR-to-grayscale conversion to interpret the swapped channels as though they were still in canonical BGR order, producing a different result.

This ordering principle keeps individual processing functions independent. A grayscale-conversion function, for example, should not need to know whether some unrelated channel-remapping operation is enabled elsewhere in the application.

### Adding an optional runtime operation

For an operation that the user turns on and off while the application is running:

1. Declare the processing function in `include/photo_booth/ImageProcessing.hpp`.
2. Implement the function in `src/ImageProcessing.cpp`.
3. Add the operation's runtime state to `ProcessingState` in `apps/photo_booth.cpp`.
4. Add a keyboard control in `handleKey()` to toggle or adjust that state.
5. Add the operation to `processFrame()` at the point appropriate for its image-representation requirements and desired processing order.

For example, a future threshold operation might add:

```cpp
bool threshold_enabled{false};
```

to `ProcessingState`, toggle it from `handleKey()`, and apply the threshold operation from `processFrame()` when the state is enabled. Independent optional operations should normally have independent state so that multiple operations can be active at the same time.

When several algorithms are alternatives for the **same** processing stage, however, model that choice explicitly rather than using unrelated enable flags that could accidentally permit incompatible alternatives to run together. A simple enum is often appropriate for representing mutually exclusive choices within a stage.

It is also useful to distinguish **runtime state** from **persistent operation data**. Runtime state answers questions such as whether an operation is currently active or which method is selected. Persistent operation data is information the operation may reuse across frames or across enable/disable cycles, such as a selected reference image or precomputed data derived from it. Keeping those concepts separate prevents disabling an operation from unnecessarily discarding data that may be reused later.

### Adding a baseline configurable operation

For an operation whose initial behavior is established by `config.toml` and applied automatically to every frame when enabled:

1. Declare the processing function in `include/photo_booth/ImageProcessing.hpp`.
2. Implement the function in `src/ImageProcessing.cpp`.
3. Add the operation's enable flag and any parameters to the flat `ProcessingConfig` structure in `include/photo_booth/AppConfig.hpp`.
4. Add matching values to the `[processing]` section of `config.toml`.
5. Add those key names to the processing validation list in `src/AppConfig.cpp`, read the values with `readOptional()`, and add any necessary range/value checks to `validateValues()`.
6. Add the operation to `processFrame()` at the point appropriate for its image-representation requirements and desired processing order.

For example, a future configurable fixed-threshold operation could use:

```toml
[processing]
channel_swap_enabled = true
threshold_enabled = false
threshold_value = 128
```

with a correspondingly simple configuration structure:

```cpp
struct ProcessingConfig {
  bool channel_swap_enabled{false};
  bool threshold_enabled{false};
  int threshold_value{128};
};
```

The flat `[processing]` section is intentional. It keeps the mechanics of adding a configuration value visible and repetitive, so that most student effort remains focused on the image-processing algorithm itself. If the configuration becomes unwieldy later in the semester, that is an appropriate opportunity to consider refactoring.

### Adding an analysis or display operation

Not every operation belongs in `processFrame()`. Operations that measure or visualize an image without modifying it should generally operate on the completed `processed_frame` after the image-processing pipeline.

This keeps three responsibilities distinct:

- **Image transformation** changes the image and belongs in `processFrame()`.
- **Image analysis** computes information from the completed image and normally belongs in `ImageProcessing`.
- **Visualization** presents the analysis result and belongs in an appropriate supplied display component such as `PlotDisplay` or `SpectrumDisplay`.

The supplied histogram display demonstrates this pattern. `calcHist()` analyzes the completed `processed_frame` and returns a `3 x 256` matrix containing the B, G, and R channel histograms. `showPlot()` displays that result, while `hidePlot()` closes the histogram window when the display is disabled or the runtime pipeline is reset.

More generally, one-dimensional analysis functions can return a `cv::Mat` whose rows represent separate curves and whose columns represent samples. Those results can also be displayed with `showPlot()`. Examples of data suited to this pattern include PDFs, CDFs, image profiles, and other sampled measurements.

For two-dimensional complex Fourier-domain data, an analysis function can return a `CV_32FC2` complex spectrum. `showSpectrum()` can then display either its magnitude or phase, and `hideSpectrum()` closes the spectrum window. The display component handles visualization; it does not calculate the Fourier transform itself.

Analysis displays do not necessarily need to update at the camera frame rate. The supplied histogram display deliberately refreshes only once every several camera frames because updating the external Gnuplot window every frame can reduce application responsiveness. This demonstrates how analysis-display refresh can be decoupled from image acquisition and processing while still providing useful continuously updated information.

#### Analysis dependencies

Some analyses impose requirements on the image representation they receive. If an analysis requires grayscale input, for example, the application should prevent an invalid operation combination when possible. The reusable analysis function should nevertheless validate its own input requirements independently.

This demonstrates a useful general principle: the application should guide the user away from invalid states, while reusable processing and analysis functions should still enforce their own contracts.

## Single-image camera utility

```sh
./build/bin/capture_single_image
```

or:

```sh
./build/bin/capture_single_image laboratory_camera.toml
```

The output directory and camera warmup count are controlled by the `[capture]` section. The output image uses a UTC timestamp-based filename that includes milliseconds.

All three applications support `--help`.

## Virtual camera for Linux server development

The `tools/` directory contains a virtual-camera setup for Linux systems that do not have access to a physical camera. This is useful when developing on a shared server or other remote Linux system.

`start_virtual_camera.sh` uses FFmpeg to continuously feed either a video file or a still image into a `v4l2loopback` virtual camera. Video files are looped continuously, while still images are repeated to produce a continuous camera stream.

By default, the script uses:

- media file `media/videos/female_model_2_720p.mp4`
- virtual camera device `/dev/video10`
- output frame rate of 30 fps

The media file and virtual camera device can be overridden independently from the command line:

```sh
./tools/start_virtual_camera.sh
./tools/start_virtual_camera.sh -d /dev/video12
./tools/start_virtual_camera.sh -f sample.mp4
./tools/start_virtual_camera.sh -f test_image.jpg
./tools/start_virtual_camera.sh -d /dev/video12 -f sample.mp4
```

Use `-h` to display the available command-line options:

```sh
./tools/start_virtual_camera.sh -h
```

The output image is constrained to fit within a 1280 x 720 bounding box while preserving the source aspect ratio. Media smaller than this limit is not upscaled. For example, a 1600 x 1200 source produces a 960 x 720 virtual-camera image, while a 640 x 480 source remains 640 x 480.

The script reports the selected camera device, corresponding OpenCV camera index, media type, input resolution, actual virtual-camera resolution, frame rate, and FFmpeg process ID when it starts.

The script requires:

- FFmpeg and `ffprobe`
- a `v4l2loopback` virtual camera device already created at the selected `/dev/videoN` device

For the default `/dev/video10` device, select camera index `10` in the configuration file:

```toml
[camera]
device = 10
```

If a different virtual camera is selected with `-d`, use the corresponding numeric camera index. For example, `/dev/video12` corresponds to:

```toml
[camera]
device = 12
```

Stop the virtual camera with:

```sh
./tools/stop_virtual_camera.sh
```

The virtual-camera scripts are development utilities only; they are not required when a normal built-in or USB camera is available.

## ImageCapture component

`ImageCapture` contains no photo-booth user-interface or image-processing behavior. It acquires camera frames and exposes the most recent frame as a `cv::Mat`.

```cpp
#include "photo_booth/ImageCapture.hpp"

#include <iostream>

int main() {
  photo_booth::ImageCapture camera;

  if (!camera.open()) {
    std::cerr << camera.errorMessage() << '\n';
    return 1;
  }

  if (!camera.read()) {
    std::cerr << camera.errorMessage() << '\n';
    return 1;
  }

  const cv::Mat& image = camera.image();

  // image is CV_8UC3 using BGR channel order.
}
```

Camera properties such as image dimensions, frame rate, and FOURCC are requests rather than guarantees. The hardware, operating system, driver, and OpenCV backend may ignore a requested property or select a nearby supported mode. Use `cameraInfo()` after opening the device to inspect the values reported by the backend.

## Suggested evolution during the course

The contrast between `live_preview` and `photo_booth` provides a simple starting lesson in application structure. `live_preview` demonstrates direct use of an acquired frame. `photo_booth` demonstrates how a growing application benefits from an explicit processing pipeline and from separating image transformation, analysis, and visualization.

As capabilities accumulate, students should continue adding simple processing functions and explicit configuration/runtime state. `processFrame()` and `handleKey()` are expected to become somewhat more crowded as the project grows; that pressure can provide a concrete reason to refactor toward additional classes or modules when those abstractions become useful.

Potential additions include quantization, dynamic contrast enhancement, histogram operations, probability density and cumulative distribution displays, spatial and frequency-domain filtering, sharpening, geometric transformations, perspective correction, artistic filters, feature detection, segmentation, and capture/review behavior.

## License

This project is licensed under the GNU General Public License v3.0. See `LICENSE` for details.

## Contact

### Author

Carl Salvaggio, Ph.D.  
Professor of Imaging Science  
Director, Digital Imaging and Remote Sensing (DIRS) Laboratory

### E-mail

carl.salvaggio@rit.edu

### Organization

Chester F. Carlson Center for Imaging Science  
Rochester Institute of Technology  
Rochester, New York, 14623  
United States
