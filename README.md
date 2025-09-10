# kyubey

A command-line instrument for quantitative analysis of grayscale images through the programmatic application of stochastic noise, spatial filtering, and gradient-based edge detection.

```
Usage:
  kyubey [OPTION...]

  -1, --image1 arg      Path to the first grayscale input image artifact.
  -2, --image2 arg      Path to the second grayscale input image artifact.
  -d, --density arg     Specifies the stochastic noise density. Accepts a value in the range [0.0, 1.0].
                        (default: 0.10)
  -o, --output-dir arg  Specifies the directory for output artifacts. (default:
                        output)
  -h, --help            Prints this usage summary.
```

## Architectural Rationale

The system's architecture is predicated on a deterministic and hermetic build environment, leveraging GN and Ninja. The entire toolchain is encapsulated within a multi-stage Docker container, ensuring a consistent and isolated context for both compilation and execution, thereby eliminating environmental variance in analytical workflows.

## Prerequisites

- A functional Docker daemon.

## Build Procedure

The build process is entirely self-contained. From the root of the source tree, the build sequence is initiated by the following command:

```bash
docker build -t kyubey .
```

This directive orchestrates a sequenced, multi-stage build, resulting in a minimal runtime image containing the kyubey executable.

## Operational Execution

Execution of the `kyubey` binary is performed via the `docker run` command. A volume must be mapped from the host system to the container's `/data` directory to facilitate file I/O for both input and output images.

For validation and baseline testing, the repository contains a data directory with sample images (image1.png, image2.png). To process these artifacts, execute the following:

```bash
docker run --rm \
  -v "$(pwd)/data:/data" \
  kyubey \
  --image1 /data/image1.png \
  --image2 /data/image2.png \
  --density 0.1 \
  --output-dir /data/output
```

The `-v "$(pwd)/data:/data"` directive establishes a bind mount between the host's `data` directory and the container's `/data` mount point. Input images within the container are addressed via their path in the container's virtual filesystem (e.g., `/data/image1.png`). The --density parameter controls the stochastic noise density, accepting a floating-point value in the range of [0.0, 1.0]; if this argument is omitted, the system defaults to a value of 0.10. The resulting processed images will be deposited in the `data/output` directory on the host filesystem.

For arbitrary image processing, place the target images within a designated local directory (e.g., `/path/to/images/`) and execute the container with the appropriate volume mapping:

```bash
docker run --rm \
  -v /path/to/images/:/data \
  kyubey \
  --image1 /data/image1.png \
  --image2 /data/image2.png \
  --output-dir /data/output
```

The program will generate filtered, noise-augmented, and Sobel edge-detected renditions of the input images within the `/path/to/images/output` directory on the host system.
