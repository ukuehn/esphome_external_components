# DS2438 component for ESPHome

## About

This external component provides support for the Dallas DS2438 smart
battery monitor device. It communicates via 1-wire bus and allows to
measure volatages (external or supply) resp. voltage over a sense
resistor for current measurements, thus a 1-wire bus is required to be
set up in the configuration. For details on the device see the [data
sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS2438.pdf).

## Configuration

The DS2438 requires a [1-wire bus](https://esphome.io/components/one_wire/)
for communication. The example uses one based on a GPIO.


```yaml
# Example configuration entry

external_components:
  - source:
      type: local
      path: ../components/.
    components: ds2438

one_wire:
  - platform: gpio
    pin: GPIO2
    id: one_wire_bus

sensor:
  - platform: ds2438
    one_wire_id: one_wire_bus
    address: 0xce0000022ffc4b26
    id: my_ds2438
    temperature:
      name: "internal temperature"
      id: "temp"
    vad:
      name: "external voltage"
      id: "vad"
    vdd:
      name: "supply voltage"
      id: "vdd"
    vsense:
      name: "current sense voltage"
      id: "vsense"

```

## Configuration variables

- **one_wire_id** (*Optional*): Specify the id of the 1-wire bus the device is connected to. If only a single 1-wire bus is present in the system this can be left out.

- **address** (*Optional*): The unique device ID, as shown by the 1-wire bus component on start-up (so far only tested with specifying the address).

- **id** (*Optional*): Specify the ID of the sensor for code generation.

- **temperature** (*Optional*): Specify to use the built-in temperature sensor of the DS2438. Caveat: due to self-heating this temperature probe is somewhat less accurate than a separate temperature sensor, e.g. a DS18B20 (using the dallas_temp component). Output is the temperature in degree Celsius (°C).

- **vad** (*Optional*): Specify the use of the voltage ADC to measure the externally supplied voltage. Output is the voltage in Volt (V).

- **vdd** (*Optional*): Specify the use of the voltage ADC to measure the chip's supply voltage. Output is the voltage in Volt (V). In an ESP32-based environment expect about 3.32 V

- **vsense** (*Optional*): Specify the use of the current sensing ADC to measure the vaoltage difference of the vsense+ and vsense- pins. Output is the voltage in milli-Volt (mV).

- All other options from [Sensor](https://esphome.io/components/sensor/).





## Elaborated example

The following configuration example shows how to measure humidity
using a DS2438 and a HIH5030 sensor connected to the Vad pin. For
correct humidity calculation the supply voltage and the temperature
must be considered as well.

Since the ds2438 component measures first vad and then vdd if both are
configured (like here), having a vdd value indicates that we have all
values collected and updated to calculate the humidity.


```yaml

external_components:
  - source:
      type: local
      path: ../components/.
    components: ds2438

one_wire:
  - platform: gpio
    pin: GPIO2
    id: one_wire_bus

sensor:
  - platform: ds2438
    # DS2438 with HIH5030 humidity sensor on Vad
    # uses built-in Vdd and temperature sensors for correct
    # humidity calculation and updates humidity "sensor"
    # with the result
    one_wire_id: one_wire_bus
    address: 0xce0000022ffc4b26
    id: humsensor
    temperature:
      name: "hum sensor temp"
      id: "ce_temp"
    vad:
      name: "hum sensor vad"
      id: "ce_vad"
    vdd:
      name: "hum sensor vdd"
      id: "ce_vdd"
      on_value:
        then:
          - lambda: |-
              float vad = id(ce_vad).state;
              float vdd = id(ce_vdd).state;
              float temp = id(ce_temp).state;
              float hum = ((vad/vdd)-0.1515)/0.00636;
              float humcomp = hum/(1.0546-0.00216*temp);
              id(hum).publish_state(humcomp);
              ESP_LOGD("ds2438", "humidity >> %.0f", humcomp);

  - platform: template
    # This sensor is being updated from a lambda expression of
    # a ds2438 sensor as an example how to handle more complex
    # sensor fusion setups
    name: "humidity"
    id: hum
    accuracy_decimals: 0
    update_interval: never
    state_topic: "sensor/humidity"


```


