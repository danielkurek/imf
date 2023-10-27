# Serial communication command structure

In serial communication you can either write to some field or get its value. Alternatively you can ask for the status of the server device.

## Commands

There are 3 command types:

- `GET <field>`
- `PUT <field> <value>`
- `STATUS`

Each command must end with null byte `\0`.

Currently to keep things simple there is no support for escaping. So you cannot have field or its value with spaces or `\0` char.