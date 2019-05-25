# HTTPS POST Request Example

## Overview

- ESP32(ESP-IDF) example
- Connect HTTPS and POST JSON data
- Using HTTP Basic Authentication
- Validate certificate with x509 root certificate
- Measure ESP32 chip temperature
- Blink onboard LED
- Use WiFi

## Description

This code is an example of HTTPS communication with ESP32. Send the ESP32 chip temperature to the URL you specify.

## Demo

Running this sample, ESP32 will output the following serially. HTTP Response is different from the following because it depends on the API of connection destination.

~~~
I (309381) HTTPS: Connected to AP
I (310381) HTTPS: Connection established...
I (310391) HTTPS: request_body={"esp32":{"temperature":56.666667}}
I (310391) HTTPS: 241 bytes written
I (310391) HTTPS: Reading HTTP response...
HTTP/1.1 200 OK
Server: nginx
Date: Sat, 25 May 2019 14:56:51 GMT
Content-Type: text/html; charset=UTF-8
Connection: close

{"esp32": {"temperature": 56.666667}}
I (310581) HTTPS: connection closed
---

## Requirement

To build this code, you need an ESP-IDF development environment. See ESPRESSIF web site.[Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)

## Usage

Around available WiFi, turn on your ESP32 board.

## Install

1. Type `make menuconfig` command
1. Enter "Connection Configuration" menu
1. Configurate
1. Select "Save" and "Exit"
1. Type `make -j4 flash` command and write to your ESP32 device
1. Type `make monitor` command

## Licence

[MIT](https://github.com/u-masao/esp32-https-post-basicauth/blob/master/LICENSE)

## Author

[u-masao](https://github.com/u-masao)
