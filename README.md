# What is esp32-wifi-manager?
*esp32-wifi-manager* is an esp32 program that enables easy management of wifi networks through a web application.

*esp32-wifi-manager* is **lightweight** (8KB of task stack in total) and barely uses any CPU power through a completely event driven architecture. It's an all in one wifi scanner, http server & dns daemon living in the least amount of RAM possible.

For real time constrained applications, *esp32-wifi-manager* can live entirely on PRO CPU, leaving the entire APP CPU untouched for your own needs.

*esp32-wifi-manager* will automatically attempt to re-connect to a previously saved network on boot, and it will start its own wifi access point through which you can manage wifi networks if a saved network cannot be found and/or if the connection is lost.

*esp32-wifi-manager* is an esp-idf project that compiles successfully with the esp-idf 3.2 release. You can simply copy the project and start adding your own code to it.

# Demo
[![esp32-wifi-manager demo](http://img.youtube.com/vi/hxlZi15bym4/0.jpg)](http://www.youtube.com/watch?v=hxlZi15bym4)

# Look and Feel
![esp32-wifi-manager on an mobile device](https://idyl.io/wp-content/uploads/2017/11/esp32-wifi-manager-password.png "esp32-wifi-manager") ![esp32-wifi-manager on an mobile device](https://idyl.io/wp-content/uploads/2017/11/esp32-wifi-manager-connected-to.png "esp32-wifi-manager")

# License
*esp32-wifi-manager* is MIT licensed. As such, it can be included in any project. Please make sure to read the license file.
