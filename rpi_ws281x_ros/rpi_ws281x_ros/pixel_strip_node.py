import rclpy
from rclpy.node import Node

from std_msgs.msg import ColorRGBA

from rpi_ws281x import PixelStrip, Color
import rpi_ws281x as rpi
import argparse

# LED strip configuration:
LED_COUNT = 96        # Number of LED pixels.
LED_PIN = 18          # GPIO pin connected to the pixels (18 uses PWM!).
# LED_PIN = 10        # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA = 10          # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest
LED_INVERT = False    # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53

class PixelStrip(Node):
    def __init__(self):
        super().__init__('pixel_strip')
        self.sub = self.create_subscription(ColorRGBA, 'pixel_strip', self.set_pixels_callback, 10)

        self.strip = rpi.PixelStrip(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS, LED_CHANNEL)
        # Intialize the library (must be called once before other functions).
        self.strip.begin()

    def set_pixels_callback(self, msg):
        self.get_logger().info('Incoming message\nr: %d g: %d b: %d' % (msg.r, msg.g, msg.b))

        r, g, b = int(msg.r), int(msg.g), int(msg.b)

        for i in range(LED_COUNT):
            self.strip.setPixelColor(i, rpi.Color(r, g, b))

        self.strip.show()



def main():
    rclpy.init()
    
    pixel_strip_node = PixelStrip()

    rclpy.spin(pixel_strip_node)

    rclpy.shutdown()

if __name__ == '__main__':
    main()
