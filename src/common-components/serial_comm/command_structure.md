# Serial communication command structure

In serial communication you can either write to some field or get its value. Alternatively you can ask for the status of the server device.

## Commands

There are 3 command types:

- `GET (<addr>:)<field>`
- `PUT (<addr>:)<field> <value>`
- `STATUS`

Each command must end with null byte `\0`.

Currently to keep things simple there is no support for escaping. So you cannot have field or its value with spaces or `\0` char.

- `(<addr>:)` is an optional parameter `uint16_t` type encoded as hex string with leading zeros (4 characters)
- `<field>` is a string, cannot contain `:`
- `<value>` is a string