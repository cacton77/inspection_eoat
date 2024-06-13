import sys
import signal
import rclpy
from rclpy.node import Node

from std_msgs.msg import ColorRGBA

import rpi_ws281x as ws281x


class PixelStripNode(Node):

    def __init__(self):
        super().__init__('pixel_strip')
        self.sub = self.create_subscription(
            ColorRGBA, 'pixel_strip', self.set_pixels_callback, 10)
        self.declare_parameters(
            namespace='',
            parameters=[
                ('led_count', 1),
                ('led_pin', 18),
                ('led_freq_hz', 800000),
                ('led_dma', 10),
                ('led_brightness', 255),
                ('led_invert', False),
                ('led_channel', 0)
            ]
        )

        self.init_strip()

        # timer_period = 0.1
        # self.timer = self.create_timer(timer_period, self.timer_callback)

    def init_strip(self):

        led_count = self.get_parameter('led_count').value
        led_pin = self.get_parameter('led_pin').value
        led_freq_hz = self.get_parameter('led_freq_hz').value
        led_dma = self.get_parameter('led_dma').value
        led_invert = self.get_parameter('led_invert').value
        led_brightness = self.get_parameter('led_brightness').value
        led_channel = self.get_parameter('led_channel').value

        self.strip = ws281x.PixelStrip(led_count,
                                       led_pin,
                                       led_freq_hz,
                                       led_dma,
                                       led_invert,
                                       led_brightness,
                                       led_channel)

        self.strip.begin()

    def set_pixels_callback(self, msg):
        self.get_logger().info('Incoming message\nr: %d g: %d b: %d' % (msg.r, msg.g, msg.b))

        r, g, b = int(msg.r), int(msg.g), int(msg.b)

        for i in range(self.strip.numPixels()):
            self.strip.setPixelColor(i, ws281x.Color(r, g, b))

        self.strip.show()

    def on_shutdown(self):
        self.get_logger().info('Node is shutting down, turning off leds...')
        for i in range(self.strip.numPixels()):
            self.strip.setPixelColor(i, ws281x.Color(0, 0, 0))

        self.strip.show()


def main():
    rclpy.init()

    pixel_strip_node = PixelStripNode()

    def shutdown_hook(signum, frame):
        pixel_strip_node.on_shutdown()
        rclpy.shutdown()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown_hook)
    signal.signal(signal.SIGTERM, shutdown_hook)

    try:
        rclpy.spin(pixel_strip_node)
    except KeyboardInterrupt:
        pass
    finally:
        pixel_strip_node.on_shutdown()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
