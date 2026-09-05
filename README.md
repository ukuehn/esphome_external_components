# esphome_external_components

My collection (so far one) of external components for ESPHome

ESPHome has a built-in mechanism to use a local copy, or directly from
github: [external
components](https://esphome.io/components/external_components.html).

Example:
```yaml
# use a local copy
external_components:
  - source:
      type: local
      path: ../components/.
    components: [ ds2438 ]

# use github
external_components:
  - source:
      type: git
      url: https://github.com/ukuehn/esphome_external_components
    components: [ ds2438 ]
```

A more detailed description for each components can be found in the
respective folders of the components.
