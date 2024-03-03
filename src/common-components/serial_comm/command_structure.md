# Serial communication command structure

In serial communication you can either write to some field or get its value. Alternatively you can ask for the status of the server device.

## Commands

There are 3 command types:

- `GET (<addr>:)<field>`
- `PUT (<addr>:)<field> <value>`
- `STATUS`

Each command must end with new line (`\n`).

Currently to keep things simple there is no support for escaping. So you cannot have field or its value with spaces, `\n` char or null byte `\0`.

- `(<addr>:)` is an optional parameter `uint16_t` type encoded as hex string with leading zeros (4 characters)
- `<field>` is a string, cannot contain `:`
- `<value>` is a string

## Response

Response is asynchronous from commands. It is just a channel to notify the other device of value change for given field.

Response structure:
```
(<addr>:)<field>=<value>
```

`(<addr>:)`, `<field>`, `<value>` have the same meaning as in [command section](#commands).


