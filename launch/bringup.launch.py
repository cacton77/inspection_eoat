from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, RegisterEventHandler
from launch_ros.actions import Node
from launch import LaunchDescription

def generate_launch_description():
    declared_arguments = [
        # Add any declared arguments here if needed in the future
        # DeclareLaunchArgument("cell", default_value="alpha"),
    ]

    d405_camera_node = Node(
            package="realsense2_camera",
            executable="realsense2_camera_node",
            name="d405_camera",
            output="screen",
    )

    joy_node = Node(
        package='joy',
        executable="joy_node",
        name='joy'
    )
    
    return LaunchDescription(declared_arguments + [
        joy_node,
        d405_camera_node
    ])