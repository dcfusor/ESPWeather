# ESPWeather
Simple ESP8266 indoor weather station

Just using this to learn github itself...the code here could be called gpl, but really as far as I'm concerned, it's
just copyleft - share and enjoy, nothing special here in my first go.  Of course, the code examples I'm making use of
to do this have their own licences which are contained in them.

This ESP8266 sketch uses an Adafruit Huzzah board and a SHT 31 temperature and humidity sensor that talks on I2C, using the
default addresses and GPIO pins 4 and 5.  The eventual goal might be to make it work on a cheaper Chinese clone using
GPIO 0 and 1, but I've not tried that yet.  Provision to set the I2C pins IS in the code, but generates a warning when
commented in, and isn't tested at this point.

To use this, you'll need the arduino IDE, just get the latest version for your platform and set it up, adding the 
board support line in file->preferences for the ESP modules: 
https://adafruit.github.io/arduino-board-index/package_adafruit_index.json,http://arduino.esp8266.com/stable/package_esp8266com_index.json

In general I mark my code with @@@ wherever there was something I thought was dodgy - search for that in case of problems,
that's likely where they will be found.  In this example, I hardcoded the wifi access point credentials for example, which 
is "bad".  Not being sure how to get around that easily at the ESP end of things, another part of the overall LAN of things
project is to create a private raspberry pi based WAP that has those credentials, and either do or don't route those
to your actual main LAN.  For now, this isn't useful if you can't see it on your lan, and later versions will talk only to
the pi-3 which will then emit the website, freeing up a lot of resources on the ESP chip and adding some security (as in, the
pi won't route the private wifi access it generates to your lan anymore).  I've had some crashes, maybe power glitches with
the ESP web server, yet UDP kept working all along, so I'm going to use that as the communication between these "leaves"
and the pi "root" in the future.

This code reads the sensor once every 2 minutes and keeps the results in  circular buffers, one for each of temperature
and humidity.  When the web page is hit, these buffer are rendered into SVG plots of the last 24 hours of readings.
The sensor is actually read more often than that, and low pass filtered to cut down noise, but that's not very important
right now as the plot is done in integers anyway.  Later, when UDP control and comm is added, and the web server removed, 
which I suppose should be another project here - it will return floating point data when requested by the master.

This sketch also implements my LANDNS, or half of it - the part that broadcasts the machine name on the LAN along with
its IP address, for other machines to listen to and build up their /etc/hosts file with using code I call "tellme". This
solves endless problems with balky lan-internal naming issues, the inability to get a hostname to really change so it's uniform
on all possible naming interfaces and so on - /etc/hosts is always checked first, so making it correct solves all that cruft.

More info, as always, on my forums, at www.coultersmithing.com/forums  You can think of this as a simpler grab and go
redundant backup of what might be found there in various versions, but there is where all the real explanations
and design justification stuff can be found.
