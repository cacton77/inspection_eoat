import rpi_ws281x as ws281x

# LED strip configuration:
LED_COUNT = 1        # Number of LED pixels.
LED_PIN = 18          # GPIO pin connected to the pixels (18 uses PWM!).
# LED_PIN = 10        # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA = 10          # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest
LED_INVERT = False    # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53   



self.strip = ws281x.PixelStrip(self.LED_COUNT, 
                            self.LED_PIN, 
                            self.LED_FREQ_HZ, 
                            self.LED_DMA, 
                            self.LED_INVERT, 
                            self.LED_BRIGHTNESS, 
                            self.LED_CHANNEL)

self.strip.begin()

self.strip.setPixelColor(i, ws281x.Color(255, 255, 255))