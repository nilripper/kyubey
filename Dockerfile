#
# Stage 1: Builder
#
# This stage compiles the application. It installs all necessary build
# dependencies, including the specific GN build tool, and then compiles the
# source code into a single executable.
#
FROM ubuntu:22.04 AS builder

# ARG TARGETARCH is automatically set by Docker to the architecture of the
# build machine (e.g., 'amd64', 'arm64').
ARG TARGETARCH

# Set a non-interactive frontend for Debian's package manager to prevent
# prompts from blocking the build process.
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies.
# - build-essential: Provides a C++ compiler.
# - git: For version control operations if needed during the build.
# - python3: Required by the GN build system.
# - unzip, wget: Utilities for downloading and extracting dependencies.
# - ninja-build: The build system used to compile the code.
# - libopencv-dev: Development libraries for OpenCV.
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    python3 \
    unzip \
    wget \
    ninja-build \
    libopencv-dev

# Download and install GN, a meta-build system. The version is selected
# dynamically based on the target architecture.
RUN wget "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-${TARGETARCH}/+/latest" -O gn.zip && \
    unzip gn.zip -d /usr/local/bin && \
    rm gn.zip

# Set the working directory for the application source code.
WORKDIR /app

# Copy the entire build context (source code, build scripts, etc.) into the
# builder stage.
COPY . .

# Generate Ninja build files from the GN configuration. The output is placed
# in the 'out/Default' directory.
RUN gn gen out/Default

# Compile the application using Ninja.
RUN ninja -C out/Default

#
# Stage 2: Final Image
#
# This stage creates the final, minimal production image. It copies the
# compiled application from the builder stage and includes only the necessary
# runtime dependencies, resulting in a smaller and more secure image.
#
FROM ubuntu:22.04

# Set a non-interactive frontend for Debian's package manager.
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies and clean up the apt cache to minimize image size.
# - libopencv-dev: Runtime libraries for OpenCV.
# - libstdc++6: Standard C++ library.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopencv-dev \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory for the application.
WORKDIR /app

# Copy the compiled executable from the builder stage into the final image.
COPY --from=builder /app/out/Default/obj/src/kyubey .

# Set the container's entrypoint to run the application executable.
ENTRYPOINT ["./kyubey"]

# Set the default command to display the application's help message. This is
# executed when the container is run without any additional arguments.
CMD ["--help"]
