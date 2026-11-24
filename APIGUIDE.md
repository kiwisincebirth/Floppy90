# File Services API Guide

The primary purposes of Floppy90 is the provision of a File services API
compatible with the FreHD, but an extension is provided to allow for multiple
files to be open at once.

## Introduction

### Registers Ports

| IO Address | Register              |                                        |    |
|------------|-----------------------|----------------------------------------|----|
| $C2        | Pico DATA IO Port     | Data passed to and from API            | RW |
| $C3        | Pico SIZE Register    | Size of DATA Bytes being passed        | RW |
| $C4        | Pico COMMAND Register | The command to execute                 | W  |
| $C5        | Pico ERROR Register   | The Last Error code if command         | R  |
| $C6        | Pico FILEID Register  | When accessing an open File its FileID | RW |
| $CF        | Pico STATUS Register  | Status of the Last command             | R  |

### Interaction

## API Summary

The first Command are The FreHD commands that are supported

| Cmd ID | Name                 | Description |
|--------|----------------------|-------------|
| $00    | TRS_EXTRA_VERSION    |             |
| $01    | TRS_EXTRA_GETTIME    | unsupported |
| $02    | TRS_EXTRA_SETTIME    | unsupported |
| $03    | TRS_EXTRA_OPENFILE   |             |
| $04    | TRS_EXTRA_READFILE   |             |
| $05    | TRS_EXTRA_WRITEFILE  |             |
| $06    | TRS_EXTRA_CLOSEFILE  |             |
| $07    | TRS_EXTRA_BOOTLOADER | unsupported |
| $08    | TRS_EXTRA_OPENDIR    |             |
| $09    | TRS_EXTRA_READDIR    |             |

Then we have some general purpose API commands that are supported.

| Cmd ID | Name              | Description |
|--------|-------------------|-------------|
| $11    | TRS_NEWER_DIR     |             |

Next is basic File handling commands that deal with File control

| Cmd ID | Name           | Description |
|--------|----------------|-------------|
| $14    | TRS_FILE_OPEN  |             |
| $15    | TRS_FILE_CLOSE |             |
| $16    | TRS_FILE_KILL  |             |

Then we have some Directory handling commands that deal with Directory control

| Cmd ID | Name                | Description                                  |
|--------|---------------------|----------------------------------------------|
| $18    | TRS_NEWER_ASSIGN    | assign a virtual drive Number to a directory |
| $19    | TRS_NEWER_CHANGEDIR | Change the current working directory         |
| $1A    | TRS_NEWER_COPYFILE  | Copy a file or Files                         |
| $1B    | TRS_NEWER_DELETE    | Delete a file of Files, or directory         |
| $1C    | TRS_NEWER_MAKEDIR   | Make a new directory                         |
| $1D    | TRS_NEWER_RENAME    | Rename a File                                |
| $1E    | TRS_NEWER_PWD       | Get current Directory                        |

Then we have some File Data Access commands that deal with read and writing to files

| Cmd ID | Name             | Description                                  |
|--------|------------------|----------------------------------------------|
| $20    | TRS_FILE_READ    | Read Bytes from a File                       |
| $21    | TRS_FILE_WRITE   | Write Bytes to a File                        |
| $22    | TRS_FILE_SOF     | Position                                     |
| $23    | TRS_FILE_POS     | Position                                     |
| $24    | TRS_FILE_BACK    | Position                                     |
| $25    | TRS_FILE_EOF     | Position                                     |
| $26    | TRS_FILE_FCB     | Get current File Position data in FCB format |

## Version API

## Simple File Access

Used by

## File Control (Open Close)

## File Data Access (Read Write)

## Directory Management

## Directory Listing.

## Reference

### IO Address Map



### Status Codes


## Error Codes

| Error Number | Error Message           | TRS-DOS |
|--------------|-------------------------|---------|
| 0            | No Error                | Yes     |
| 15           | Write Protected Disk    | Yes     |
| 16           | Illegal File Number     | Yes     |
| 19           | Illegal Filename        | Yes     |
| 24           | File Not Found          | Yes     |
| 28           | End of File Encountered | Yes     |
| 31           | Program Not Found       | Yes     |
| 32           | Illegal Drive Number    | Yes     |
| 34           | Load File Format Error  | Yes     |
| 57           | Denied, Already Open    |         |
| 58           | Denied, Not Empty       |         |
| 59           | File Exists             |         |
| 60           | Path Not Found          |         |
| 61           | Illegal DOS Function    |         |
| 62           | Internal DOS Error      |         |
| 63           | Unknown Error           |         |
