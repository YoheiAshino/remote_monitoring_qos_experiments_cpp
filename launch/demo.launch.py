from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():

    return LaunchDescription([

        Node(
            package="remote_monitoring_qos_experiments_cpp",
            executable="telemetry_publisher",
            output="screen"
        ),

        Node(
            package="remote_monitoring_qos_experiments_cpp",
            executable="remote_monitor",
            parameters=["config/qos.yaml"],
            output="screen"
        )
    ])