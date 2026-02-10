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

    stepper_light_controller_node = Node(
        package='inspection_eoat',
        executable='stepper_light_controller_node',
        name='stepper_light_controller_node',
        output='screen',
        parameters=[{
            'port': '/dev/ttyACM0',
            'home_on_start': True,
            'velocity_scale': 127.0,
            'publish_rate': 50.0,
        }]
    )

    return LaunchDescription(declared_arguments + [
        joy_node,
        d405_camera_node,
        stepper_light_controller_node
    ])
