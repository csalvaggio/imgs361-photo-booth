#!/usr/bin/env bash

# ---------------------------------------------------------------------------
# IMGS.361 Photo Booth - Virtual Camera
#
# Feeds a video or still-image file into a v4l2loopback virtual camera.
#
# Video files are looped continuously. Still images are repeated continuously
# to produce a video stream.
#
# The output is constrained to fit within 1280 x 720 while preserving the
# original aspect ratio. Media smaller than this limit is not upscaled.
#
# Usage:
#   start_virtual_camera.sh [-d video_device] [-f media_file]
#
# Options:
#   -d  Virtual camera device
#       Default: /dev/video10
#
#   -f  Video or still-image file to stream
#       Default: media/videos/female_model_2_720p.mp4 in the project directory
#
#   -h  Display this help message
#
# Examples:
#   start_virtual_camera.sh
#   start_virtual_camera.sh -h
#   start_virtual_camera.sh -d /dev/video12
#   start_virtual_camera.sh -f sample.mp4
#   start_virtual_camera.sh -f test_image.jpg
#   start_virtual_camera.sh -d /dev/video12 -f sample.mp4
# ---------------------------------------------------------------------------

set -e

# Directory containing this script.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---------------------------------------------------------------------------
# Default configuration.
# ---------------------------------------------------------------------------

PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEFAULT_MEDIA_FILE="${PROJECT_DIR}/media/videos/female_model_2_720p.mp4"
DEFAULT_VIDEO_DEVICE="/dev/video10"

MEDIA_FILE="${DEFAULT_MEDIA_FILE}"
VIDEO_DEVICE="${DEFAULT_VIDEO_DEVICE}"

MAX_WIDTH=1280
MAX_HEIGHT=720
FRAME_RATE=30
RUN_DURATION="02:00:00"

PID_FILE="${SCRIPT_DIR}/.virtual_camera.pid"
LOG_FILE="${SCRIPT_DIR}/.virtual_camera.log"

# ---------------------------------------------------------------------------
# Display usage information.
# ---------------------------------------------------------------------------

usage() {
    echo "Usage: $0 [-d video_device] [-f media_file]"
    echo
    echo "Options:"
    echo "  -d  Virtual camera device"
    echo "      Default: ${DEFAULT_VIDEO_DEVICE}"
    echo
    echo "  -f  Video or still-image file to stream"
    echo "      Default: ${DEFAULT_MEDIA_FILE}"
    echo
    echo "  -h  Display this help message"
    echo
    echo "Output:"
    echo "  Maximum resolution: ${MAX_WIDTH} x ${MAX_HEIGHT}"
    echo "  Frame rate:         ${FRAME_RATE} fps"
    echo
    echo -n "The original aspect ratio is preserved and smaller media "
    echo "is not upscaled."
}

# ---------------------------------------------------------------------------
# Process command-line options.
# ---------------------------------------------------------------------------

while getopts "d:f:h" option; do
    case "${option}" in
        d)
            VIDEO_DEVICE="${OPTARG}"
            ;;
        f)
            MEDIA_FILE="${OPTARG}"
            ;;
        h)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

shift $((OPTIND - 1))

# Reject unexpected positional arguments.
if [[ $# -ne 0 ]]; then
    echo "ERROR: Unexpected command-line argument: $1"
    echo
    usage
    exit 1
fi

# ---------------------------------------------------------------------------
# Check prerequisites.
# ---------------------------------------------------------------------------

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ERROR: ffmpeg is not installed."
    exit 1
fi

if ! command -v ffprobe >/dev/null 2>&1; then
    echo "ERROR: ffprobe is not installed."
    exit 1
fi

if [[ ! -e "${VIDEO_DEVICE}" ]]; then
    echo "ERROR: Virtual camera ${VIDEO_DEVICE} does not exist."
    echo "       The v4l2loopback device must be created first."
    exit 1
fi

if [[ ! -f "${MEDIA_FILE}" ]]; then
    echo "ERROR: Media file not found:"
    echo "       ${MEDIA_FILE}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Determine media information.
# ---------------------------------------------------------------------------

MEDIA_FORMAT=$(
    ffprobe \
        -v error \
        -show_entries format=format_name \
        -of default=noprint_wrappers=1:nokey=1 \
        "${MEDIA_FILE}"
)

DIMENSIONS=$(
    ffprobe \
        -v error \
        -select_streams v:0 \
        -show_entries stream=width,height \
        -of csv=s=x:p=0 \
        "${MEDIA_FILE}"
)

if [[ -z "${DIMENSIONS}" || "${DIMENSIONS}" != *x* ]]; then
    echo "ERROR: Unable to determine image dimensions:"
    echo "       ${MEDIA_FILE}"
    exit 1
fi

INPUT_WIDTH="${DIMENSIONS%x*}"
INPUT_HEIGHT="${DIMENSIONS#*x}"

# Make sure the dimensions are numeric.
if [[ ! "${INPUT_WIDTH}" =~ ^[0-9]+$ ||
      ! "${INPUT_HEIGHT}" =~ ^[0-9]+$ ]]; then
    echo "ERROR: Invalid media dimensions:"
    echo "       ${DIMENSIONS}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Determine whether the input is a still image or video.
#
# ffprobe commonly reports still-image formats as image2 or *_pipe.
# ---------------------------------------------------------------------------

case "${MEDIA_FORMAT}" in
    image2|*_pipe)
        MEDIA_TYPE="Still image"
        INPUT_OPTIONS=(-loop 1 -framerate "${FRAME_RATE}")
        ;;
    *)
        MEDIA_TYPE="Video"
        INPUT_OPTIONS=(-stream_loop -1)
        ;;
esac

# ---------------------------------------------------------------------------
# Determine the output resolution.
#
# Scale the media to fit within MAX_WIDTH x MAX_HEIGHT while:
#
#   - preserving the original aspect ratio,
#   - never upscaling,
#   - keeping both dimensions even for yuvj420p.
# ---------------------------------------------------------------------------

OUTPUT_WIDTH="${INPUT_WIDTH}"
OUTPUT_HEIGHT="${INPUT_HEIGHT}"

if (( INPUT_WIDTH > MAX_WIDTH || INPUT_HEIGHT > MAX_HEIGHT )); then

    # Determine whether width or height is the limiting dimension.
    if (( INPUT_WIDTH * MAX_HEIGHT > INPUT_HEIGHT * MAX_WIDTH )); then

        # Width is the limiting dimension.
        OUTPUT_WIDTH="${MAX_WIDTH}"
        OUTPUT_HEIGHT=$((INPUT_HEIGHT * MAX_WIDTH / INPUT_WIDTH))

    else

        # Height is the limiting dimension.
        OUTPUT_HEIGHT="${MAX_HEIGHT}"
        OUTPUT_WIDTH=$((INPUT_WIDTH * MAX_HEIGHT / INPUT_HEIGHT))

    fi
fi

# yuvj420p requires even dimensions. Reduce an odd dimension by one pixel.
if (( OUTPUT_WIDTH % 2 != 0 )); then
    OUTPUT_WIDTH=$((OUTPUT_WIDTH - 1))
fi

if (( OUTPUT_HEIGHT % 2 != 0 )); then
    OUTPUT_HEIGHT=$((OUTPUT_HEIGHT - 1))
fi

# ---------------------------------------------------------------------------
# Determine the camera index from /dev/videoN.
# ---------------------------------------------------------------------------

if [[ "${VIDEO_DEVICE}" =~ /video([0-9]+)$ ]]; then
    CAMERA_INDEX="${BASH_REMATCH[1]}"
else
    CAMERA_INDEX="unknown"
fi

# ---------------------------------------------------------------------------
# Check whether the virtual camera is already running.
# ---------------------------------------------------------------------------

if [[ -f "${PID_FILE}" ]]; then
    PID=$(cat "${PID_FILE}")

    if kill -0 "${PID}" 2>/dev/null; then
        echo "Virtual camera is already running (PID ${PID})."
        exit 0
    else
        # Stale PID file.
        rm -f "${PID_FILE}"
    fi
fi

# ---------------------------------------------------------------------------
# Start FFmpeg.
# ---------------------------------------------------------------------------

echo "Starting IMGS.361 virtual camera..."
echo
echo "Media:   ${MEDIA_FILE}"
echo "Type:    ${MEDIA_TYPE}"
echo "Device:  ${VIDEO_DEVICE}"
echo

ffmpeg \
    -loglevel error \
    -re \
    "${INPUT_OPTIONS[@]}" \
    -i "${MEDIA_FILE}" \
    -an \
    -vf "scale=${OUTPUT_WIDTH}:${OUTPUT_HEIGHT}" \
    -r "${FRAME_RATE}" \
    -t "${RUN_DURATION}" \
    -c:v mjpeg \
    -pix_fmt yuvj420p \
    -q:v 2 \
    -f v4l2 \
    "${VIDEO_DEVICE}" \
    >"${LOG_FILE}" 2>&1 &

PID=$!

echo "${PID}" > "${PID_FILE}"

# Remove the PID file when FFmpeg terminates.
(
    while kill -0 "${PID}" 2>/dev/null; do
        sleep 1
    done

    rm -f "${PID_FILE}"
) &

# Give FFmpeg a moment to start.
sleep 1

# Make sure it is still running.
if ! kill -0 "${PID}" 2>/dev/null; then
    echo "ERROR: Virtual camera failed to start."
    echo
    echo "See:"
    echo "    ${LOG_FILE}"
    rm -f "${PID_FILE}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Report virtual-camera information.
# ---------------------------------------------------------------------------

echo "Virtual camera started."
echo
echo "Camera device:     ${VIDEO_DEVICE}"
echo "Camera index:      ${CAMERA_INDEX}"
echo "Media type:        ${MEDIA_TYPE}"
echo "Input resolution:  ${INPUT_WIDTH} x ${INPUT_HEIGHT}"
echo "Camera resolution: ${OUTPUT_WIDTH} x ${OUTPUT_HEIGHT}"
echo "Frame rate:        ${FRAME_RATE} fps"
echo "Maximum run time:  ${RUN_DURATION}"
echo "Process ID:        ${PID}"
