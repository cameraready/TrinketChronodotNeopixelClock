This project started as an experiment to control Adafruit NeoPixel rings with the Trinket and a ChronoDot.

I went through several iterations and you can see some of the remnants of the previous version commented out.

Early on I had converted the DST calculations based on Jack Christensen's TimeZone library and it just fit in the memory of the Trinket. Then I decided that wouthout any sound it needed some animations at the top of every hour. The problem was that the additional functions for the animations put it over the memory limit. I ended up converting the DST calculations to just storing the start and stop dates in ProgMem up to the year 2037. I'm not sure the function would have worked beyond that year since it used unix time anyway. So while it has a limited time frame for use I doubt I will still be using it by then. This project was a good exercise for simplifying code. I hope to use this in my next clock build.

Currently the clock sits on my desk where the hourly animations catch my attention when they happen.