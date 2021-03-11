# esp8266 wifi eprom configurator

This repo provides the barebone implementation for a new iot device.

When the esp8266 will run this code it will try to join the wifi stored in the eprom.
If no wifi is stored or the connection fails it will start a new wifi AP with a captive portal where you can configure the wifi network.