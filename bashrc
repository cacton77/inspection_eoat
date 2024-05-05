source /opt/ros/${ROS_DISTRO}/setup.bash
source /usr/share/colcon_argcomplete/hook/colcon-argcomplete.bash

# Source the base workspace, if built
if [ -f /tool_ws/install/setup.bash ]
then
    source /tool_ws/install/setup.bash
fi

# Source the overlay workspace, if built
if [ -f /overlay_ws/install/setup.bash ]
then
    source /overlay_ws/install/setup.bash
fi