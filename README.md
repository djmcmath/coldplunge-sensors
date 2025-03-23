# coldplunge-sensors
A quick and dirty ESP32-based monitor for DIY cold plunge

I've built a simple cold plunge in my driveway: basically just a 150gal Rubbermaid stock tank in a plywood box filled with insulation, plus a Danner 1200 pump, 2-stage filtration, and a Penguin 1/2HP chiller -- nothing too fancy.  But I wanted to (a) see what the temperature was without going outside with a thermometer and (b) track flow rate to know when it's time to change the filter.

So the equipment:
- Adafruit MAX31855 (https://www.adafruit.com/product/269)
- ESP32 with external WiFi antenna connection (https://www.adafruit.com/product/5438) -- Something **like** this -- ESP32 with an external antenna, because you probably have mediocre signal at your cold plunge, and the PCB antenna just won't reach
- Flow meter (https://www.amazon.com/dp/B07MY6LFPH?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1&th=1) -- Again, virtually any flow meter that will work in your system will behave about the same, so it doesn't need to be this one, just something with a typical 3-pin connection
- K-Type thermocouple (https://www.amazon.com/Temperature-Sensors-Thermocouple-Detachable-Connector/dp/B09H4X98L9/ref=asc_df_B09H4X98L9) -- I just used one I had on the workbench, looked similar to this

It's not beautiful, but it works fine for my purposes.  Feel free to copy, adapt, and share any improvements (there are plenty of opportunities for improvement here).

Enjoy!
