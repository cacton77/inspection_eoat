ARG ROS_DISTRO=humble

##########################################
# Base Image for Inspection Tool Control #
##########################################
FROM ros:${ROS_DISTRO}-perception as base
ENV ROS_DISTRO=${ROS_DISTRO}
SHELL ["/bin/bash", "-c"]

# Install cli tools via apt
RUN apt-get update \
    && apt-get install -y wget curl neovim python3-pip\
    && rm -rf /var/lib/apt/lists/*

# Install rpi-ws281x-python package
RUN pip install rpi_ws281x pydantic

# Remove links to old gphoto2 version
RUN rm -rf /lib/*-linux-gnu/libgphoto2*

# Download and run gphoto2-updater
RUN wget https://raw.githubusercontent.com/gonzalo/gphoto2-updater/master/gphoto2-updater.sh  \
    && wget https://raw.githubusercontent.com/gonzalo/gphoto2-updater/master/.env \ 
    && chmod +x gphoto2-updater.sh \
    && ./gphoto2-updater.sh

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y \
    build-essential \
    cmake \
    git-all \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# Use Cyclone DDS as middleware
RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-${ROS_DISTRO}-rmw-cyclonedds-cpp
ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

ARG DEBIAN_FRONTEND=dialog

# Create base Colcon workspace with external dependencies
RUN mkdir -p /base_ws/src

# Build the base Colcon workspace, installing dependencies first.
WORKDIR /base_ws
COPY ./gphoto2_srvs/ ./src/gphoto2_srvs/
RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
    && apt-get update -y \
    && rosdep install --from-paths src --ignore-src --rosdistro ${ROS_DISTRO} -y \
    && colcon build --symlink-install

#############################################
# Overlay Image for Inspection Tool Control #
#############################################
FROM base AS overlay

# Create an overlay Colcon workspace
RUN mkdir -p /overlay_ws/src
WORKDIR /overlay_ws/src
COPY dependencies.repos .
RUN vcs import < dependencies.repos

WORKDIR /overlay_ws
RUN source /base_ws/install/setup.bash \
    && rosdep install --from-paths src --ignore-src --rosdistro ${ROS_DISTRO} -y \
    && colcon build --symlink-install

# Set up the entrypoint
COPY ./docker/entrypoint.sh /
COPY ./bashrc /root/.bashrc
ENTRYPOINT [ "/entrypoint.sh" ]

CMD ["bash"]