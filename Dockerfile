FROM ros:humble

RUN apt-get update && apt-get install -y neovim && rm -rf /var/lib/apt/lists/*

RUN apt-get update \
    && apt-get install -y ros-humble-image-transport \
    && apt-get install python3-colcon-common-extensions

# Install curl and wget
RUN apt-get update \
    && apt-get install -y curl wget

# Install Realsense SDK and ROS Wrappers
RUN mkdir -p /etc/apt/keyrings \
    && curl -sSf https://librealsense.intel.com/Debian/librealsense.pgp | sudo tee /etc/apt/keyrings/librealsense.pgp > /dev/null \
    && echo "deb [signed-by=/etc/apt/keyrings/librealsense.pgp] https://librealsense.intel.com/Debian/apt-repo `lsb_release -cs` main" | \
    tee /etc/apt/sources.list.d/librealsense.list \
    && apt-get update \
    && apt-get install -y librealsense2-dkms \
    && apt-get install -y librealsense2-utils \
    && apt-get install -y ros-humble-realsense2*

# Remove links to old gphoto2 version
RUN rm -rf /lib/x86_64-linux-gnu/libgphoto2*

# Download and run gphoto2-updater
RUN wget https://raw.githubusercontent.com/gonzalo/gphoto2-updater/master/gphoto2-updater.sh  \
    && wget https://raw.githubusercontent.com/gonzalo/gphoto2-updater/master/.env \ 
    && chmod +x gphoto2-updater.sh \
    && ./gphoto2-updater.sh

# Create workspace with source code
RUN mkdir -p inspection_ws/src \
    && cd inspection_ws \
    && git clone https://github.com/macs-lab/gphoto2_ros.git src/gphoto2_ros
# && colcon build --symlink-install

RUN bin/bash -c "source /opt/ros/humble/setup.bash ; cd inspection_ws; colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release"

COPY entrypoint.sh /entrypoint.sh
COPY bashrc /root/.bashrc

ENTRYPOINT ["/bin/bash", "/entrypoint.sh"]

CMD ["bash"]